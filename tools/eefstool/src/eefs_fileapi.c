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
 * Filename: eefs_fileapi.c
 *
 * Purpose: This file contains the lower level interface functions to the eeprom file system api functions.  These functions
 *   are volume or device independent, so most functions require a pointer to a inode table.  All api functions are designed 
 *   to be as similar to a standard unix file system api as possible.
 *
 */

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"
#include "eefs_macros.h"
#include <string.h>
#include <math.h>
#include "eefs_swap.h"

/*
 * Macro Definitions
 */

#define EEFS_MAX(x,y) (((x) > (y)) ? (x) : (y))
#define EEFS_MIN(x,y) (((x) < (y)) ? (x) : (y))
#define EEFS_ROUND_UP(x, align)	(((int) (x) + (align - 1)) & ~(align - 1))

/*
 * Local Data
 */

/* Note: the file descriptors are shared across all file systems */
uint32                          EEFS_FileDescriptorsInUse;
uint32                          EEFS_FileDescriptorsHighWaterMark;
EEFS_FileDescriptor_t           EEFS_FileDescriptorTable[EEFS_MAX_OPEN_FILES];

/* Note: at the moment there is only one directory descriptor, so only one process can use it at a time */
EEFS_DirectoryDescriptor_t      EEFS_DirectoryDescriptor;
EEFS_DirectoryEntry_t           EEFS_DirectoryEntry;

/*
 * Local Function Prototypes
 */

int32                           EEFS_LibOpenFile(EEFS_InodeTable_t *InodeTable, int32 InodeIndex, uint32 Flags, uint32 Attributes);
int32                           EEFS_LibCreatFile(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes);
uint32                          EEFS_LibFmode(EEFS_InodeTable_t *InodeTable, uint32 InodeIndex);
int32                           EEFS_LibFindFile(EEFS_InodeTable_t *InodeTable, char *Filename);
int32                           EEFS_LibGetFileDescriptor(void);
int32                           EEFS_LibFreeFileDescriptor(int32 FileDescriptor);
uint8                           EEFS_LibIsValidFileDescriptor(int32 FileDescriptor);
uint8                           EEFS_LibHasOpenCreat(EEFS_InodeTable_t *InodeTable);
uint8                           EEFS_LibIsValidFilename(char *Filename);

/*
 * Function Definitions
 */

/* Initialize global data shared by all file systems */
void EEFS_LibInit(void)
{
    EEFS_FileDescriptorsInUse = 0;
    EEFS_FileDescriptorsHighWaterMark = 0;
    memset(EEFS_FileDescriptorTable, 0, sizeof(EEFS_FileDescriptorTable));
    memset(&EEFS_DirectoryDescriptor, 0, sizeof(EEFS_DirectoryDescriptor_t));
    memset(&EEFS_DirectoryEntry, 0, sizeof(EEFS_DirectoryEntry_t));

} /* End of EEFS_LibInit() */

