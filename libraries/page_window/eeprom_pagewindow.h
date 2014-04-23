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
 * Filename: eeprom_pagewindow.h
 *
 * Purpose: This file contains typedefs and function prototypes for the file eeprom_pagewindow.c.
 *
 * Design Notes:
 *
 * References:
 *
 */

#ifndef _eeprom_pagewindow_
#define _eeprom_pagewindow_

/*
 * Includes
 */

#include "common_types.h"

/*
 * Macro Definitions
 */

#define EEPROM_BANK1                1
#define EEPROM_BANK2                2

#define EEPROM_PAGE_WINDOW_SIZE     1024 /* should divide evenly with eeprom size and matches the hardware page window size */
#define EEPROM_PAGE_WINDOW_MASK     (~((uint32)(EEPROM_PAGE_WINDOW_SIZE - 1)))

#define EEPROM_SIZE                 0x400000
#define EEPROM_START_ADDR           0x03400000
#define EEPROM_END_ADDR             EEPROM_START_ADDR + EEPROM_SIZE - 1

#define EEPROM_BANK1_SIZE           0x200000
#define EEPROM_BANK1_START_ADDR     0x03400000
#define EEPROM_BANK1_END_ADDR       EEPROM_BANK1_START_ADDR + EEPROM_BANK1_SIZE - 1

#define EEPROM_BANK2_SIZE           0x200000
#define EEPROM_BANK2_START_ADDR     0x03600000
#define EEPROM_BANK2_END_ADDR       EEPROM_BANK2_START_ADDR + EEPROM_BANK2_SIZE - 1

/* Error Codes */
#define EEPROM_SUCCESS              0
#define EEPROM_ERROR               -1
#define EEPROM_WRITE_PROTECTED     -2
#define EEPROM_INVALID_ADDRESS     -3
#define EEPROM_SEM_ERROR           -4

/*
 * Type Definitions
 */

typedef struct {
    uint32          Loaded;
    uint32          LowerAddress;
    uint32          UpperAddress;
    uint32          BufferSize;
    uint8           Buffer[EEPROM_PAGE_WINDOW_SIZE];
} EEPROM_PageWindow_t;

/*
 * Exported Functions
 */

int32 EEPROM_PageWindowInit(void);
int32 EEPROM_PageWindowWrite(void *Dest, void *Src, uint32 Size);
void  EEPROM_PageWindowFlush(void);
uint8 EEPROM_IsValidAddressRange(uint32 Address, uint32 Size);
uint8 EEPROM_IsWriteProtected(uint32 Address);

#endif

/************************/
/*  End of File Comment */
/************************/
