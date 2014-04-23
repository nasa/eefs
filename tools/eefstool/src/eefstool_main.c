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
**  $Id: eefstool_main.c 1.2 2011/08/09 11:42:58GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2011/08/09 11:42:58GMT-05:00 $
** $Revision: 1.2 $
** $Log: eefstool_main.c  $
** Revision 1.2 2011/08/09 11:42:58GMT-05:00 acudmore 
** EEFS 2.0 changes for eefstool
** Revision 1.4 2010/06/04 16:09:40EDT acudmore 
** Added --usage command
** Revision 1.3 2010/05/13 16:25:48EDT acudmore 
** updated printfs
** Revision 1.2 2010/05/13 09:41:09EDT acudmore 
** Update EEFS tool
** Revision 1.1 2010/05/06 15:09:05EDT acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eefs/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/16 10:41:40EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eeprom-filesystem/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/10 14:37:53EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/src/project.pj
** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include "common_types.h"
#include "eefs_filesys.h"
#include "eefstool_driver.h"
#include "eefstool_opts.h"

/*
** Global variables
*/
CommandLineOptions_t CommandLineOptions;

char                 eefs_filename[128];
char                 eefs_mountpoint[64] = "/eebank1";

/*
** Code 
*/

int main (int argc, char **argv)
{
    int32                         Status;
    EEFS_DirectoryDescriptor_t   *DirDescriptor = NULL;
    EEFS_DirectoryEntry_t        *DirEntry = NULL;
	EEFS_Stat_t                   StatBuffer;
    int32                         eefs_fd;
    int                           host_fd;
	int                           total_used_space;
	int                           total_free_space;

    /*
    ** Process the command line options
    */
    SetCommandLineOptionsDefaults(&CommandLineOptions);
    ProcessCommandLineOptions(argc, argv, &CommandLineOptions);

    /*
    ** Open the device ( BDM or image file )
    */
    eefstool_open_device();

    /*
    ** Init the EEFS
    */

    /*
    ** Mount the EEPROM disk volumes
    */
    Status = EEFS_InitFS("/EEDEV1", CommandLineOptions.EEFSMemoryAddress);
    if ( Status == 0 )
    {
       Status = EEFS_Mount("/EEDEV1",eefs_mountpoint);
       if ( Status != 0 )
       {
          printf("Error: Failed to mount EEPROM File System\n");
       }
    }
    else
    {
       printf("Error: Failed to initialize EEPROM FS\n");
    } 
  
    /*
    ** Process the command
    */
    if ( CommandLineOptions.DirectoryListingCommand == TRUE )
    {
       /*
       ** Get a directory of the EEFS
       */
       DirDescriptor = EEFS_OpenDir(eefs_mountpoint); 
       if ( DirDescriptor != NULL )
       {  
          DirEntry = NULL;
          printf("--> EEFS Directory:\n");
          printf("%32s      %10s\n","Filename","Size");
          printf("------------------------------------------------------\n");
                  
          while ( (DirEntry = EEFS_ReadDir(DirDescriptor)) != NULL )
          {
             if ( DirEntry->InUse != 0 )
             {
                printf("%32s      %10d\n",DirEntry->Filename,(int)DirEntry->MaxFileSize);
             }
          }
          printf("------------------------------------------------------\n");
          Status = EEFS_CloseDir(DirDescriptor); 
       }
    }
    if ( CommandLineOptions.EepromUsageCommand == TRUE )
    {
       /*
       ** Dump the EEPROM usage stats for the EEFS
       */
       total_used_space = 0;
       total_free_space = 0;
       DirDescriptor = EEFS_OpenDir(eefs_mountpoint); 
       if ( DirDescriptor != NULL )
       {
          DirEntry = NULL;
          printf("--> EEFS Usage Stats:\n");		  
          printf("%32s      %10s    %10s\n","Filename","Size", "Max Size");
          printf("------------------------------------------------------\n");
                  
          while ( (DirEntry = EEFS_ReadDir(DirDescriptor)) != NULL )
          {
             if ( DirEntry->InUse != 0 )
             {
                strcpy(eefs_filename, eefs_mountpoint);
                strcat(eefs_filename,"/");
				strcat(eefs_filename,DirEntry->Filename);
                Status = EEFS_Stat(eefs_filename, &StatBuffer);

                if ( Status == 0 )
				{
				   printf("%32s      %10d    %10d\n",DirEntry->Filename, (int)StatBuffer.FileSize,
				                                     (int)DirEntry->MaxFileSize);
				   total_used_space = total_used_space + StatBuffer.FileSize;
				   total_free_space = total_free_space + 
				                      ( DirEntry->MaxFileSize - StatBuffer.FileSize);
				}
				else
				{
				   printf("Error: Cannot get Stat buffer for file: %s\n",eefs_filename);
				}
             }
          }
          printf("------------------------------------------------------\n");
		  printf("Total Used space = %d bytes.\n",total_used_space);
		  printf("Total Free space = %d bytes.\n",total_free_space);
		  printf("Total Space = %d bytes.\n",total_used_space + total_free_space);
          Status = EEFS_CloseDir(DirDescriptor); 
       }
    }
    else if ( CommandLineOptions.CopyFromCommand == TRUE )
    {    
       host_fd = open(CommandLineOptions.Filename2, O_CREAT | O_WRONLY, S_IRWXU );
       if ( host_fd < 0 )
       {
          printf("Error opening host file: %s\n",CommandLineOptions.Filename2);
       }
       else
       {
          strcpy(eefs_filename, eefs_mountpoint);
          strcat(eefs_filename,"/");
          strcat(eefs_filename,CommandLineOptions.Filename1);

          eefs_fd = EEFS_Open(eefs_filename, 0);
          if ( eefs_fd < 0 )
          {
             printf("Error opening EEFS file: %s\n",CommandLineOptions.Filename1);
             close(host_fd);
          }
          else
          {
             int32   DataRead;
             int     DataWritten;
             boolean DataToRead;
             char    buffer[512];

             /*
             ** Copy the file
             */
             printf("Copying: EEPROM File System: %s, to the host: %s\n",
               CommandLineOptions.Filename1,
               CommandLineOptions.Filename2);    

             DataToRead = TRUE;
             while ( DataToRead == TRUE )
             {
                DataRead = EEFS_Read(eefs_fd, &buffer[0], 512);
                if ( DataRead == 0 )
                {
                   DataToRead = FALSE;
                }
                else
                {
                   DataWritten = write(host_fd, &buffer[0],DataRead);
                   if ( DataWritten != DataRead )
                   {
                     printf("Warning: Amount of data written != Data Read\n");
                   }
                } 

             } /* End while */

             EEFS_Close(eefs_fd);
             close(host_fd);
      
             printf("Copy completed\n");
   
          } 
       } 
    }
    else if ( CommandLineOptions.CopyToCommand == TRUE )
    {
       host_fd = open(CommandLineOptions.Filename1,  O_RDONLY, S_IRWXU );
       if ( host_fd < 0 )
       {
          printf("Error opening host file: %s\n",CommandLineOptions.Filename1);
       }
       else
       {
          strcpy(eefs_filename, eefs_mountpoint);
          strcat(eefs_filename,"/");
          strcat(eefs_filename,CommandLineOptions.Filename2);

          eefs_fd = EEFS_Creat(eefs_filename, 0);
          if ( eefs_fd < 0 )
          {
             printf("Error calling EEFS_Creat on EEFS file: %s\n",CommandLineOptions.Filename2);
             close(host_fd);
          }
          else
          {
             int32   DataRead;
             int     DataWritten;
             boolean DataToRead;
             char    buffer[512];

             /*
             ** Copy the file
             */
             printf("Copying: From the host file %s, to the EEPROM File System file: %s.\n",
                     CommandLineOptions.Filename1,
                     CommandLineOptions.Filename2);    

             DataToRead = TRUE;
             while ( DataToRead == TRUE )
             {
                DataRead = read(host_fd, &buffer[0], 512);
                if ( DataRead == 0 )
                {
                   DataToRead = FALSE;
                }
                else
                {
                   DataWritten = EEFS_Write(eefs_fd, &buffer[0],DataRead);
                   if ( DataWritten != DataRead )
                   {
                     printf("Warning: Amount of data written != Data Read\n");
                   }
                } 

             } /* End while */

             EEFS_Close(eefs_fd);
             close(host_fd);
      
             printf("\nCopy completed\n");
   
          } 
       } 

    }
    else if ( CommandLineOptions.DeleteCommand == TRUE )
    {
       printf("Deleting %s from the EEPROM File system\n",
               CommandLineOptions.Filename1);
       printf("Done\n");

    }
    else if ( CommandLineOptions.RenameCommand == TRUE )
    {
       printf("Rename a file on the EEPROM file system from: %s, to %s\n",
               CommandLineOptions.Filename1,
               CommandLineOptions.Filename2);
    }
    /*
    ** Close the device ( BDM or image file )
    */ 
    eefstool_close_device();

    return 0;
}
