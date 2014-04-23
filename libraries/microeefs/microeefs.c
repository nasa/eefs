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
 * Filename: microeefs.c
 *
 * Purpose: This file contains a function that reads a EEFS File System and returns a pointer to the specified
 *    file in the file system.  This code is intended to be used in bootstrap code when booting the system from
 *    a file in the EEFS.
 *
 */

/*
 * Includes
 */

#include "eefs_fileapi.h"
#include "eefs_macros.h"
#include <string.h>

/*
 * Function Definitions
 */

/* Return a pointer to the File Header of the specified file in the specified EEFS or NULL if an error occurs. */
void *MicroEEFS_FindFile(uint32 BaseAddress, char *Filename)
{

    EEFS_FileAllocationTableHeader_t    FileAllocationTableHeader;
    EEFS_FileAllocationTableEntry_t     FileAllocationTableEntry;
    EEFS_FileAllocationTableEntry_t    *FileAllocationTableEntry_ptr;
    EEFS_FileHeader_t                   FileHeader;
    uint32                              i;

    if (Filename != NULL) {

        EEFS_LIB_EEPROM_READ(&FileAllocationTableHeader, (void *) BaseAddress, sizeof(EEFS_FileAllocationTableHeader_t));
        if ((FileAllocationTableHeader.Magic == EEFS_FILESYS_MAGIC) &&
            (FileAllocationTableHeader.Version == 1)) {

            /* I use a pointer to search the FAT for the specified file.  This is done to minimize the amount of ram
             * this code requires and to be independent of the EEFS_MAX_FILES parameter.  So if this code is used in the 
             * bootstrap then the EEFS_MAX_FILES can change without the need update the code and reburn the PROM. */
            FileAllocationTableEntry_ptr = (void *)(BaseAddress + sizeof(EEFS_FileAllocationTableHeader_t));
            for (i=0; i < FileAllocationTableHeader.NumberOfFiles; i++) {
                EEFS_LIB_EEPROM_READ(&FileAllocationTableEntry, FileAllocationTableEntry_ptr, sizeof(EEFS_FileAllocationTableEntry_t));
                EEFS_LIB_EEPROM_READ(&FileHeader, (void *)(BaseAddress + FileAllocationTableEntry.FileHeaderOffset), sizeof(EEFS_FileHeader_t));
                if ((FileHeader.InUse == TRUE) &&
                    (strncmp(Filename, FileHeader.Filename, EEFS_MAX_FILENAME_SIZE) == 0)) {
                    return((void *)(BaseAddress + FileAllocationTableEntry.FileHeaderOffset));
                }
                FileAllocationTableEntry_ptr++;
            }

            /* file not found */
            return(NULL);
        }
        else { /* invalid file allocation table */
            return(NULL);
        }
    }
    else { /* invalid filename */
        return(NULL);
    }
}

/************************/
/*  End of File Comment */
/************************/
