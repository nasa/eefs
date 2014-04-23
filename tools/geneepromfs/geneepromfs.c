
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
 * Filename: geneepromfs.c
 *
 * Purpose: This file contains functions to build a EEPROM File System image.
 *
 */

/*
 * Includes
 */

#include "common_types.h"
#include "parser.h"
#include "cmdlineopt.h"
#include "geneepromfs.h"
#include "eefs_fileapi.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * Macro Definitions
 */

#define ROUND_UP(x, align)	(((int) (x) + (align - 1)) & ~(align - 1))

/*
 * Type Definitions
 */

typedef struct {
    void                           *BaseAddress;
    EEFS_FileAllocationTable_t     *FileAllocationTable;
} FileSystem_t;

/*
 * Local Data
 */

CommandLineOptions_t        CommandLineOptions;

/*
 * Local Function Prototypes
 */

void                        AddFile(FileSystem_t *FileSystem, char *InputFilename, char *EEFSFilename, uint32 SpareBytes, uint32 Attributes);
boolean                     IsDuplicateFilename(FileSystem_t *FileSystem, char *Filename);
uint32                      Fsize(char *Filename);
void                        OutputMemoryMap(FileSystem_t *FileSystem, char *Filename);
void                        ByteSwapFileSystem(FileSystem_t *FileSystem);
void                        SwapUInt32(uint32 *ValueToSwap);
uint32                      ThisMachineDataEncoding(void);
uint32                      CalculateCRC(void *DataPtr, int DataLength, uint32 InputCrc);

/*
 * Function Definitions
 */

int main(int argc, char** argv) {

    FILE                            *OutputFilePointer;
    InputParameters_t                InputParameters;
    FileSystem_t                     FileSystem;

    SetCommandLineOptionsDefaults(&CommandLineOptions);
    ProcessCommandLineOptions(argc, argv, &CommandLineOptions);

    if ((FileSystem.BaseAddress = malloc(CommandLineOptions.EEPromSize))) {

        memset(FileSystem.BaseAddress, 0, CommandLineOptions.EEPromSize);

        FileSystem.FileAllocationTable = FileSystem.BaseAddress;
        
        FileSystem.FileAllocationTable->Header.Crc = 0; /* updated later */
        FileSystem.FileAllocationTable->Header.Magic = EEFS_FILESYS_MAGIC;
        FileSystem.FileAllocationTable->Header.Version = 1;
        FileSystem.FileAllocationTable->Header.FreeMemoryOffset = sizeof(EEFS_FileAllocationTable_t);
        FileSystem.FileAllocationTable->Header.FreeMemorySize = (CommandLineOptions.EEPromSize - sizeof(EEFS_FileAllocationTable_t));
        FileSystem.FileAllocationTable->Header.NumberOfFiles = 0;

        if (ParserOpen(CommandLineOptions.InputFilename)) {

            /* read each entry in the input file and add it to the file system */
            GetToken();
            while (Parser.Token != END_OF_FILE) {
                GetInputParameters(&InputParameters);
                AddFile(&FileSystem, InputParameters.InputFilename, InputParameters.EEFSFilename, InputParameters.SpareBytes, InputParameters.Attributes);
            }
            ParserClose();

            if (CommandLineOptions.Map)
                OutputMemoryMap(&FileSystem, CommandLineOptions.MapFilename);

            /* write the output file */
            if ((OutputFilePointer = fopen(CommandLineOptions.OutputFilename, "w"))) {

                /* Kludge to save off the current values of these variables before we byte swap the file system */
                uint32      FreeMemoryOffset = FileSystem.FileAllocationTable->Header.FreeMemoryOffset;
                uint32      FreeMemorySize = FileSystem.FileAllocationTable->Header.FreeMemorySize;
                uint32      FileSystemCrc = 0;
                uint32      NumberOfFiles = FileSystem.FileAllocationTable->Header.NumberOfFiles;
                
                if (ThisMachineDataEncoding() != CommandLineOptions.Endian)
                    ByteSwapFileSystem(&FileSystem);

                /* now calculate the crc, this has to be done after the file system has been byte swapped */
                FileSystem.FileAllocationTable->Header.Crc = CalculateCRC(FileSystem.BaseAddress + 4, CommandLineOptions.EEPromSize - 4, 0);
                FileSystemCrc = FileSystem.FileAllocationTable->Header.Crc;
                if (ThisMachineDataEncoding() != CommandLineOptions.Endian)
                    SwapUInt32(&FileSystem.FileAllocationTable->Header.Crc);

                if (CommandLineOptions.FillEEProm)
                    fwrite(FileSystem.BaseAddress, CommandLineOptions.EEPromSize, 1, OutputFilePointer);
                else
                    fwrite(FileSystem.BaseAddress, FreeMemoryOffset, 1, OutputFilePointer);
                fclose(OutputFilePointer);

                if (CommandLineOptions.Verbose) {

                    printf("Max Number Of Files: %i\n", EEFS_MAX_FILES);
                    printf("Number Of Files Added: %lu\n", NumberOfFiles);
                    printf("EEPROM Size: %lu\n", CommandLineOptions.EEPromSize);
                    printf("Allocated EEPROM: %lu\n", FreeMemoryOffset);
                    printf("Unallocated EEPROM: %lu\n", FreeMemorySize);
                    printf("Utilization: %.0f%%\n", ((double)FreeMemoryOffset / (double)CommandLineOptions.EEPromSize) * 100.0);
                    printf("Image Checksum: 0x%lx\n", FileSystemCrc);
                }
            }
            else {
                UglyExit("Error: Can't Open Output File: %s, %s\n", CommandLineOptions.OutputFilename, strerror(errno));
            }
        }
        else {
            UglyExit("Error: Can't Open Input File: %s, %s\n", CommandLineOptions.InputFilename, strerror(errno));
        }

        free(FileSystem.BaseAddress);
    }
    else {
        UglyExit("Error: Can't Allocate Buffer: %u\n", CommandLineOptions.EEPromSize);
    }

    return (EXIT_SUCCESS);
}

