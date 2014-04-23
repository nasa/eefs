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
 * Filename: cmdlineopt.h
 *
 * Purpose: This file contains the typedefs and function prototypes for the file cmdlineopt.c.
 *
 * Design Notes:
 *
 * References:
 *
 */

#ifndef _cmdlineopt_
#define	_cmdlineopt_

/*
 * Includes
 */

#include "common_types.h"
#include "geneepromfs.h"
#include <time.h>

/*
 * Type Definitions
 */

typedef struct {
    char            InputFilename[MAX_FILENAME_SIZE];
    char            OutputFilename[MAX_FILENAME_SIZE];
    boolean         Verbose;
    uint32          Endian;
    uint32          EEPromSize;
    boolean         FillEEProm;
    time_t          TimeStamp;
    boolean         Map;
    char            MapFilename[MAX_FILENAME_SIZE];
} CommandLineOptions_t;

/*
 * Exported Functions
 */

/* Set the default values for the command line options, this should be called before calling
 * ProcessCommandLineOptions */
void SetCommandLineOptionsDefaults(CommandLineOptions_t *CommandLineOptions);

/* Process command line options, options specified on the command line will override the values
 * set by SetCommandLineOptionsDefauls */
void ProcessCommandLineOptions(int argc, char *argv[], CommandLineOptions_t *CommandLineOptions);

#endif	
