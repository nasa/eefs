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
 * Filename: parser.h
 *
 * Purpose: This file contains typedefs and function prototypes for the file parser.c.
 *
 * Design Notes:
 *
 * References:
 *
 */

#ifndef _parser_
#define	_parser_

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"
#include "geneepromfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

/*
 * Macro Definitions
 */

#define     STRING_TOKEN_SIZE       256

/*
 * Type Definitions
 */

enum TokenValue {
    NUMBER,
    STRING,
    COMMA,
    END_OF_INPUT,
    END_OF_FILE,
};

typedef struct
{
    char            InputFilename[MAX_FILENAME_SIZE];
    char            EEFSFilename[EEFS_MAX_FILENAME_SIZE];
    uint32          SpareBytes;
    uint32          Attributes;
} InputParameters_t;

typedef struct {
    char            Filename[MAX_FILENAME_SIZE];
    FILE           *FilePointer;
    uint32          LineNumber;
    uint32          Token;
    char            StringToken[STRING_TOKEN_SIZE];
    uint32          NumberToken;
} Parser_t;

/*
 * Exported Global Data
 */

extern Parser_t     Parser;

/*
 * Exported Functions
 */

/* Opens the input file to be parsed and initializes internal data structures.  Returns TRUE if successful, FALSE
 * if an error occurred opening the file. */
boolean             ParserOpen(char *Filename);

/* Closes the input file opened by ParserOpen. */
void                ParserClose(void);

/* Gets the next token from the input file. */
uint32              GetToken(void);

/* Top level function to get the next line or set of input parameters from the input file. Parameters are returned
 * in the InputParameters structure. */
void                GetInputParameters(InputParameters_t *InputParameters);

#endif
