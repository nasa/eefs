/*
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
*/

/*
 * Filename: eeprom_pagewindow.c
 *
 * Purpose: This file contains the interface routines that implement a simple page write interface to the EEPROM.
 *
 */

/*
 * Includes
 */

#include "eeprom_pagewindow.h"
#include "common_types.h"
#include "semLib.h"
#include <stdio.h>
#include <string.h>

/*
 * Local Data
 */

SEM_ID                  EEPROM_semId;
EEPROM_PageWindow_t     EEPROM_PageWindow;

/*
 * Global Data
 */

extern uint32           GSFC_EepromWriteEnableFlags;

/*
 * Local Function Prototypes
 */

void  EEPROM_PageWindowWriteByte(uint32 MemoryAddress, uint8 ByteValue);
void  EEPROM_PageWindowLoad(uint32 MemoryAddress);

/*
 * External Function Prototypes
 */

/* These files are from the file LRO_System_Services.c and are used for performing low level EEPROM writes.  These
 * prototypes are included here to avoid including the file LRO_System_Services.h which would make unit testing
 * difficult due to the long list of additional vxWorks include files that would also be included. */
int     LRO_Write_EEPROM(void *from_addr, unsigned long int eeprom_offset, unsigned int num_bytes);
int     LRO_Read_EEPROM(void *to_addr, unsigned long int eeprom_offset, unsigned int num_bytes);

/*
 * Function Definitions
 */

/* Initialize the data structures. */
int32 EEPROM_PageWindowInit(void)
{
    int32  ReturnStatus = EEPROM_SUCCESS;

    memset(&EEPROM_PageWindow, 0, sizeof(EEPROM_PageWindow_t));

    if ((EEPROM_semId = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE)) == NULL) {
        ReturnStatus = EEPROM_SEM_ERROR;
    }

    return(ReturnStatus);
    
} /* End of EEPROM_PageWindowInit() */

/* High level api function to write data to the page window. */
int32 EEPROM_PageWindowWrite(void *Dest, void *Src, uint32 Size)
{
    uint32      i;
    int32       ReturnStatus;

    if ((Src != NULL) && (Dest != NULL)) {
        
        if (EEPROM_IsValidAddressRange((uint32)Dest, Size) == TRUE) {
            
            if (EEPROM_IsWriteProtected((uint32)Dest) == FALSE) {

                semTake(EEPROM_semId, WAIT_FOREVER);
                for (i=0; i < Size; i++) {
                    EEPROM_PageWindowWriteByte((uint32)(Dest + i), *((uint8 *)(Src + i)));
                }
                semGive(EEPROM_semId);

                ReturnStatus = EEPROM_SUCCESS;
            }
            else {
                ReturnStatus = EEPROM_WRITE_PROTECTED;
            }
        }
        else {
            ReturnStatus = EEPROM_INVALID_ADDRESS;
        }
    }
    else {
        ReturnStatus = EEPROM_INVALID_ADDRESS;
    }
    
    return(ReturnStatus);
    
} /* End of EEPROM_PageWindowWrite() */

/* Write a byte into the page window buffer */
void EEPROM_PageWindowWriteByte(uint32 MemoryAddress, uint8 ByteValue)
{   
    if (EEPROM_PageWindow.Loaded == FALSE) {
        EEPROM_PageWindowLoad(MemoryAddress);
    }
    else if ((MemoryAddress < EEPROM_PageWindow.LowerAddress) || 
             (MemoryAddress > EEPROM_PageWindow.UpperAddress)) {
        EEPROM_PageWindowFlush();
        EEPROM_PageWindowLoad(MemoryAddress);
    }

    EEPROM_PageWindow.Buffer[MemoryAddress - EEPROM_PageWindow.LowerAddress] = ByteValue;
    
} /* End of EEPROM_PageWindowWriteByte() */

