/*
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
**
*/

/*
** This file is an example of how to setup and initialize the EEPROM File System
** in RTEMS
**
** It is not intended to be part of the EEFS library, but instead should be incorporated
** into an application that uses the EEFS.
**
**  
*/

#error "This C file is an example, not intended to be compiled with the EEFS"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <rtems.h>
#include <rtems/libio.h>
#include <rtems/imfs.h>

#include "common_types.h"

/*
** External functions/drivers
*/
int rtems_eefs_initialize(rtems_filesystem_mount_table_entry_t *temp_mt_entry,
                          const void *data);


/*
** Global variables used for the EEFS bank addresses
** These are required by the EEFS RTEMS driver.
** The defines are set in the RKI config file.
*/


uint32 rtems_eefs_a_address;
uint32 rtems_eefs_b_address;

/*
**  Register the EEFS filesystem and assign the 
**  addresses for the EEFS banks. 
*/
int setup_eefs(void)
{
   int   Status;

   /*
   ** Assign the EEFS addresses
   ** These addresses should be the actual addresses of your EEFS images
   ** You can use variables instead of absolute addresses ( or variables 
   ** in a linker script )
   */
   rtems_eefs_a_address = 0x12345678;
   rtems_eefs_b_address = 0x0;

   /*
   ** Register the EEFS file system with RTEMS
   ** After this is done, then the volumes can be mounted 
   ** using a C call or a shell "mount" command
   */
   Status = rtems_filesystem_register("eefs", &rtems_eefs_initialize );

   if ( Status == 0 )
   {
      /*
      ** Mount a volume
      */
      Status = mount ( "/dev/eefsa", "/eefs", "eefs", RTEMS_FILESYSTEM_READ_WRITE, NULL);

      if( Status == 0 )
      {
         printf("setup_eefs: EEFS file system successfully mounted at /eefs \n");
      } 
      else 
      {
         printf("setup_eefs: EEFS mount failed\n");
      }
   }
   else
   {
      printf("setup_eefs: Failed to register the EEFS file system with RTEMS\n");
   }
   return Status;
}