/* Initializes the Inode Table.  Returns EEFS_SUCCESS on success, EEFS_NO_SUCH_DEVICE or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibInitFS(EEFS_InodeTable_t *InodeTable, uint32 BaseAddress)
{
    EEFS_FileAllocationTable_t         *FileAllocationTable;
    EEFS_FileAllocationTableHeader_t    FileAllocationTableHeader;
    EEFS_FileAllocationTableEntry_t     FileAllocationTableEntry;
    uint32                              i;
    int32                               ReturnCode;
  
    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        /* Load the File Allocation Table Header from EEPROM */
        FileAllocationTable = (void *)BaseAddress;
        EEFS_LIB_EEPROM_READ(&FileAllocationTableHeader, &FileAllocationTable->Header, sizeof(EEFS_FileAllocationTableHeader_t));
        EEFS_SwapFileAllocationTableHeader(&FileAllocationTableHeader);/*APC */
        if ((FileAllocationTableHeader.Magic == EEFS_FILESYS_MAGIC) &&
            (FileAllocationTableHeader.Version == 1) &&
            (FileAllocationTableHeader.NumberOfFiles <= EEFS_MAX_FILES)) {

            /* Initialize the Inode Table */
            memset(InodeTable, 0, sizeof(EEFS_InodeTable_t));
            InodeTable->BaseAddress = BaseAddress;
            InodeTable->FreeMemoryPointer = (void *)(InodeTable->BaseAddress + FileAllocationTableHeader.FreeMemoryOffset);
            InodeTable->FreeMemorySize = FileAllocationTableHeader.FreeMemorySize;
            InodeTable->NumberOfFiles = FileAllocationTableHeader.NumberOfFiles;
            for (i=0; i < InodeTable->NumberOfFiles; i++) {
                EEFS_LIB_EEPROM_READ(&FileAllocationTableEntry, &FileAllocationTable->File[i], sizeof(EEFS_FileAllocationTableEntry_t));
                EEFS_SwapFileAllocationTableEntry(&FileAllocationTableEntry); /* APC */
                InodeTable->File[i].FileHeaderPointer = (void *)(BaseAddress + FileAllocationTableEntry.FileHeaderOffset);
                InodeTable->File[i].MaxFileSize = FileAllocationTableEntry.MaxFileSize;
            }
            ReturnCode = EEFS_SUCCESS;
        }
        else { /* invalid file allocation table */
            ReturnCode = EEFS_NO_SUCH_DEVICE;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibInitFS() */

/* Clears the Inode Table.  Returns EEFS_SUCCESS on success, EEFS_DEVICE_IS_BUSY or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibFreeFS(EEFS_InodeTable_t *InodeTable)
{
    int32       ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if ((EEFS_LibHasOpenFiles(InodeTable) == FALSE) &&
            (EEFS_LibHasOpenDir(InodeTable) == FALSE)) {

            memset(InodeTable, 0, sizeof(EEFS_InodeTable_t));
            ReturnCode = EEFS_SUCCESS;
        }
        else { /* files or directory descriptor open */
            ReturnCode = EEFS_DEVICE_IS_BUSY;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibFreeFS() */

/* Opens the specified file for read or write access.  This function supports the following Flags (O_RDONLY, O_WRONLY,
 * O_RDWR, O_TRUNC, O_CREAT).  Files can always be opened for shared read access, however files cannot be opened more than 
 * once for shared write access.  Returns a file descriptor on success, EEFS_NO_FREE_FILE_DESCRIPTOR, EEFS_PERMISSION_DENIED,
 * EEFS_INVALID_ARGUMENT, or EEFS_FILE_NOT_FOUND on error. */
int32 EEFS_LibOpen(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Flags, uint32 Attributes)
{
    int32                           InodeIndex;
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if (EEFS_LibIsValidFilename(Filename)) {

            if ((InodeIndex = EEFS_LibFindFile(InodeTable, Filename)) != EEFS_FILE_NOT_FOUND) {
                
                ReturnCode = EEFS_LibOpenFile(InodeTable, InodeIndex, Flags, Attributes);
            }
            else if (Flags & O_CREAT) {
            
                ReturnCode = EEFS_LibCreatFile(InodeTable, Filename, EEFS_ATTRIBUTE_NONE);
            }           
            else { /* file not found */
            
                ReturnCode = EEFS_FILE_NOT_FOUND;
            }
        }
        else { /* invalid filename size */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibOpen() */

/* Creates a new file and opens it for writing.  If the file already exists then the existing file is opened for write
 * access and the file is truncated.  If the file does not already exist then a new file is created.  Since we don't know
 * the size of the file yet all remaining free eeprom is allocated for the new file. When the file is closed then the
 * MaxFileSize is updated to be the actual size of the file + EEFS_DEFAULT_CREAT_SPARE_BYTES.  Note that since all free eeprom
 * is allocated for the file while it is open, only one new file can be created at a time. Returns a file descriptor on
 * success, EEFS_NO_FREE_FILE_DESCRIPTOR, EEFS_PERMISSION_DENIED, EEFS_INVALID_ARGUMENT, or EEFS_NO_SPACE_LEFT_ON_DEVICE
 * on error.*/
int32 EEFS_LibCreat(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes)
{
    int32                               InodeIndex;
    int32                               ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if (EEFS_LibIsValidFilename(Filename)) {

            /* If the file already exists then open it for write access otherwise create a new file */
            if ((InodeIndex = EEFS_LibFindFile(InodeTable, Filename)) != EEFS_FILE_NOT_FOUND) {

                ReturnCode = EEFS_LibOpenFile(InodeTable, InodeIndex, (O_WRONLY | O_TRUNC), Attributes);
            }
            else {

                ReturnCode = EEFS_LibCreatFile(InodeTable, Filename, Attributes);
            }
        }
        else { /* filename too long */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibCreat() */

/* Internal function to open a file. */
int32 EEFS_LibOpenFile(EEFS_InodeTable_t *InodeTable, int32 InodeIndex, uint32 Flags, uint32 Attributes)
{
    int32                           FileDescriptor;
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;
    uint32                          Fmode;

    (void)Attributes;  /* Unsupported at this time */

    /* Verify that the Flags field does not contain any unsupported options */
    if ((Flags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_TRUNC | O_CREAT)) == 0) { 

        /* Don't allow the file to be opened for write if the file system is write protected */
        if (((Flags & O_ACCMODE) == O_RDONLY) ||                             /* open only for reading OR */
             (EEFS_LIB_IS_WRITE_PROTECTED == FALSE)) {                          /* open for writing and the file system is not write protected */

            /* Don't allow the file to be opened for write if it has the read only attribute set */
            EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            if (((Flags & O_ACCMODE) == O_RDONLY) ||                         /* open only for reading OR */
                 (FileHeader.Attributes & EEFS_ATTRIBUTE_READONLY) == 0) {   /* open for writing and read only file attribute not set */

                /* This always allows the file to be opened for read only access, however it does not allow the
                 * file to be opened for shared write access */
                Fmode = EEFS_LibFmode(InodeTable, InodeIndex);
                if (((Flags & O_ACCMODE) == O_RDONLY) ||                     /* open only for reading OR */
                     (Fmode & EEFS_FWRITE) == 0) {                           /* open for writing and file is not already open for writing */

                    if ((FileDescriptor = EEFS_LibGetFileDescriptor()) != EEFS_NO_FREE_FILE_DESCRIPTOR) {

                        /* Initialize the File Descriptor */
                        EEFS_FileDescriptorTable[FileDescriptor].Mode = (Flags & O_ACCMODE) + 1;
                        EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer = InodeTable->File[InodeIndex].FileHeaderPointer;
                        EEFS_FileDescriptorTable[FileDescriptor].MaxFileSize = InodeTable->File[InodeIndex].MaxFileSize;
                        EEFS_FileDescriptorTable[FileDescriptor].InodeTable = InodeTable;
                        EEFS_FileDescriptorTable[FileDescriptor].InodeIndex = InodeIndex;

                        if ((((Flags & O_ACCMODE) == O_WRONLY) ||
                             ((Flags & O_ACCMODE) == O_RDWR))  &&
                              (Flags & O_TRUNC)) {
                            EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = InodeTable->File[InodeIndex].FileHeaderPointer + sizeof(EEFS_FileHeader_t);
                            EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = 0;
                            EEFS_FileDescriptorTable[FileDescriptor].FileSize = 0;
                        }
                        else {
                            EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = InodeTable->File[InodeIndex].FileHeaderPointer + sizeof(EEFS_FileHeader_t);
                            EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = 0;
                            EEFS_FileDescriptorTable[FileDescriptor].FileSize = FileHeader.FileSize;
                        }

                        /* Return the File Descriptor */
                        ReturnCode = FileDescriptor;
                    }
                    else { /* no available file descriptor */
                        ReturnCode = EEFS_NO_FREE_FILE_DESCRIPTOR;
                    }
                }
                else { /* file is already open for write */
                    ReturnCode = EEFS_PERMISSION_DENIED;
                }
            }
            else { /* file is read only */
                ReturnCode = EEFS_PERMISSION_DENIED;
            }
        }
        else { /* file system is write protected */
            ReturnCode = EEFS_READ_ONLY_FILE_SYSTEM;
        }
    } /* Unsupported Flag */
    else {
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    return(ReturnCode);
    
} /* End of EEFS_LibOpenFile() */

/* Internal function to create a new file */
int32 EEFS_LibCreatFile(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes)
{

    EEFS_FileHeader_t                   FileHeader;
    int32                               InodeIndex;
    int32                               FileDescriptor;
    int32                               ReturnCode;

    /* If the file system is not write protected */
    if (EEFS_LIB_IS_WRITE_PROTECTED == FALSE) {

        /* Make sure there is a free slot in the File Allocation Table */
        if (InodeTable->NumberOfFiles < EEFS_MAX_FILES) {

            /* Since we use all available free memory when creating a new file we can only create one new file at
             * a time */
            if (EEFS_LibHasOpenCreat(InodeTable) == FALSE) {

                /* Make sure there is enough room in eeprom for at least a file header */
                if (InodeTable->FreeMemorySize > sizeof(EEFS_FileHeader_t)) {

                    if ((Attributes == EEFS_ATTRIBUTE_NONE) || (Attributes == EEFS_ATTRIBUTE_READONLY)) {

                        if ((FileDescriptor = EEFS_LibGetFileDescriptor()) != EEFS_NO_FREE_FILE_DESCRIPTOR) {

                            /* Add the new entry to the InodeTable.  Temporarily set the MaxFileSize equal to all free eeprom.
                             * The FreeMemoryPointer and the FreeMemorySize variables are NOT updated until the file is
                             * closed and the actual file size is known.  Setting EEFS_FCREAT in the Mode variable
                             * will prevent any other file creations until the current one is complete and the FreeMemoryPointer 
                             * and the FreeMemorySize variables are updated. */
                            InodeIndex = InodeTable->NumberOfFiles;
                            InodeTable->NumberOfFiles++;
                            InodeTable->File[InodeIndex].FileHeaderPointer = InodeTable->FreeMemoryPointer;
                            InodeTable->File[InodeIndex].MaxFileSize = (InodeTable->FreeMemorySize - sizeof(EEFS_FileHeader_t));

                            /* Initialize a new File Header and write it to EEPROM*/
                            FileHeader.Crc = 0;    /* Automatically updating the CRC is not supported at this time */
                            FileHeader.InUse = TRUE;
                            FileHeader.Attributes = Attributes;
                            FileHeader.FileSize = 0;
                            FileHeader.ModificationDate = EEFS_LIB_TIME;
                            FileHeader.CreationDate = FileHeader.ModificationDate;
                            strncpy(FileHeader.Filename, Filename, EEFS_MAX_FILENAME_SIZE);
                            EEFS_LIB_EEPROM_WRITE(InodeTable->File[InodeIndex].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
                            EEFS_LIB_EEPROM_FLUSH;

                            /* Initialize the File Descriptor */
                            EEFS_FileDescriptorTable[FileDescriptor].Mode = (EEFS_FCREAT | EEFS_FWRITE);
                            EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer = InodeTable->File[InodeIndex].FileHeaderPointer;
                            EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = InodeTable->File[InodeIndex].FileHeaderPointer + sizeof(EEFS_FileHeader_t);
                            EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = 0;
                            EEFS_FileDescriptorTable[FileDescriptor].FileSize = 0;
                            EEFS_FileDescriptorTable[FileDescriptor].MaxFileSize = InodeTable->File[InodeIndex].MaxFileSize;
                            EEFS_FileDescriptorTable[FileDescriptor].InodeTable = InodeTable;
                            EEFS_FileDescriptorTable[FileDescriptor].InodeIndex = InodeIndex;

                            /* Return the File Descriptor */
                            ReturnCode = FileDescriptor;
                        }
                        else { /* no available file descriptor */
                            ReturnCode = EEFS_NO_FREE_FILE_DESCRIPTOR;
                        }
                    }
                    else { /* invalid attributes */
                        ReturnCode = EEFS_INVALID_ARGUMENT;
                    }
                }
                else { /* not enough free space in eeprom */
                    ReturnCode = EEFS_NO_SPACE_LEFT_ON_DEVICE;
                }
            }
            else { /* a file creat is already in progress */
                ReturnCode = EEFS_PERMISSION_DENIED;
            }
        }
        else { /* no available slots in the File Allocation Table */
            ReturnCode = EEFS_NO_SPACE_LEFT_ON_DEVICE;
        }
    }
    else { /* file system is write protected */
        ReturnCode = EEFS_READ_ONLY_FILE_SYSTEM;
    }

    return(ReturnCode);
    
} /* End of EEFS_LibCreatFile() */

/* Closes a file.  Returns EEFS_SUCCESS on success or EEFS_INVALID_ARGUMENT on error.  If a new file is being created then the
 * MaxFileSize is updated to be the actual size of the file + EEFS_DEFAULT_CREAT_SPARE_BYTES. Note that the File Allocation
 * Table is not updated until the file is closed to reduce the number of EEPROM Writes. */
int32 EEFS_LibClose(int32 FileDescriptor)
{
    EEFS_FileAllocationTable_t         *FileAllocationTable;
    EEFS_FileAllocationTableHeader_t    FileAllocationTableHeader;
    EEFS_FileAllocationTableEntry_t     FileAllocationTableEntry;
    EEFS_FileHeader_t                   FileHeader;
    uint32                              MaxFileSize;
    EEFS_InodeTable_t                  *InodeTable;
    uint32                              InodeIndex;
    int32                               ReturnCode;

    EEFS_LIB_LOCK;
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {

        /* Note that both the EEFS_FCREAT and EEFS_FWRITE bits are set when a new file is created, so we check for the
         * EEFS_CREAT bit first */
        if (EEFS_FileDescriptorTable[FileDescriptor].Mode & EEFS_FCREAT) {

            InodeTable = EEFS_FileDescriptorTable[FileDescriptor].InodeTable;
            InodeIndex = EEFS_FileDescriptorTable[FileDescriptor].InodeIndex;

            /* Calculate the New MaxFileSize and round it up to a 4 byte boundary */
            MaxFileSize = EEFS_ROUND_UP((EEFS_FileDescriptorTable[FileDescriptor].FileSize + EEFS_DEFAULT_CREAT_SPARE_BYTES), 4);

            /* Make sure since we added some spare bytes to the end of the file we do not exceed the free memory size */
            MaxFileSize = EEFS_MIN(MaxFileSize, (InodeTable->FreeMemorySize - sizeof(EEFS_FileHeader_t)));

            /* Update the Inode Table with the new MaxFileSize */
            InodeTable->FreeMemoryPointer += (sizeof(EEFS_FileHeader_t) + MaxFileSize);
            InodeTable->FreeMemorySize -= (sizeof(EEFS_FileHeader_t) + MaxFileSize);
            InodeTable->File[InodeIndex].MaxFileSize = MaxFileSize;
            
            /* Update the File Header */
            EEFS_LIB_EEPROM_READ(&FileHeader, EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            FileHeader.FileSize = EEFS_FileDescriptorTable[FileDescriptor].FileSize;
            FileHeader.Crc = 0;      /* Automatically updating the CRC is not supported at this time */
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            EEFS_LIB_EEPROM_WRITE(EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
            EEFS_LIB_EEPROM_FLUSH;

            /* Add the new entry to the File Allocation Table */
            FileAllocationTable = (void *)InodeTable->BaseAddress;

            FileAllocationTableEntry.FileHeaderOffset = (uint32)(InodeTable->File[InodeIndex].FileHeaderPointer - InodeTable->BaseAddress);
            FileAllocationTableEntry.MaxFileSize = InodeTable->File[InodeIndex].MaxFileSize;
            EEFS_SwapFileAllocationTableEntry(&FileAllocationTableEntry); /* APC */
            EEFS_LIB_EEPROM_WRITE(&FileAllocationTable->File[InodeIndex], &FileAllocationTableEntry, sizeof(EEFS_FileAllocationTableEntry_t));
            EEFS_LIB_EEPROM_FLUSH;

            /* This is done last to reduce the chance that a reset during a file creat will cause the file system to be corrupted.  If a 
               reset occurs the new file will not exist in the file system until the following lines of code are executed. */
            EEFS_LIB_EEPROM_READ(&FileAllocationTableHeader, &FileAllocationTable->Header, sizeof(EEFS_FileAllocationTableHeader_t));
            EEFS_SwapFileAllocationTableHeader(&FileAllocationTableHeader);/*APC */
            FileAllocationTableHeader.FreeMemoryOffset = (uint32)(InodeTable->FreeMemoryPointer - InodeTable->BaseAddress);
            FileAllocationTableHeader.FreeMemorySize = InodeTable->FreeMemorySize;
            FileAllocationTableHeader.NumberOfFiles = InodeTable->NumberOfFiles;
            EEFS_SwapFileAllocationTableHeader(&FileAllocationTableHeader);/*APC */
            EEFS_LIB_EEPROM_WRITE(&FileAllocationTable->Header, &FileAllocationTableHeader, sizeof(EEFS_FileAllocationTableHeader_t));
            EEFS_LIB_EEPROM_FLUSH;
        }
        else if (EEFS_FileDescriptorTable[FileDescriptor].Mode & EEFS_FWRITE) {
            
            /* Update the File Header */
            EEFS_LIB_EEPROM_READ(&FileHeader, EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            FileHeader.FileSize = EEFS_FileDescriptorTable[FileDescriptor].FileSize;
            FileHeader.ModificationDate = EEFS_LIB_TIME;
            FileHeader.Crc = 0;   /* Automatically updating the CRC is not supported at this time */
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            EEFS_LIB_EEPROM_WRITE(EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
            EEFS_LIB_EEPROM_FLUSH;
        }

        EEFS_LibFreeFileDescriptor(FileDescriptor);
        ReturnCode = EEFS_SUCCESS;
    }
    else { /* invalid file descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibClose() */

/* Read from a file.  Returns the number of bytes read, 0 bytes if we have reached the end of file, or EEFS_INVALID_ARGUMENT
 * on error. */
int32 EEFS_LibRead(int32 FileDescriptor, void *Buffer, uint32 Length)
{
    uint32      BytesToRead;
    int32       ReturnCode;

    EEFS_LIB_LOCK;
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {

        if (Buffer != NULL) {
            
            if (EEFS_FileDescriptorTable[FileDescriptor].Mode & EEFS_FREAD) {

                BytesToRead = EEFS_MIN((EEFS_FileDescriptorTable[FileDescriptor].FileSize - EEFS_FileDescriptorTable[FileDescriptor].ByteOffset), Length);
                EEFS_LIB_EEPROM_READ(Buffer, EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer, BytesToRead);
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer += BytesToRead;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset += BytesToRead;
                ReturnCode = BytesToRead;
            }
            else { /* file not open for reading */
                ReturnCode = EEFS_PERMISSION_DENIED;
            }
        }
        else { /* invalid buffer pointer */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid file descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibRead() */

/* Write to a file.  Returns the number of bytes written, 0 bytes if we have run out of memory or EEFS_INVALID_ARGUMENT
 * on error. */
int32 EEFS_LibWrite(int32 FileDescriptor, void *Buffer, uint32 Length)
{
    uint32      BytesToWrite;
    int32       ReturnCode;

    EEFS_LIB_LOCK;
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {

        if (Buffer != NULL) {
            
            if (EEFS_FileDescriptorTable[FileDescriptor].Mode & EEFS_FWRITE) {

                BytesToWrite = EEFS_MIN((EEFS_FileDescriptorTable[FileDescriptor].MaxFileSize - EEFS_FileDescriptorTable[FileDescriptor].ByteOffset), Length);
                EEFS_LIB_EEPROM_WRITE((void *)EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer, Buffer, BytesToWrite);
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer += BytesToWrite;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset += BytesToWrite;
                if (EEFS_FileDescriptorTable[FileDescriptor].ByteOffset > EEFS_FileDescriptorTable[FileDescriptor].FileSize) {
                    EEFS_FileDescriptorTable[FileDescriptor].FileSize = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
                }
                ReturnCode = BytesToWrite;
            }
            else { /* file not open for writing */
                ReturnCode = EEFS_PERMISSION_DENIED;
            }
        }
        else { /* invalid buffer pointer */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid file descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }   

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibWrite() */

/* Set the file pointer to a specific offset in the file.  This implementation does not support seeking beyond the end of a file.  
 * If a ByteOffset is specified that is beyond the end of the file then the file pointer is set to the end of the file.  If 
 * a ByteOffset is specified that is less than the start of the file then an error is returned.  Returns the current file pointer 
 * on success, or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibLSeek(int32 FileDescriptor, int32 ByteOffset, uint16 Origin)
{
    void       *BeginningOfFilePointer;
    void       *EndOfFilePointer;
    int32       ReturnCode;

    EEFS_LIB_LOCK;
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {

        BeginningOfFilePointer = EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer + sizeof(EEFS_FileHeader_t);
        EndOfFilePointer = BeginningOfFilePointer + EEFS_FileDescriptorTable[FileDescriptor].FileSize;

        if (Origin == SEEK_SET) {

            if (ByteOffset < 0) {
                ReturnCode = EEFS_INVALID_ARGUMENT;
            }
            else if (ByteOffset > (int32)EEFS_FileDescriptorTable[FileDescriptor].FileSize) {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = EndOfFilePointer;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = EEFS_FileDescriptorTable[FileDescriptor].FileSize;
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
            else {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = (BeginningOfFilePointer + ByteOffset);
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = ByteOffset;
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
        }
        else if (Origin == SEEK_CUR) {

            if ((int32)(ByteOffset + EEFS_FileDescriptorTable[FileDescriptor].ByteOffset) < 0) {
                ReturnCode = EEFS_INVALID_ARGUMENT;
            }
            else if ((int32)(ByteOffset + EEFS_FileDescriptorTable[FileDescriptor].ByteOffset) > (int32)EEFS_FileDescriptorTable[FileDescriptor].FileSize) {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = EndOfFilePointer;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = EEFS_FileDescriptorTable[FileDescriptor].FileSize;
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
            else {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer += ByteOffset;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset += ByteOffset;
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
        }
        else if (Origin == SEEK_END) {

            if ((int32)(ByteOffset + EEFS_FileDescriptorTable[FileDescriptor].FileSize) < 0) {
                ReturnCode = EEFS_INVALID_ARGUMENT;
            }
            else if (ByteOffset > 0) {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = EndOfFilePointer;
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = EEFS_FileDescriptorTable[FileDescriptor].FileSize;                
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
            else {
                EEFS_FileDescriptorTable[FileDescriptor].FileDataPointer = (EndOfFilePointer + ByteOffset);
                EEFS_FileDescriptorTable[FileDescriptor].ByteOffset = (EEFS_FileDescriptorTable[FileDescriptor].FileSize + ByteOffset);
                ReturnCode = EEFS_FileDescriptorTable[FileDescriptor].ByteOffset;
            }
        }
        else { /* invalid Origin */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid file descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibLSeek() */

/* Removes the specified file from the file system.  Note that this just marks the file as deleted and does not free the memory
 * in use by the file.  Once a file is deleted, the only way the slot can be reused is to manually write a new file into the
 * slot, i.e. there is no way to reuse the memory through a EEFS api function.  Returns a file descriptor on success,
 * EEFS_PERMISSION_DENIED, EEFS_FILE_NOT_FOUND or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibRemove(EEFS_InodeTable_t *InodeTable, char *Filename)
{
    int32                           InodeIndex;
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;
    
    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if (EEFS_LibIsValidFilename(Filename)) {

            /* If the file system is not write protected */
            if (EEFS_LIB_IS_WRITE_PROTECTED == FALSE) {

                if ((InodeIndex = EEFS_LibFindFile(InodeTable, Filename)) != EEFS_FILE_NOT_FOUND) {

                    /* Can't delete a read only file */
                    EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
                    EEFS_SwapFileHeader(&FileHeader); /* APC */
                    if ((FileHeader.Attributes & EEFS_ATTRIBUTE_READONLY) == 0) {

                        /* Does the file have any open file descriptors */
                        if (EEFS_LibFmode(InodeTable, InodeIndex) == 0) {

                            memset(&FileHeader, 0, sizeof(EEFS_FileHeader_t)); /* clears the InUse flag marking the file deleted */
                            EEFS_SwapFileHeader(&FileHeader); /* APC */
                            EEFS_LIB_EEPROM_WRITE(InodeTable->File[InodeIndex].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
                            EEFS_LIB_EEPROM_FLUSH;
                            ReturnCode = EEFS_SUCCESS;
                        }
                        else { /* error file is open */
                            ReturnCode = EEFS_PERMISSION_DENIED;
                       }
                    }
                    else { /* error read only file */
                        ReturnCode = EEFS_PERMISSION_DENIED;
                    }
                }
                else { /* file not found */
                    ReturnCode = EEFS_FILE_NOT_FOUND;
                }
            }
            else { /* file system is write protected */
                ReturnCode = EEFS_READ_ONLY_FILE_SYSTEM;
            }
        }
        else { /* invalid filename size */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibRemove() */

/* Renames the specified file.  Returns EEFS_SUCCESS on success, EEFS_PERMISSION_DENIED, EEFS_FILE_NOT_FOUND,
 * or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibRename(EEFS_InodeTable_t *InodeTable, char *OldFilename, char *NewFilename)
{
    int32                           InodeIndex;
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if ((EEFS_LibIsValidFilename(OldFilename)) &&
            (EEFS_LibIsValidFilename(NewFilename))) {

            /* If the file system is not write protected */
            if (EEFS_LIB_IS_WRITE_PROTECTED == FALSE) {

                if ((EEFS_LibFindFile(InodeTable, NewFilename)) == EEFS_FILE_NOT_FOUND) {

                    if ((InodeIndex = EEFS_LibFindFile(InodeTable, OldFilename)) != EEFS_FILE_NOT_FOUND) {

                        /* Can't rename a read only file */
                        EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
                         EEFS_SwapFileHeader(&FileHeader); /* APC */
                        if ((FileHeader.Attributes & EEFS_ATTRIBUTE_READONLY) == 0) {

                            strncpy(FileHeader.Filename, NewFilename, EEFS_MAX_FILENAME_SIZE);
                            EEFS_SwapFileHeader(&FileHeader); /* APC */
                            EEFS_LIB_EEPROM_WRITE(InodeTable->File[InodeIndex].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
                            EEFS_LIB_EEPROM_FLUSH;
                            ReturnCode = EEFS_SUCCESS;
                        }
                        else { /* error read only file */
                            ReturnCode = EEFS_PERMISSION_DENIED;
                        }
                    }
                    else { /* file not found */
                        ReturnCode = EEFS_FILE_NOT_FOUND;
                    }
                }
                else { /* new filename already exists */
                    ReturnCode = EEFS_PERMISSION_DENIED;
                }
            }
            else { /* file system is write protected */
                ReturnCode = EEFS_READ_ONLY_FILE_SYSTEM;
            }                
        }
        else { /* invalid filename size */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibRename() */

/* Returns file information for the specified filename in StatBuffer.  Returns EEFS_SUCCESS on success, EEFS_FILE_NOT_FOUND,
 * or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibStat(EEFS_InodeTable_t *InodeTable, char *Filename, EEFS_Stat_t *StatBuffer)
{
    int32                           InodeIndex;
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL)  {

        if (StatBuffer != NULL) {

            if (EEFS_LibIsValidFilename(Filename)) {

                if ((InodeIndex = EEFS_LibFindFile(InodeTable, Filename)) != EEFS_FILE_NOT_FOUND) {

                    EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
                    EEFS_SwapFileHeader(&FileHeader); /* APC */
                    StatBuffer->InodeIndex = InodeIndex;
                    StatBuffer->Attributes = FileHeader.Attributes;
                    StatBuffer->FileSize = FileHeader.FileSize;
                    StatBuffer->ModificationDate = FileHeader.ModificationDate;
                    StatBuffer->CreationDate = FileHeader.CreationDate;
                    StatBuffer->Crc = FileHeader.Crc;
                    strncpy(StatBuffer->Filename, FileHeader.Filename, EEFS_MAX_FILENAME_SIZE);
                    ReturnCode = EEFS_SUCCESS;
                }
                else { /* file not found */
                    ReturnCode = EEFS_FILE_NOT_FOUND;
                }
            }
            else { /* invalid filename */
                ReturnCode = EEFS_INVALID_ARGUMENT;
            }
        }
        else { /* invalid stat buffer */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibStat() */

/* Returns file information for the specified file descriptor in StatBuffer.  Returns EEFS_SUCCESS on success or 
 * EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibFstat(int32 FileDescriptor, EEFS_Stat_t *StatBuffer)
{
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {
    
        if (StatBuffer != NULL) {

            EEFS_LIB_EEPROM_READ(&FileHeader, EEFS_FileDescriptorTable[FileDescriptor].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            StatBuffer->InodeIndex = EEFS_FileDescriptorTable[FileDescriptor].InodeIndex;
            StatBuffer->Attributes = FileHeader.Attributes;
            StatBuffer->FileSize = FileHeader.FileSize;
            StatBuffer->ModificationDate = FileHeader.ModificationDate;
            StatBuffer->CreationDate = FileHeader.CreationDate;
            StatBuffer->Crc = FileHeader.Crc;
            strncpy(StatBuffer->Filename, FileHeader.Filename, EEFS_MAX_FILENAME_SIZE);
            ReturnCode = EEFS_SUCCESS;
        }
        else { /* invalid stat buffer */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid file descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibFstat() */

/* Sets the Attributes for the specified file. Currently the only attribute that is supported is the EEFS_ATTRIBUTE_READONLY
 * attribute.  Returns EEFS_SUCCESS on success, EEFS_FILE_NOT_FOUND or EEFS_INVALID_ARGUMENT on error.  To read file
 * attributes use the stat function. */
int32 EEFS_LibSetFileAttributes(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes)
{
    int32                           InodeIndex;
    EEFS_FileHeader_t               FileHeader;
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL)  {

        if (Attributes == EEFS_ATTRIBUTE_NONE || Attributes == EEFS_ATTRIBUTE_READONLY) {

            if (EEFS_LibIsValidFilename(Filename)) {

                /* If the file system is not write protected */
                if (EEFS_LIB_IS_WRITE_PROTECTED == FALSE) {

                    if ((InodeIndex = EEFS_LibFindFile(InodeTable, Filename)) != EEFS_FILE_NOT_FOUND) {

                        EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
                        EEFS_SwapFileHeader(&FileHeader); /* APC */
                        FileHeader.Attributes = Attributes;
                        EEFS_SwapFileHeader(&FileHeader); /* APC */
                        EEFS_LIB_EEPROM_WRITE(InodeTable->File[InodeIndex].FileHeaderPointer, &FileHeader, sizeof(EEFS_FileHeader_t));
                        EEFS_LIB_EEPROM_FLUSH;
                        ReturnCode = EEFS_SUCCESS;
                    }
                    else { /* file not found */
                        ReturnCode = EEFS_FILE_NOT_FOUND;
                    }
                }
                else { /* file system is write protected */
                    ReturnCode = EEFS_READ_ONLY_FILE_SYSTEM;
                }
            }
            else { /* invalid filename */
                ReturnCode = EEFS_INVALID_ARGUMENT;
            }
        }
        else { /* invalid attributes */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid inode table */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode); 
    
} /* End of EEFS_LibSetFileAttributes() */

/* Opens a file system for reading the file directory.  This should be followed by calls to EEFS_ReadDir() and EEFS_CloseDir().
 * Note that currently only one process can read the file directory at a time.  Returns a pointer to a directory descriptor
 * on success and a NULL pointer on error. */
EEFS_DirectoryDescriptor_t *EEFS_LibOpenDir(EEFS_InodeTable_t *InodeTable)
{
    EEFS_DirectoryDescriptor_t      *DirectoryDescriptor;

    EEFS_LIB_LOCK;
    if (InodeTable != NULL) {

        if (EEFS_DirectoryDescriptor.InUse == FALSE) {

            EEFS_DirectoryDescriptor.InUse = TRUE;
            EEFS_DirectoryDescriptor.InodeIndex = 0;
            EEFS_DirectoryDescriptor.InodeTable = InodeTable;
            DirectoryDescriptor = &EEFS_DirectoryDescriptor;
        }
        else { /* no available directory descriptors */
            DirectoryDescriptor = NULL;
        }
    }
    else { /* invalid inode table */
        DirectoryDescriptor = NULL;
    }

    EEFS_LIB_UNLOCK;
    return(DirectoryDescriptor);
    
} /* End of EEFS_LibOpenDir() */

/* Read the next file directory entry.  Returns a pointer to a EEFS_DirectoryEntry_t if successful or NULL if no more file
 * directory entries exist or an error occurs.  Note that all entries are returned, even empty slots. (The InUse flag will be
 * set to FALSE for empty slots) */
EEFS_DirectoryEntry_t *EEFS_LibReadDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor)
{
    EEFS_FileHeader_t               FileHeader;
    EEFS_DirectoryEntry_t          *DirectoryEntry;

    EEFS_LIB_LOCK;
    if (DirectoryDescriptor != NULL) {

        if (DirectoryDescriptor->InodeIndex < DirectoryDescriptor->InodeTable->NumberOfFiles) {
            
            EEFS_LIB_EEPROM_READ(&FileHeader, DirectoryDescriptor->InodeTable->File[DirectoryDescriptor->InodeIndex].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
            EEFS_SwapFileHeader(&FileHeader); /* APC */
            EEFS_DirectoryEntry.InodeIndex = DirectoryDescriptor->InodeIndex;
            EEFS_DirectoryEntry.FileHeaderPointer = DirectoryDescriptor->InodeTable->File[DirectoryDescriptor->InodeIndex].FileHeaderPointer;
            EEFS_DirectoryEntry.MaxFileSize = DirectoryDescriptor->InodeTable->File[DirectoryDescriptor->InodeIndex].MaxFileSize;
            EEFS_DirectoryEntry.InUse = FileHeader.InUse;
            strncpy(EEFS_DirectoryEntry.Filename, FileHeader.Filename, EEFS_MAX_FILENAME_SIZE);
            DirectoryDescriptor->InodeIndex++;
            DirectoryEntry = &EEFS_DirectoryEntry;
        }
        else { /* no more directory entries */
            DirectoryEntry = NULL;
        }
    }
    else { /* invalid directory descriptor */
        DirectoryEntry = NULL;
    }

    EEFS_LIB_UNLOCK;
    return(DirectoryEntry);
    
} /* End of EEFS_LibReadDir() */

/* Close file system for reading the file directory.  Returns EEFS_SUCCESS on success or EEFS_INVALID_ARGUMENT on error. */
int32 EEFS_LibCloseDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor)
{
    int32                           ReturnCode;

    EEFS_LIB_LOCK;
    if (DirectoryDescriptor != NULL) {

        if (DirectoryDescriptor->InUse == TRUE) {

            memset(DirectoryDescriptor, 0, sizeof(EEFS_DirectoryDescriptor_t)); /* this sets InUse to FALSE */
            memset(&EEFS_DirectoryEntry, 0, sizeof(EEFS_DirectoryEntry_t));
            ReturnCode = EEFS_SUCCESS;
        }
        else { /* directory descriptor not in use */
            ReturnCode = EEFS_INVALID_ARGUMENT;
        }
    }
    else { /* invalid directory descriptor */
        ReturnCode = EEFS_INVALID_ARGUMENT;
    }

    EEFS_LIB_UNLOCK;
    return(ReturnCode);
    
} /* End of EEFS_LibCloseDir() */

/* Returns TRUE if any files in the file system are open. */
uint8 EEFS_LibHasOpenFiles(EEFS_InodeTable_t *InodeTable)
{
    uint32  i;

    for (i=0; i < EEFS_MAX_OPEN_FILES; i++) {
        if ((EEFS_FileDescriptorTable[i].InUse == TRUE) &&
            (EEFS_FileDescriptorTable[i].InodeTable == InodeTable)) {
            return(TRUE); /* a file is open */
        }
    }
    return(FALSE);
    
} /* End of EEFS_LibHasOpenFiles() */

/* Returns TRUE if a directory is open. */
uint8 EEFS_LibHasOpenDir(EEFS_InodeTable_t *InodeTable)
{
    if ((EEFS_DirectoryDescriptor.InUse == TRUE) &&
        (EEFS_DirectoryDescriptor.InodeTable == InodeTable)) {
        return(TRUE); /* a directory is open */
    }
    return(FALSE);
    
} /* End of EEFS_LibHasOpenDir() */

/* Returns TRUE if any files in the file system are open for create. */
uint8 EEFS_LibHasOpenCreat(EEFS_InodeTable_t *InodeTable)
{
    uint32  i;

    for (i=0; i < EEFS_MAX_OPEN_FILES; i++) {
        if ((EEFS_FileDescriptorTable[i].InUse == TRUE) &&
            (EEFS_FileDescriptorTable[i].InodeTable == InodeTable) &&
            (EEFS_FileDescriptorTable[i].Mode & EEFS_FCREAT)) {
            return(TRUE); /* a file is open for creat */
        }
    }
    return(FALSE);
    
} /* End of EEFS_LibHasOpenCreat() */

/* Searches the file descriptor table for entries that match the specified file and 
 * or's together all of the file mode flags.  The return bit mask will be EEFS_FREAD if
 * the file is open for read access, EEFS_WRITE if the file is open for write access, 
 * (EEFS_FREAD | EEFS_FWRITE) if the file is open for both read and write access, or
 * EEFS_CREAT if a new file is being created. */
uint32 EEFS_LibFmode(EEFS_InodeTable_t *InodeTable, uint32 InodeIndex)
{
    uint32      i;
    uint32      Mode = 0;

    for (i=0; i < EEFS_MAX_OPEN_FILES; i++) {
        if ((EEFS_FileDescriptorTable[i].InUse == TRUE) &&
            (EEFS_FileDescriptorTable[i].InodeTable == InodeTable) &&
            (EEFS_FileDescriptorTable[i].InodeIndex == InodeIndex)) {
            Mode |= EEFS_FileDescriptorTable[i].Mode;
        }
    }
    return(Mode);
    
} /* End of EEFS_LibFmode() */

/* Performs a sequential search of the InodeTable looking for a matching Filename. */
int32 EEFS_LibFindFile(EEFS_InodeTable_t *InodeTable, char *Filename)
{
    uint32                          i;
    EEFS_FileHeader_t               FileHeader;

    for (i=0; i < InodeTable->NumberOfFiles; i++) {
        EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[i].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
        EEFS_SwapFileHeader(&FileHeader); /* APC */
        if ((FileHeader.InUse == TRUE) &&
            (strncmp(Filename, FileHeader.Filename, EEFS_MAX_FILENAME_SIZE) == 0))
            return(i);
    }
    return(EEFS_FILE_NOT_FOUND);
    
} /* End of EEFS_LibFindFile() */

/* Allocates a free entry in the FileDescriptorTable. */
int32 EEFS_LibGetFileDescriptor(void)
{
    uint32      i;
    for (i=0; i < EEFS_MAX_OPEN_FILES; i++) {
        if (EEFS_FileDescriptorTable[i].InUse == FALSE) {
            EEFS_FileDescriptorTable[i].InUse = TRUE;
            EEFS_FileDescriptorsInUse++;
            if (EEFS_FileDescriptorsInUse > EEFS_FileDescriptorsHighWaterMark) {
                EEFS_FileDescriptorsHighWaterMark = EEFS_FileDescriptorsInUse;
            }
            return(i);
        }
    }
    return(EEFS_NO_FREE_FILE_DESCRIPTOR);
    
} /* End of EEFS_LibGetFileDescriptor() */

/* Returns a entry to the FileDescriptorTable. */
int32 EEFS_LibFreeFileDescriptor(int32 FileDescriptor)
{
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor) == TRUE) {
        memset(&EEFS_FileDescriptorTable[FileDescriptor], 0, sizeof(EEFS_FileDescriptor_t)); /* This sets InUse to FALSE */
        EEFS_FileDescriptorsInUse--;
        return(EEFS_SUCCESS);
    }
    return(EEFS_INVALID_ARGUMENT);
    
} /* End of EEFS_LibFreeFileDescriptor() */

/* Returns TRUE is the specified File Descriptor is valid */
uint8 EEFS_LibIsValidFileDescriptor(int32 FileDescriptor)
{
    if ((FileDescriptor >= 0) &&
        (FileDescriptor < EEFS_MAX_OPEN_FILES) &&
        (EEFS_FileDescriptorTable[FileDescriptor].InUse == TRUE)) {
        return(TRUE);
    }
    else {
        return(FALSE);
    }
    
} /* End of EEFS_LibIsValidFileDescriptor() */

/* Returns a pointer to the specified File Descriptor, or NULL if the specified
 * File Descriptor is not valid */
EEFS_FileDescriptor_t *EEFS_LibFileDescriptor2Pointer(int32 FileDescriptor)
{
    if (EEFS_LibIsValidFileDescriptor(FileDescriptor)) {
        return(&EEFS_FileDescriptorTable[FileDescriptor]);
    }
    else {
        return(NULL);
    }
    
} /* End of EEFS_LibFileDescriptor2Pointer() */

/* Validates the specified Filename. Probably need to be more strict on what I allow. */
uint8 EEFS_LibIsValidFilename(char *Filename)
{
    if ((Filename != NULL) &&
        (strlen(Filename) < EEFS_MAX_FILENAME_SIZE) &&
        (strlen(Filename) > 0))
        return(TRUE);
    else
        return(FALSE);
        
} /* End of EEFS_LibIsValidFilename() */

/* Perform consistency checks on the file system.  At the moment all this does is dumps the inode table. */
int32 EEFS_LibChkDsk(EEFS_InodeTable_t *InodeTable, uint32 Flags)
{
    EEFS_FileHeader_t               FileHeader;
    uint32                          i;

/* validate fat */
/* validate checksum */
/* validate file checksums */
/* verify that eeprom matches ram */
/* verify file pointers and max file sizes */

    (void)Flags;

    /* Dump the Inode Table and File Headers */
    printf("Base Address:        %#lx\n", InodeTable->BaseAddress);
    printf("Free Memory Pointer: %#lx\n", (uint32)InodeTable->FreeMemoryPointer);
    printf("Free Memory Size:    %ld\n", InodeTable->FreeMemorySize);
    printf("Number Of Files:     %ld\n", InodeTable->NumberOfFiles);

    for (i=0; i < InodeTable->NumberOfFiles; i++) {

        printf("[%ld] FileHeaderPointer    %#lx\n", i, (uint32)InodeTable->File[i].FileHeaderPointer);
        printf("[%ld] Max File Size        %ld\n", i, InodeTable->File[i].MaxFileSize);

        EEFS_LIB_EEPROM_READ(&FileHeader, InodeTable->File[i].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
        EEFS_SwapFileHeader(&FileHeader); /* APC */
        printf("[%ld] Crc                  %#lx\n", i, FileHeader.Crc);
        printf("[%ld] InUse                %ld\n", i, FileHeader.InUse);
        printf("[%ld] Attributes           %#lx\n", i, FileHeader.Attributes);
        printf("[%ld] FileSize             %ld\n", i, FileHeader.FileSize);
        printf("[%ld] Modification Date    %ld\n", i, FileHeader.ModificationDate);
        printf("[%ld] Creation Date        %ld\n", i, FileHeader.CreationDate);
        printf("[%ld] Filename             %-40s\n", i, FileHeader.Filename);
    }
    
    return(EEFS_SUCCESS);
    
} /* End of EEFS_LibChkDsk() */

/* Returns the number of file descriptors currently in use */
uint32 EEFS_LibGetFileDescriptorsInUse(void)
{
    return(EEFS_FileDescriptorsInUse);
} /* End of EEFS_LibGetFileDescriptorsInUse() */

/* Returns the file descriptors high water mark - useful for determining if the file descriptor table
 * is large enough */
uint32 EEFS_LibGetFileDescriptorsHighWaterMark(void)
{
    return(EEFS_FileDescriptorsHighWaterMark);
} /* End of EEFS_LibGetFileDescriptorsHighWaterMark() */

/* Returns the max number of files the file system can support */
uint32 EEFS_LibGetMaxFiles(void)
{
    return(EEFS_MAX_FILES);
} /* End of EEFS_LibGetMaxFiles() */

/* Returns the max number of file descriptors */
uint32 EEFS_LibGetMaxOpenFiles(void)
{
    return(EEFS_MAX_OPEN_FILES);
} /* End of EEFS_LibGetMaxOpenFiles() */

/* Prints the filenames of all open files for debugging */
void EEFS_LibPrintOpenFiles(void)
{
    EEFS_FileHeader_t           FileHeader;
    uint32                      i;

    for (i=0; i < EEFS_MAX_OPEN_FILES; i++) {
        if (EEFS_FileDescriptorTable[i].InUse == TRUE) {
           EEFS_LIB_EEPROM_READ(&FileHeader, EEFS_FileDescriptorTable[i].FileHeaderPointer, sizeof(EEFS_FileHeader_t));
           EEFS_SwapFileHeader(&FileHeader); /* APC */
           printf("%s\n", FileHeader.Filename);
        }
    }
    
} /* End of EEFS_LibPrintOpenFiles() */

/************************/
/*  End of File Comment */
/************************/
