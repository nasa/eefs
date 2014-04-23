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
** File:
**  $Id: eefstool_driver_img.c 1.2 2011/08/09 11:42:57GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2011/08/09 11:42:57GMT-05:00 $
** $Revision: 1.2 $
** $Log: eefstool_driver_img.c  $
** Revision 1.2 2011/08/09 11:42:57GMT-05:00 acudmore 
** EEFS 2.0 changes for eefstool
** Revision 1.2 2010/05/13 09:41:08EDT acudmore 
** Update EEFS tool
** Revision 1.1 2010/05/06 15:09:04EDT acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eefs/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/16 10:41:38EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eeprom-filesystem/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/10 14:37:52EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/src/project.pj
** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>


#include "common_types.h"
#include "eefs_filesys.h"
#include "eefstool_opts.h"

/*
** Global variables
*/
extern CommandLineOptions_t CommandLineOptions;

int    imageFileFd;
char  *eefsImageDataPtr;
int    eefsFileSize;

/*
** in eefstool_copy_from_device
**
*/
void *eefstool_copy_from_device(void *dest, const void *src, size_t n)
{
  memcpy(dest, src, n);
  return(dest);
}

/*
** In eefstool_copy_to_device
*/
void *eefstool_copy_to_device(void *dest, const void *src, size_t n)
{
  memcpy(dest, src, n);
  return(dest);
}

void eefstool_flush_device(void)
{
    printf("flush called\n");
}

void eefstool_lock(void)
{
    return;
}

void eefstool_unlock(void)
{
   return;
}

void eefstool_open_device (void)
{

    struct stat st;
    int         retCode;

    /*
    ** Open the image file 
    */
    imageFileFd = open(CommandLineOptions.ImageFileName,O_RDWR);
    if ( imageFileFd < 0 )
    {
      printf("Error: Cannot open image file: %s\n",CommandLineOptions.ImageFileName);
      exit(-1);
    }
   
    /*
    ** Get the size of the file
    */ 
    retCode = stat(CommandLineOptions.ImageFileName, &st);
    if ( retCode < 0 )
    {
       printf("Error: Cannot get size of image file: %s\n",CommandLineOptions.ImageFileName);
       close(imageFileFd);
       exit(-1);
    }
    eefsFileSize = (int)st.st_size;    


    /*
    ** Malloc a buffer to hold the image
    */
    eefsImageDataPtr = malloc(eefsFileSize);
    if ( eefsImageDataPtr == NULL )
    {
       printf("Error: Cannot allocate buffer for EEFS image\n");
       close(imageFileFd);
       exit(-1);
    }
    else
    {
       printf("Allocated Buffer for EEFS Image\n");
    }    

    /*
    ** Read the file into the buffer
    */
    retCode = read(imageFileFd, eefsImageDataPtr, eefsFileSize);
    if ( retCode != eefsFileSize )
    {
       printf("Error: Could not read entire image into buffer\n");
       free(eefsImageDataPtr);
       close(imageFileFd);
       exit(-1);
    }
    else
    {
       printf("Read the file into the temporary buffer\n");
    }
    CommandLineOptions.EEFSMemoryAddress = eefsImageDataPtr;
    CommandLineOptions.EEFSMemoryAddressEntered = FALSE;

    return;

}

void eefstool_close_device(void)
{
    int retCode;

    retCode = lseek(imageFileFd, 0, SEEK_SET);
    if ( retCode != 0 )
    {
       printf("Error: Could not seek back to start of file\n");
    }
    else
    {
       retCode = write(imageFileFd, eefsImageDataPtr, eefsFileSize);
       if ( retCode != eefsFileSize )
       {
          printf("Error: Could not write EEFS image back to file!\n");
       }
       else
       {
          printf("Wrote the EEFS image back out to the file\n");
       }
    }

    fsync(imageFileFd);

    /*
    ** Close the image file 
    */
    close(imageFileFd);

    return;
}
