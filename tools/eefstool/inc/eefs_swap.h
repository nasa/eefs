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
 * Filename: eefs_swap.h
 *
 * Purpose: 
 *
 */

#ifndef _EEFS_SWAP_H_
#define _EEFS_SWAP_H_

/*
 *  Function Prototypes
 */

void    EEFS_SwapFileHeader(EEFS_FileHeader_t *FileHeader);
void    EEFS_SwapFileAllocationTable(EEFS_FileAllocationTable_t *FileAllocationTable);
void    EEFS_SwapFileAllocationTableEntry(EEFS_FileAllocationTableEntry_t *FileAllocationTableEntry); 
void    EEFS_SwapFileAllocationTableHeader(EEFS_FileAllocationTableHeader_t *FileAllocationTableHeader); 

#endif
