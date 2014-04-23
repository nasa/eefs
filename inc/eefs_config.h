/*
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
**
** Filename: eefs_config.h
**
** Purpose: This file contains system dependent macros for the eefs.
**
** Design Notes:
**
** References:
**
*/

#ifndef _eefs_config_
#define _eefs_config_

/*
 * Macro Definitions
 */

/* Maximum number of files in the file system */
#define EEFS_MAX_FILES                      64

/* Maximum number of file descriptors */
#define EEFS_MAX_OPEN_FILES                 20

/* Default number of spare bytes added to the end of a slot when a 
   new file is created by calling the EEFS_LibCreat function */
#define EEFS_DEFAULT_CREAT_SPARE_BYTES      512

#endif 

/************************/
/*  End of File Comment */
/************************/

