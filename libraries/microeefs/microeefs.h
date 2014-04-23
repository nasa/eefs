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
 * Filename: microeefs.h
 *
 * Purpose: This file contains a function that reads a EEFS File System and returns a pointer to the specified
 *    file in the file system.  This code is intended to be used in bootstrap code when booting the system from
 *    a file in the EEFS.
 *
 * Design Notes:
 *
 * References:
 *
 */

#ifndef _microeefs_
#define _microeefs_

/*
 * Includes
 */

#include "common_types.h"

/*
 * Exported Functions
 */

/* Return a pointer to the File Header of the specified file in the specified EEFS or NULL if an error occurs. */
void *MicroEEFS_FindFile(uint32 BaseAddress, char *Filename);

#endif

/************************/
/*  End of File Comment */
/************************/