/* Add a new file to the file system */
void AddFile(FileSystem_t *FileSystem, char *InputFilename, char *EEFSFilename, uint32 SpareBytes, uint32 Attributes)
{
    FILE                            *InputFilePointer;
    EEFS_FileHeader_t               *FileHeader;
    void                            *FileData;
    uint32                           FileSize;
    uint32                           MaxFileSize;

    if (FileSystem->FileAllocationTable->Header.NumberOfFiles < EEFS_MAX_FILES) {

        if (IsDuplicateFilename(FileSystem, EEFSFilename) == FALSE) {

            if ((InputFilePointer = fopen(InputFilename, "r"))) {

                FileSize = Fsize(InputFilename);
                MaxFileSize = ROUND_UP((FileSize + SpareBytes), 4); /* round the MaxFileSize up to the next 4 byte boundary */

                if (FileSystem->FileAllocationTable->Header.FreeMemorySize >= (sizeof(EEFS_FileHeader_t) + MaxFileSize)) {

                    if (CommandLineOptions.Verbose) printf("Adding File %s\n", EEFSFilename);

                    FileSystem->FileAllocationTable->File[FileSystem->FileAllocationTable->Header.NumberOfFiles].FileHeaderOffset = FileSystem->FileAllocationTable->Header.FreeMemoryOffset;
                    FileSystem->FileAllocationTable->File[FileSystem->FileAllocationTable->Header.NumberOfFiles].MaxFileSize = MaxFileSize;

                    FileHeader = FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[FileSystem->FileAllocationTable->Header.NumberOfFiles].FileHeaderOffset;
                    FileHeader->Crc = 0;       
                    FileHeader->InUse = TRUE;
                    FileHeader->Attributes = Attributes;
                    FileHeader->FileSize = FileSize;
                    FileHeader->ModificationDate = CommandLineOptions.TimeStamp;
                    FileHeader->CreationDate = CommandLineOptions.TimeStamp;
                    strncpy(FileHeader->Filename, EEFSFilename, EEFS_MAX_FILENAME_SIZE);

                    FileData = FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[FileSystem->FileAllocationTable->Header.NumberOfFiles].FileHeaderOffset + sizeof(EEFS_FileHeader_t);
                    fread(FileData, FileSize, 1, InputFilePointer);

                    FileSystem->FileAllocationTable->Header.FreeMemoryOffset += (sizeof(EEFS_FileHeader_t) + MaxFileSize);
                    FileSystem->FileAllocationTable->Header.FreeMemorySize -= (sizeof(EEFS_FileHeader_t) + MaxFileSize);
                    FileSystem->FileAllocationTable->Header.NumberOfFiles++;
                    fclose(InputFilePointer);
                }
                else {
                    UglyExit("Error: File System Exceeds Available EEPROM Memory: %u\n", CommandLineOptions.EEPromSize);
                }
            }
            else {
                UglyExit("Error: Can't Open Input File: %s, %s\n", InputFilename, strerror(errno));
            }
        }
        else {
            UglyExit("Error: Filename Already Exists In File System: %s\n", EEFSFilename);
        }
    }
    else {
        UglyExit("Error: Maximum Number Of Files Exceeded: %u\n", EEFS_MAX_FILES);
    }
}

