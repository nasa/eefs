
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
 * Filename: geneepromfs.h
 *
 * Purpose: This file contains typedefs and function prototypes for the file geneepromfs.c.
 *
 * Design Notes:
 *
 * References:
 *
 */

#ifndef _geneepromfs_
#define	_geneepromfs_

/*
 * Macro Definitions
 */

#define             VERSION_NUMBER                  1.0
#define             MAX_FILENAME_SIZE               64

#ifndef LITTLE_ENDIAN
#define             LITTLE_ENDIAN                   0
#endif
#ifndef BIG_ENDIAN
#define             BIG_ENDIAN                      1
#endif

/*
 * Exported Functions
 */

/* Print an error message to the console and exit */
void                UglyExit(char *Spec, ...);

#endif

