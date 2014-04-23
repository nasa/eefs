
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
 * Filename: eefs_swap.c
 *
 * Purpose: 
 *
 */

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

/*
 * Local Function Prototypes
 */

void     SwapUInt16(uint16 *ValueToSwap);
void     SwapUInt32(uint32 *ValueToSwap);
uint32   ThisMachineDataEncoding(void);

/*
 * Function Definitions
 */

/* 
** This function byte swaps a 16 bit integer 
*/
void SwapUInt16(uint16 *ValueToSwap)
{
    uint8 *BytePtr = (uint8 *)ValueToSwap;
    uint8  TempByte = BytePtr[1];
    BytePtr[1] = BytePtr[0];
    BytePtr[0] = TempByte;
}

/* 
** This function byte swaps a 32 bit integer 
*/
void SwapUInt32(uint32 *ValueToSwap)
{
    uint8 *BytePtr = (uint8 *)ValueToSwap;
    uint8  TempByte = BytePtr[3];
    BytePtr[3] = BytePtr[0];
    BytePtr[0] = TempByte;
    TempByte   = BytePtr[2];
    BytePtr[2] = BytePtr[1];
    BytePtr[1] = TempByte;
}

/* 
** This function examines the byte ordering of a 32 bit word to determine
** if this machine is a Big Endian or Little Endian machine 
*/
uint32 ThisMachineDataEncoding(void)
{
    uint32   DataEncodingCheck = 0x01020304;

    if (((uint8 *)&DataEncodingCheck)[0] == 0x04 &&
        ((uint8 *)&DataEncodingCheck)[1] == 0x03 &&
        ((uint8 *)&DataEncodingCheck)[2] == 0x02 &&
        ((uint8 *)&DataEncodingCheck)[3] == 0x01) {
        return(LITTLE_ENDIAN);
    }
    else {
        return(BIG_ENDIAN);
    }
}

/*
** This function swaps a file header
*/
void EEFS_SwapFileHeader( EEFS_FileHeader_t  *FileHeader)
{
    SwapUInt32(&FileHeader->Crc);
    SwapUInt32(&FileHeader->InUse);
    SwapUInt32(&FileHeader->Attributes);
    SwapUInt32(&FileHeader->FileSize);
    SwapUInt32((uint32 *)&FileHeader->ModificationDate);
    SwapUInt32((uint32 *)&FileHeader->CreationDate);
}

/*
** This function swaps a file allocation table
*/

void EEFS_SwapFileAllocationTable(EEFS_FileAllocationTable_t *FileAllocationTable)
{
    uint32                           i;

    SwapUInt32(&FileAllocationTable->Header.Crc);
    SwapUInt32(&FileAllocationTable->Header.Magic);
    SwapUInt32(&FileAllocationTable->Header.Version);
    SwapUInt32(&FileAllocationTable->Header.FreeMemoryOffset);
    SwapUInt32(&FileAllocationTable->Header.FreeMemorySize);
    SwapUInt32(&FileAllocationTable->Header.NumberOfFiles);

    for (i=0; i < FileAllocationTable->Header.NumberOfFiles; i++)
    {
        SwapUInt32(&FileAllocationTable->File[i].FileHeaderOffset);
        SwapUInt32(&FileAllocationTable->File[i].MaxFileSize);
    }
}


void    EEFS_SwapFileAllocationTableEntry(EEFS_FileAllocationTableEntry_t *FileAllocationTableEntry)
{
    SwapUInt32(&FileAllocationTableEntry->FileHeaderOffset);
    SwapUInt32(&FileAllocationTableEntry->MaxFileSize);
}

 
void    EEFS_SwapFileAllocationTableHeader(EEFS_FileAllocationTableHeader_t *FileAllocationTableHeader)
{

    SwapUInt32(&FileAllocationTableHeader->Crc);
    SwapUInt32(&FileAllocationTableHeader->Magic);
    SwapUInt32(&FileAllocationTableHeader->Version);
    SwapUInt32(&FileAllocationTableHeader->FreeMemoryOffset);
    SwapUInt32(&FileAllocationTableHeader->FreeMemorySize);
    SwapUInt32(&FileAllocationTableHeader->NumberOfFiles);

}