/* Search the file system looking for a duplicate filename */
boolean IsDuplicateFilename(FileSystem_t *FileSystem, char *Filename)
{
    EEFS_FileHeader_t               *FileHeader;
    uint32                           i;

    for (i=0; i < FileSystem->FileAllocationTable->Header.NumberOfFiles; i++) {
        FileHeader = (EEFS_FileHeader_t *)(FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[i].FileHeaderOffset);
        if (strcmp(FileHeader->Filename, Filename) == 0) {
            return(TRUE);
        }
    }
    return(FALSE);
}

/* Return the size of a file, 0 is returned if there is an error */
uint32 Fsize(char *Filename)
{
    struct stat StatBuf;
    if (stat(Filename, &StatBuf) != -1)
        return(StatBuf.st_size);
    else
        return(0);
}

/* Output a Memory Map */
void OutputMemoryMap(FileSystem_t *FileSystem, char *Filename)
{
    FILE              *FilePointer;
    EEFS_FileHeader_t *FileHeader;
    uint8             *FileData;
    uint32             i;

    if ((FilePointer = fopen(Filename, "w"))) {

        fprintf(FilePointer, "Offset\tSize\tSection\tSlot\tFilename\tFile Size\tSpare\tMax Size\tCrc\tAttributes\n");
        fprintf(FilePointer, "%i\t%i\t%s\n",
            0,
            sizeof(EEFS_FileAllocationTable_t),
            "FAT");

        for (i=0; i < FileSystem->FileAllocationTable->Header.NumberOfFiles; i++) {

            FileHeader = FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[i].FileHeaderOffset;
            FileData = FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[i].FileHeaderOffset + sizeof(EEFS_FileHeader_t);

            fprintf(FilePointer, "%lu\t%i\t%s\t%lu\n",
                    FileSystem->FileAllocationTable->File[i].FileHeaderOffset,
                    sizeof(EEFS_FileHeader_t),
                    "Header",
                    i);

            fprintf(FilePointer, "%lu\t%lu\t%s\t%lu\t%s\t%lu\t%lu\t%lu\t0x%08lX\t%lu\n",
                    FileSystem->FileAllocationTable->File[i].FileHeaderOffset + sizeof(EEFS_FileHeader_t),
                    FileSystem->FileAllocationTable->File[i].MaxFileSize,
                    "Data",
                    i,
                    FileHeader->Filename,
                    FileHeader->FileSize,
                   (FileSystem->FileAllocationTable->File[i].MaxFileSize - FileHeader->FileSize),
                    FileSystem->FileAllocationTable->File[i].MaxFileSize,
                    CalculateCRC(FileData, FileHeader->FileSize, 0),
                    FileHeader->Attributes);
        }

        fprintf(FilePointer, "%lu\t%lu\t%s\n",
                FileSystem->FileAllocationTable->Header.FreeMemoryOffset,
                FileSystem->FileAllocationTable->Header.FreeMemorySize,
                "Free");

        fclose(FilePointer);
    }
    else {
        UglyExit("Error: Can't Open Map File: %s, %s\n", Filename, strerror(errno));
    }
}

