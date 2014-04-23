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
 * Filename: cmdlineopt.c
 *
 * Purpose: This file contains functions for processing command line options.
 *
 */

/*
 * Includes
 */

#include "common_types.h"
#include "cmdlineopt.h"
#include "geneepromfs.h"
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*
 * Macro Definitions
 */

#define     DEFAULT_ENDIAN          BIG_ENDIAN
#define     DEFAULT_EEPROM_SIZE     0x200000         /* 2 megabytes */

/*
 * Local Function Prototypes
 */

void            DisplayVersion(void);
void            DisplayUsage(void);

/*
 * Function Definitions
 */

/* Set the default values for the command line options, this should be called before calling
 * ProcessCommandLineOptions */
void SetCommandLineOptionsDefaults(CommandLineOptions_t *CommandLineOptions)
{
    memset(&CommandLineOptions->InputFilename, '\0', MAX_FILENAME_SIZE);
    memset(&CommandLineOptions->OutputFilename, '\0', MAX_FILENAME_SIZE);
    CommandLineOptions->Verbose = FALSE;
    CommandLineOptions->Endian = DEFAULT_ENDIAN;
    CommandLineOptions->EEPromSize = DEFAULT_EEPROM_SIZE;
    CommandLineOptions->FillEEProm = FALSE;
    CommandLineOptions->TimeStamp = time(NULL);
    CommandLineOptions->Map = FALSE;
    memset(&CommandLineOptions->MapFilename, '\0', MAX_FILENAME_SIZE);
}

/* Process command line options, options specified on the command line will override the values
 * set by SetCommandLineOptionsDefauls */
void ProcessCommandLineOptions(int argc, char *argv[], CommandLineOptions_t *CommandLineOptions)
{
    int   opt = 0;
    int   longIndex = 0;

    static const char *optString = "e:s:t:m:vVfh";

    static const struct option longOpts[] = {
            { "endian",                   required_argument, NULL, 'e' },
            { "eeprom_size",              required_argument, NULL, 's' },
            { "time",                     required_argument, NULL, 't' },
            { "map",                      required_argument, NULL, 'm' },
            { "fill_eeprom",              no_argument,       NULL, 'f' },
            { "verbose",                  no_argument,       NULL, 'v' },
            { "version",                  no_argument,       NULL, 'V' },
            { "help",                     no_argument,       NULL, 'h' },
            { NULL,                       no_argument,       NULL, 0 }
    };

    /* Process optional parameters */
    opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    while(opt != -1)
    {
        switch(opt)
        {

            case 'e':
                if (strcmp(optarg, "big") == 0 ||
                    strcmp(optarg, "BIG") == 0) {
                    CommandLineOptions->Endian = BIG_ENDIAN;
                }
                else if (strcmp(optarg, "little") == 0 ||
                         strcmp(optarg, "LITTLE") == 0) {
                    CommandLineOptions->Endian = LITTLE_ENDIAN;
                }
                else {
                    UglyExit("ERROR: Invalid Endian Parameter, Must Be big or little\n");
                }
                break;

            case 's':
            {
                char *end;
                CommandLineOptions->EEPromSize = strtoul(optarg, &end, 0);
                if (*end != 0 || errno != 0) {
                    UglyExit("Error: Invalid EEPROM Size Parameter: %s\n", optarg);
                }
                break;
            }

            case 't':
            {
                char *end;
                CommandLineOptions->TimeStamp = strtoul(optarg, &end, 0);
                if (*end != 0 || errno != 0) {
                    UglyExit("Error: Invalid Time Parameter: %s\n", optarg);
                }
                break;
            }

            case 'f':
                CommandLineOptions->FillEEProm = TRUE;
                break;

            case 'm':
                CommandLineOptions->Map = TRUE;
                if (strlen(optarg) > 0) {
                    strncpy(CommandLineOptions->MapFilename, optarg, MAX_FILENAME_SIZE);
                }
                else {
                    UglyExit("ERROR: Invalid Map Filename\n");
                }
                break;

            case 'v':
                CommandLineOptions->Verbose = TRUE;
                break;

            case 'V':
                DisplayVersion();
                break;

            case 'h':
                DisplayUsage();
                break;

            default:
                break;
        }

        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    }

    /* Adjust argc and argv to remove all of the options we have already processed */
    argc -= optind;
    argv += optind;

    /* The Input Filename and the Output Filename are required parameters */
    if (argc == 2) {
        strncpy(CommandLineOptions->InputFilename, argv[0], MAX_FILENAME_SIZE);
        strncpy(CommandLineOptions->OutputFilename, argv[1], MAX_FILENAME_SIZE);
    }
    else {
        DisplayUsage();
    }
}

/* Displays version information on the console and exits */
void DisplayVersion(void)
{
    printf("geneepromfs     %.1f\n", VERSION_NUMBER);
    printf("\n");
    exit(1);
}

/* Displays usage information on the console and exits */
void DisplayUsage(void)
{
    printf("Usage: geneepromfs [OPTION]... INPUT_FILE OUTPUT_FILE\n");
    printf("Build a EEPROM File System Image.\n");
    printf("\n");
    printf("  Options:\n");
    printf("  -e, --endian=big or little        set the output encoding (big)\n");
    printf("  -s, --eeprom_size=SIZE            set the size of the target eeprom (2 Mb)\n");
    printf("  -t, --time=TIME                   set the file timestamps to a fixed value\n");
    printf("  -f, --fill_eeprom                 fill unused eeprom with 0's\n");
    printf("  -v, --verbose                     print the name of each file added to the\n");
    printf("                                      file system\n");
    printf("  -m, --map=FILENAME                output a file system memory map\n");
    printf("  -V, --version                     output version information and exit\n");
    printf("  -h, --help                        output usage information and exit\n");
    printf("\n");
    printf("  The INPUT_FILE is a formatted text file that specifies the files to be added\n");
    printf("    to the file system.  Each entry in the INPUT_FILE contains the following\n");
    printf("    fields separated by a comma:\n");
    printf("    1. Input Filename: The path and name of the file to add to the file system\n");
    printf("    2. EEFS Filename: The name of the file in the eeprom file system.  Note the\n");
    printf("         EEFS Filename can be different from the original Input Filename\n");
    printf("    3. Spare Bytes: The number of spare bytes to add to the end of the file.  \n");
    printf("         Note also that the max size of the file is rounded up to the nearest\n");
    printf("         4 byte boundary.\n");
    printf("    4. Attributes: The file attributes, EEFS_ATTRIBUTE_NONE or EEFS_ATTRIBUTE_READONLY.\n");
    printf("    Each entry must end with a semicolon.\n");
    printf("    Comments can be added to the file by preceding the comment with an\n");
    printf("      exclamation point.\n");
    printf("\n");
    printf("    Example:\n");
    printf("    !\n");
    printf("    ! Input Filename             EEFS Filename     Spare Bytes  Attributes\n");
    printf("    !-------------------------------------------------------------------------------\n");
    printf("      /../images/cfe-core.slf,   file1.slf,        100,         EEFS_ATTRIBUTE_NONE;\n");
    printf("\n");
    exit(1);
}
