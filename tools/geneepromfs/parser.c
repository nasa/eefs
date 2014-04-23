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
 * Filename: parser.c
 *
 * Purpose: This file contains functions to parse input parameters from a input text file.
 *
 */

/*
 * Includes
 */

#include "common_types.h"
#include "parser.h"
#include "geneepromfs.h"
#include <string.h>
#include <ctype.h>
#include <errno.h>

/*
 * Exported Global Data
 */

Parser_t            Parser;

/*
 * Local Function Prototypes
 */

void                GetInputFilename(InputParameters_t *InputParameters);
void                GetEEFSFilename(InputParameters_t *InputParameters);
void                GetSpareBytes(InputParameters_t *InputParameters);
void                GetAttributes(InputParameters_t *InputParameters);

/*
 * Function Definitions
 */

/* Opens the input file to be parsed and initializes internal data structures.  Returns TRUE if successful, FALSE
 * if an error occurred opening the file. */
boolean ParserOpen(char *Filename)
{
    memset(&Parser, 0, sizeof(Parser_t));
    if ((Parser.FilePointer = fopen(Filename, "r")) != NULL) {
        strncpy(Parser.Filename, Filename, MAX_FILENAME_SIZE);
        Parser.LineNumber = 1;
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

/* Closes the input file opened by ParserOpen. */
void ParserClose(void)
{
    if (Parser.FilePointer) {
        fclose(Parser.FilePointer);
    }
    memset(&Parser, 0, sizeof(Parser_t));
}

/* Gets the next token from the input file. */
uint32 GetToken(void)
{
    int                 ch;
    uint32              i;

    while (TRUE) {

        /* skip blank spaces */
        do {
            if ((ch = fgetc(Parser.FilePointer)) == EOF) return(Parser.Token = END_OF_FILE);
        } while (isspace(ch) && ch != '\n');

        switch (ch) {

            case '\n':                  /* end of line */
                Parser.LineNumber++;
                break;

            case ',':                   /* field seperator */
                return(Parser.Token = COMMA);
                break;

            case ';':                   /* end of input */
                return(Parser.Token = END_OF_INPUT);
                break;

            case '!':                   /* comment extends to the end of the line */
                do {
                    if ((ch = fgetc(Parser.FilePointer)) == EOF) return(Parser.Token = END_OF_FILE);
                } while (ch != '\n');
                ungetc(ch, Parser.FilePointer);
                break;

            default: 
                /* A STRING_TOKEN must begin with a letter, '/', '\', or '.', can contain letters, numbers, 
                 * '/', '\', '.', '_', or '-', and cannot contain any spaces */
                if (isalpha(ch) || ch == '/' || ch == '\\' || ch == '.') { /* string argument */
                    for (i=0; i < STRING_TOKEN_SIZE; i++)  {
                        if (isalnum(ch) || ch == '/' || ch == '\\' || ch == '.' || ch == '_' || ch == '-') {
                            Parser.StringToken[i] = ch;
                        }
                        else {
                            ungetc(ch, Parser.FilePointer);
                            Parser.StringToken[i] = '\0';
                            return(Parser.Token = STRING);
                        }
                        if ((ch = fgetc(Parser.FilePointer)) == EOF) return(Parser.Token = END_OF_FILE);
                    }
                    UglyExit("File: %s Line: %lu: Error: String Token \'%s\' Too Long, Max Length: %lu\n", Parser.Filename, Parser.LineNumber, Parser.StringToken, STRING_TOKEN_SIZE);

                } 
                /* A NUMBER_TOKEN is parsed as a string and must begin with a digit and may be specified in decimal, hex, or
                 * octal. Integers are supported, no floating point or + - notations */
                else if (isdigit(ch)) { 
                    for (i=0; i < STRING_TOKEN_SIZE; i++) {
                        if (isxdigit(ch) || ch == 'x' || ch == 'X') {
                            Parser.StringToken[i] = ch;
                        }
                        else {
                            char *end;
                            ungetc(ch, Parser.FilePointer);
                            Parser.StringToken[i] = '\0';
                            Parser.NumberToken = strtoul(Parser.StringToken, &end, 0);
                            if (*end != 0 || errno != 0) {
                                UglyExit("File: %s Line: %lu: Error: Invalid Unsigned Integer Value: %s", Parser.Filename, Parser.LineNumber, Parser.StringToken);
                            }
                            return(Parser.Token = NUMBER);
                        }
                        if ((ch = fgetc(Parser.FilePointer)) == EOF) return(Parser.Token = END_OF_FILE);
                    }
                    UglyExit("File: %s Line: %lu: Error: Number Token \'%s\' Too Long, Max Length: %lu\n", Parser.Filename, Parser.LineNumber, Parser.StringToken, STRING_TOKEN_SIZE);
                }
                else {
                    UglyExit("File: %s Line: %lu: Error: Unexpected Input: %c\n", Parser.Filename, Parser.LineNumber, ch);
                }
                break;
        }
    }
}

/* Top level function to get the next line or set of input parameters from the input file. Parameters are returned
 * in the InputParameters structure. */
void GetInputParameters(InputParameters_t *InputParameters)
{
    memset(InputParameters, 0, sizeof(InputParameters_t));
    GetInputFilename(InputParameters);
}

/* Parse the Input Filename parameter */
void GetInputFilename(InputParameters_t *InputParameters)
{
    if (Parser.Token == STRING) {
        if (strlen(Parser.StringToken) < MAX_FILENAME_SIZE) {
            strcpy(InputParameters->InputFilename, Parser.StringToken);
            GetToken();
            if (Parser.Token == COMMA) {
                GetToken();
                GetEEFSFilename(InputParameters);
            }
            else {
                UglyExit("File: %s Line: %lu: Error: Missing \',\' After Input Filename\n", Parser.Filename, Parser.LineNumber);
            }
        }
        else {
            UglyExit("File: %s Line: %lu: Error: Input Filename Too Long, Max Length: %lu\n", Parser.Filename, Parser.LineNumber, MAX_FILENAME_SIZE);
        }
    }
    else {
        UglyExit("File: %s Line: %lu: Error: Missing Input Filename\n", Parser.Filename, Parser.LineNumber);
    }
}

/* Parse the EEFSFilename parameter */
void GetEEFSFilename(InputParameters_t *InputParameters)
{
    if (Parser.Token == STRING) {
        if (strlen(Parser.StringToken) < EEFS_MAX_FILENAME_SIZE) { 
            strcpy(InputParameters->EEFSFilename, Parser.StringToken);
            GetToken();
            if (Parser.Token == COMMA) {
                GetToken();
                GetSpareBytes(InputParameters);
            }
            else {
                UglyExit("File: %s Line: %lu: Error: Missing \',\' After EEFS Filename\n", Parser.Filename, Parser.LineNumber);
            }
        }
        else {
            UglyExit("File: %s Line: %lu: Error: EEFS Filename Too Long, Max Length: %lu\n", Parser.Filename, Parser.LineNumber, EEFS_MAX_FILENAME_SIZE);
        }
    }
    else {
        UglyExit("File: %s Line: %lu: Error: Missing EEFS Filename\n", Parser.Filename, Parser.LineNumber);
    }
}

/* Parse the Spare Bytes parameter */
void GetSpareBytes(InputParameters_t *InputParameters)
{
    if (Parser.Token == NUMBER) {
        InputParameters->SpareBytes = Parser.NumberToken;
        GetToken();
        if (Parser.Token == COMMA) {
            GetToken();
            GetAttributes(InputParameters);
        }
        else {
            UglyExit("File: %s Line: %lu: Error: Missing \',\' After Spare Bytes\n", Parser.Filename, Parser.LineNumber);
        }
    }
    else {
        UglyExit("File: %s Line: %lu: Error: Missing Spare Bytes\n", Parser.Filename, Parser.LineNumber);
    }
}

/* Parse the Attributes parameter */
void GetAttributes(InputParameters_t *InputParameters)
{
    if (Parser.Token == STRING) {
        if (strcmp(Parser.StringToken, "EEFS_ATTRIBUTE_READONLY") == 0)
            InputParameters->Attributes = EEFS_ATTRIBUTE_READONLY;
        else if (strcmp(Parser.StringToken, "EEFS_ATTRIBUTE_NONE") == 0)
            InputParameters->Attributes = EEFS_ATTRIBUTE_NONE;
        else
            UglyExit("File: %s Line: %lu: Error: Invalid Attribute: %s\n", Parser.Filename, Parser.LineNumber, Parser.StringToken);
        GetToken();
        if (Parser.Token == END_OF_INPUT) {
            GetToken();
        }
        else {
            UglyExit("File: %s Line: %lu: Error: Missing \';\' After Attributes\n", Parser.Filename, Parser.LineNumber);
        }
    }
    else {
        UglyExit("File: %s Line: %lu: Error: Missing Attributes\n", Parser.Filename, Parser.LineNumber);
    }
}