/* Byte swap the file allocation table and all file headers */
void ByteSwapFileSystem(FileSystem_t *FileSystem)
{
    uint32                  NumberOfFiles;
    EEFS_FileHeader_t      *FileHeader;
    uint32                  i;

    NumberOfFiles = FileSystem->FileAllocationTable->Header.NumberOfFiles;

    SwapUInt32(&FileSystem->FileAllocationTable->Header.Crc);
    SwapUInt32(&FileSystem->FileAllocationTable->Header.Magic);
    SwapUInt32(&FileSystem->FileAllocationTable->Header.Version);
    SwapUInt32(&FileSystem->FileAllocationTable->Header.FreeMemoryOffset);
    SwapUInt32(&FileSystem->FileAllocationTable->Header.FreeMemorySize);
    SwapUInt32(&FileSystem->FileAllocationTable->Header.NumberOfFiles);

    for (i=0; i < NumberOfFiles; i++)
    {
        FileHeader = FileSystem->BaseAddress + FileSystem->FileAllocationTable->File[i].FileHeaderOffset;

        SwapUInt32(&FileHeader->Crc);
        SwapUInt32(&FileHeader->InUse);
        SwapUInt32(&FileHeader->Attributes);
        SwapUInt32(&FileHeader->FileSize);
        SwapUInt32((uint32 *)&FileHeader->ModificationDate);
        SwapUInt32((uint32 *)&FileHeader->CreationDate);

        /* this is done last because I use the FileHeaderOffset to calculate the
         * address of the file header above. */
        SwapUInt32(&FileSystem->FileAllocationTable->File[i].FileHeaderOffset);
        SwapUInt32(&FileSystem->FileAllocationTable->File[i].MaxFileSize);
    }
}

/* Byte swap a 32 bit integer */
void SwapUInt32(uint32 *ValueToSwap)
{
    uint8 *BytePtr = (uint8 *)ValueToSwap;
    uint8  TempByte = BytePtr[3];
    BytePtr[3] = BytePtr[0];
    BytePtr[0] = TempByte;
    TempByte   = BytePtr[2];
    BytePtr[2] = BytePtr[1];
    BytePtr[1] = TempByte;
}

/* Examine the byte ordering of a 32 bit word to determine
 * if this machine is a Big Endian or Little Endian machine */
uint32 ThisMachineDataEncoding(void)
{
    uint32   DataEncodingCheck = 0x01020304;

    if (((uint8 *)&DataEncodingCheck)[0] == 0x04 &&
        ((uint8 *)&DataEncodingCheck)[1] == 0x03 &&
        ((uint8 *)&DataEncodingCheck)[2] == 0x02 &&
        ((uint8 *)&DataEncodingCheck)[3] == 0x01) {
        return(LITTLE_ENDIAN);
    }
    else {
        return(BIG_ENDIAN);
    }
}

/* Calculate a 16 bit CRC over a range of memory.  Note that as a convention I store the calculated crc value
 * at the beginning of the data.  In this code when I call this function I am passing the address of the
 * beginning of data I am calculating the crc for which includes the crc field.  This is not a bug, the crc field is
 * initialized to 0 and does not affect the calculated crc.  I did this for convienence and to make the
 * code more readable vs adding 4 to the DataPtr and subtracting 4 from the DataLength to skip the crc. */
uint32 CalculateCRC(void *DataPtr, int DataLength, uint32 InputCrc)
{
    int             i;
    int16           Index;
    int16           Crc = 0;
    uint8          *BufPtr;

    static const uint16 CrcTable[256]=
    {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    };

    Crc    = (int16)(0xFFFF & InputCrc);
    BufPtr = (uint8 *)DataPtr;
    for (i=0; i < DataLength; i++, BufPtr++) {
        Index = ((Crc ^ *BufPtr) & 0x00FF);
        Crc = ((Crc >> 8) & 0x00FF) ^ CrcTable[Index];
    }
    return(Crc);
}

/* Print a error message and exit */
void UglyExit(char *Spec, ...)
{
    va_list         Args;
    static char     Text[256];

    va_start(Args, Spec);
    vsprintf(Text, Spec, Args);
    va_end(Args);
    
    printf("%s", Text);
    exit(1);
}
