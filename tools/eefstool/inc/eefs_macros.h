
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
 * System Dependent Lower Level Functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>


extern void eefstool_flush_device(void);
extern void eefstool_lock(void);
extern void eefstool_unlock(void);
extern void *eefstool_copy_from_device(void *dest, const void *src, size_t n);
extern void *eefstool_copy_to_device(void *dest, const void *src, size_t n);

#define EEFS_LIB_EEPROM_WRITE(Dest, Src, Length) eefstool_copy_to_device(Dest, Src, Length) 
#define EEFS_LIB_EEPROM_READ(Dest, Src, Length)  eefstool_copy_from_device(Dest, Src, Length) 
#define EEFS_LIB_EEPROM_FLUSH                    eefstool_flush_device()
#define EEFS_LIB_LOCK                            eefstool_lock()
#define EEFS_LIB_UNLOCK eefstool_unlock()

/*
**  This macro defines the time interface function.  Defaults to time(NULL) 
*/
#define EEFS_LIB_TIME                           time(NULL)

/* This macro defines the file system write protection interface function.  If the file system
   is read-only then set this macro to TRUE.  If the file system is always write enabled then
   set this macro to FALSE.  If the eeprom has an external write protection interface then a custom
   function can be called to determine the write protect status. */
#define EEFS_LIB_IS_WRITE_PROTECTED              FALSE



