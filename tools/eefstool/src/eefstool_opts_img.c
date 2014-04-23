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
**  $Id: eefstool_opts_img.c 1.2 2011/08/09 11:42:59GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2011/08/09 11:42:59GMT-05:00 $
** $Revision: 1.2 $
** $Log: eefstool_opts_img.c  $
** Revision 1.2 2011/08/09 11:42:59GMT-05:00 acudmore 
** EEFS 2.0 changes for eefstool
** Revision 1.2 2010/06/04 16:09:41EDT acudmore 
** Added --usage command
** Revision 1.1 2010/05/06 15:09:07EDT acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eefs/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/16 10:41:41EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eeprom-filesystem/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/10 14:37:54EST acudmore 
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

#include "common_types.h"
#include "eefstool_opts.h"

/* 
** This function initializes all command line options to their default values 
*/
void SetCommandLineOptionsDefaults(CommandLineOptions_t *CommandLineOptions)
{

    memset(&CommandLineOptions->Filename1, 0, FILENAME_SIZE);
    memset(&CommandLineOptions->Filename2, 0, FILENAME_SIZE);
    
    memset(&CommandLineOptions->ImageFileName, 0, FILENAME_SIZE);
    CommandLineOptions->ImageFileNameEntered = FALSE;

    CommandLineOptions->DirectoryListingCommand = FALSE;
	CommandLineOptions->EepromUsageCommand = FALSE;
    CommandLineOptions->CopyFromCommand = FALSE;
    CommandLineOptions->CopyToCommand = FALSE;
    CommandLineOptions->DeleteCommand = FALSE;
    CommandLineOptions->RenameCommand = FALSE;
    CommandLineOptions->NeedArgs = 0;

    CommandLineOptions->CommandSelected = FALSE;
}

/* this function processes alll command line options */
void ProcessCommandLineOptions(int argc, char *argv[], CommandLineOptions_t *CommandLineOptions)
{
    int   opt = 0;
    int   longIndex = 0;

    static const char *optString = "lftdri:?";

    static const struct option longOpts[] = {
            { "img_file",         required_argument, NULL, 'i' },
            { "dir",              no_argument,       NULL, 'l' },
			{ "usage",            no_argument,       NULL, 'u' },
            { "copy_from",        no_argument,       NULL, 'f' },
            { "copy_to",          no_argument,       NULL, 't' },
            { "delete",           no_argument,       NULL, 'd' },
            { "rename",           no_argument,       NULL, 'r' },
            { "help",             no_argument,       NULL, '?' },
            { NULL,               no_argument,       NULL,   0 }
    };

    /* Process optional parameters */
    opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    while(opt != -1)
    {
        switch(opt)
        {
            case 'i':
                strncpy(CommandLineOptions->ImageFileName, optarg, FILENAME_SIZE);
                CommandLineOptions->ImageFileNameEntered = TRUE;
                break;

            case 'l':
                if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->DirectoryListingCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 0;
                }
                else
                {
                   printf("Error: Cannot enter two command actions\n");
                   DisplayUsage();
                }
                break;
				
            case 'u':
                if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->EepromUsageCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 0;
                }
                else
                {
                   printf("Error: Cannot enter two command actions\n");
                   DisplayUsage();
                }
                break;
				
            case 'f':
                 if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->CopyFromCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 2;
                }
                else
                {
                   printf("Error: Cannot enter two command actions\n");
                   DisplayUsage();
                }
                break;

            case 't':
                if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->CopyToCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 2;
                }
                else
                {
                   printf("Error: Cannot enter two command actions\n");
                   DisplayUsage();
                }
                break;

            case 'd':
                if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->DeleteCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 1;
                }
                else
                {
                   printf("Error: Cannot enter two command actions\n");
                   DisplayUsage();
                }
                break;

            case 'r':
               if ( CommandLineOptions->CommandSelected == FALSE )
                {
                   CommandLineOptions->RenameCommand = TRUE;
                   CommandLineOptions->CommandSelected = TRUE;
                   CommandLineOptions->NeedArgs = 2;
                }
                else
                {
                   printf("Error: Only one command option is supported at a time.\n");
                   DisplayUsage();
                }
                break;

            case '?':
                DisplayUsage();
                break;

            default:
                break;
        }

        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    }

    /*
    ** Must have:
    **    BDMDeviceEntered = TRUE
    **    BDMMemoryAddressEntered = TRUE;
    */
    if ( CommandLineOptions->ImageFileNameEntered == FALSE )
    {
       printf("Error: Must enter an EEPROM image filename.\n");
       DisplayUsage();
    }

    if ( CommandLineOptions->CommandSelected == TRUE )
    {

        /* Adjust argc and argv to remove all of the options we have already processed */
        argc -= optind;
        argv += optind;

        if ( CommandLineOptions->NeedArgs == 1 )
        {
           strncpy(CommandLineOptions->Filename1, argv[0], FILENAME_SIZE);
        }
        else if ( CommandLineOptions->NeedArgs == 2 )
        {
           strncpy(CommandLineOptions->Filename1, argv[0], FILENAME_SIZE);
           strncpy(CommandLineOptions->Filename2, argv[1], FILENAME_SIZE);
        }
 
    }
    else
    {
        printf("Error: No command Selected\n");
        DisplayUsage();
    }

}

/* This function displays the usage information on the console and exits */
void DisplayUsage(void)
{    
    printf("Usage: eefstool-img --img_file=<eefs image filename> [OPTION]... [FILE1] [FILE2]\n");
    printf("  Options:\n");
    printf("  --dir                      : List the contents of the EEFS File System\n");
    printf("  --usage                    : Dump the Usage information for the EEFS File System\n");	
    printf("  --copy_from FILE1 FILE2    : Override the default Entry Point Name\n");
    printf("  --copy_to   FILE1 FILE2    : Compress the Text and Data Sections\n");
    printf("  --delete    FILE1          : Print the contents of the ELF file to the console\n");
    printf("  --rename    FILE1 FILE2    : Print the CFE Static Link File Header to the console\n");
    printf("  --help                     : Print this help.\n");
    printf(" \n");
    printf("  Example:  \n");
    printf("   eefstool-img --img_file=eefs.img --copy_from cfe-core.slf /tmp/cfe-core.slf\n");
    printf(" \n");

    exit(1);
}