/* Write data from the page window buffer into eeprom */
void EEPROM_PageWindowFlush(void)
{
/*    uint32      i; */
/*    uint8       ReadBackBuffer[EEPROM_PAGE_WINDOW_SIZE]; */

    if (EEPROM_PageWindow.Loaded == TRUE) {

        semTake(EEPROM_semId, WAIT_FOREVER);
        LRO_Write_EEPROM(&EEPROM_PageWindow.Buffer, (EEPROM_PageWindow.LowerAddress - EEPROM_START_ADDR), EEPROM_PageWindow.BufferSize);

        /* read back verify - used for debugging */
/*        LRO_Read_EEPROM(&ReadBackBuffer, (EEPROM_PageWindow.LowerAddress - EEPROM_START_ADDR), EEPROM_PageWindow.BufferSize); */
/*        for (i=0; i < EEPROM_PageWindow.BufferSize; i++) { */
/*            if (EEPROM_PageWindow.Buffer[i] != ReadBackBuffer[i]) { */
/*                printf("EEPROM VERIFY ERROR: Addr = 0x%lx\n", (uint32)(EEPROM_PageWindow.LowerAddress + i)); */
/*                break; */
/*            } */
/*        } */

        EEPROM_PageWindow.Loaded = FALSE;
        semGive(EEPROM_semId);
    }
    
} /* End of EEPROM_PageWindowFlush() */

/* Copy data from eeprom into the page window buffer */
void EEPROM_PageWindowLoad(uint32 MemoryAddress)
{
    EEPROM_PageWindow.Loaded = TRUE;
   
    EEPROM_PageWindow.LowerAddress = (MemoryAddress & EEPROM_PAGE_WINDOW_MASK);
    if (EEPROM_PageWindow.LowerAddress < EEPROM_START_ADDR) {
        EEPROM_PageWindow.LowerAddress = EEPROM_START_ADDR;
    }
    
    EEPROM_PageWindow.UpperAddress = (EEPROM_PageWindow.LowerAddress + EEPROM_PAGE_WINDOW_SIZE - 1);
    if (EEPROM_PageWindow.UpperAddress > EEPROM_END_ADDR) {
        EEPROM_PageWindow.UpperAddress = EEPROM_END_ADDR;
    }

    EEPROM_PageWindow.BufferSize = (EEPROM_PageWindow.UpperAddress - EEPROM_PageWindow.LowerAddress + 1);

    LRO_Read_EEPROM(&EEPROM_PageWindow.Buffer, (EEPROM_PageWindow.LowerAddress - EEPROM_START_ADDR), EEPROM_PageWindow.BufferSize);
    
} /* End of EEPROM_PageWindowLoad() */

/* Make sure the address range is in eeprom and does not span banks */
uint8 EEPROM_IsValidAddressRange(uint32 Address, uint32 Size)
{
    uint8       ReturnStatus;

    if ((Address >= EEPROM_BANK1_START_ADDR) &&
        (Address <= EEPROM_BANK1_END_ADDR)) {

        if ((Address + Size - 1) <= EEPROM_BANK1_END_ADDR) {
            ReturnStatus = TRUE;
        }
        else {
            ReturnStatus = FALSE;
        }
    }
    else if ((Address >= EEPROM_BANK2_START_ADDR) &&
             (Address <= EEPROM_BANK2_END_ADDR)) {

        if ((Address + Size - 1) <= EEPROM_BANK2_END_ADDR) {
            ReturnStatus = TRUE;
        }
        else {
            ReturnStatus = FALSE;
        }
    }
    else {
        ReturnStatus = FALSE;
    }

    return(ReturnStatus);
    
} /* End of EEPROM_IsValidAddressRange() */

/* Check to see if the eeprom bank is write protected */
uint8 EEPROM_IsWriteProtected(uint32 Address)
{
    uint8       ReturnStatus;

    if ((Address >= EEPROM_BANK1_START_ADDR) &&
        (Address <= EEPROM_BANK1_END_ADDR)) {

        if ((GSFC_EepromWriteEnableFlags & EEPROM_BANK1) != 0) {
            ReturnStatus = FALSE;
        }
        else {
            ReturnStatus = TRUE;
        }
    }
    else if ((Address >= EEPROM_BANK2_START_ADDR) &&
             (Address <= EEPROM_BANK2_END_ADDR)) {

        if ((GSFC_EepromWriteEnableFlags & EEPROM_BANK2) != 0) {
            ReturnStatus = FALSE;
        }
        else {
            ReturnStatus = TRUE;
        }
    }
    else {
        ReturnStatus = TRUE;
    }

    return(ReturnStatus);
    
} /* End of EEPROM_IsWriteProtected() */

/************************/
/*  End of File Comment */
/************************/
