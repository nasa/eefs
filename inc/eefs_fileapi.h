/*
**
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
*/

/*
 * Filename: eefs_fileapi.h
 *
 * Purpose: This file contains the lower level interface functions to the eeprom file system api functions.  These functions
 *   are volume or device independent, so most functions require a pointer to a inode table.  All api functions are designed
 *   to be as similar to a standard unix file system api as possible.
 *
 * Design Notes:
 *
 * File System Design:
 *   The EEPROM File System is a standard file system interface for data stored in EEPROM.  The EEPROM File System is a slot
 *   based file system where each slot is a fixed size contiguous region of memory allocated for a single file.  Note that
 *   each slot can be a different size.  The size of each slot is determined when the file system is created and is based
 *   on the size of the file to be stored in the slot.  Additional free space can be to the end of each slot to allow room
 *   for the file to grow in size if necessary.  Since this file system is intended to be used for EEPROM it is unlikely
 *   that files in the file system will change very often, so in most cases it will be used as a write once, read many file
 *   system. The file system is organized this way for the following reasons:
 *   1. Keeping each file in one contiguous area of memory makes it easier to patch or reload eeprom without going through
 *      the file system interface if required by on-orbit maintenance.
 *   2. This file system is intended to be used for eeprom, so in most cases it will be used as a write once, read many
 *      file system.  So it is unlikely that files in the file system will change very often.
 *   3. Simplifies the design of the file system.
 *
 * File System Structure:
 *   The file system begins with a File Allocation Table and then is followed by slots for each file in the
 *   file system.  Each slot contains a File Header followed by the File Data.
 *       File Allocation Table
 *       File Header (Slot 0)
 *       File Data   (Slot 0)
 *       File Header (Slot 1)
 *       File Data   (Slot 1)
 *       . . .
 *       File Header (Slot N)
 *       File Data   (Slot N)
 *
 * File Allocation Table:
 *   The File Allocation Table defines where in memory each slot starts as well as the maximum slot size for each file.
 *   This table is a fixed size regardless of how many files actually reside in the file system and never changes unless
 *   a new file is added to the file system using the EEFS_LibCreat() function or the whole file system is reloaded.  The
 *   maximum number of files that can be added to the file system is determined at compile time by the EEFS_MAX_FILES
 *   define.  It is important to choose this number carefully since a code patch would be required to change it.  The file
 *   offsets for each file defined in the File Allocation Table are relative offsets from the beginning of the file system
 *   and point to the start of the File Header.  Since they are relative offsets the file system in not tied to any
 *   physical address.  In fact the exact same file system image can be burned into multiple locations of EEPROM and then
 *   mounted as different volumes.  The size of each slot is specified in bytes and does NOT include the File Header.  So
 *   each slot actually occupies (File Header Size + Slot Size) bytes of EEPROM.
 *
 * File Header:
 *   Each slot in the file system starts with a File Header.  The File Header contains information about the file
 *   contained in the slot.  The File Header will be all 0's if the file has been deleted or is unused.
 *
 * File Data:
 *   File Data starts immediately following the File Header and may or may not use all of the available space in the slot.
 *
 * Inode Table:
 *   The Inode Table is a ram table that is used by the file system api to access the file system and is similar in structure
 *   to the File Allocation Table.  The Inode table is initialized when the function EEFS_LibInitFS() is called and once the
 *   Inode table is initialized the File Allocation Table is no longer used. One important difference between the File
 *   Allocation Table and the Inode Table is that the Inode Table contains physical address pointers to the start of each file
 *   instead of relative offsets.  Note also that the Inode table does not cache any information about the file in ram.  This
 *   means that the file could be patched or reloaded to EEPROM without the need to patch the Inode Table, i.e. the file
 *   updates are available to the file system immediately.  A disadvantage to this approach is that each File Header must be
 *   read from EEPROM when searching the file system for a specific file, for example when a file is opened.
 *
 * File Descriptor Table:
 *   The File Descriptor Table manages all File Descriptors for the EEPROM File System.  There is only one File Descriptor
 *   Table that is shared by all EEPROM File System volumes.  The maximum number of files that can be open at one time is
 *   determined at compile time by the EEFS_MAX_OPEN_FILES define.
 *
 * Directory Descriptor Table:
 *   The Directory Descriptor Table manages the Directory Descriptor for the EEPROM File System.  There is currently only
 *   one Directory Descriptor that is shared by all EEPROM File System volumes.
 *
 * EEPROM Access:
 *   The EEPROM File System software never directly reads or writes to EEPROM, instead it uses implementation specific EEPROM
 *   interface functions.  Since not all EEPROM is memory mapped, some EEPROM implementations may require implementation
 *   specific functions for accessing EEPROM.  The implementation specific EEPROM interface functions are defined as macros
 *   in the file eefs_macros.h.  By default these macros are defined to use memcpy.  Note also that the EEPROM interface
 *   functions are protected from shared access by the EEPROM File System however there is nothing that prevents other
 *   processes from calling the EEPROM interface functions from outside of the EEPROM File System.
 *
 * Mutual Exclusion:
 *   Mutual exclusion is implemented by the functions EEFS_LibLock and EEFS_LibUnlock.  Since the EEPROM File System is not
 *   intended to be used very often and to keep things simple it was decided to implement a single locking mechanism that is
 *   shared by all EEPROM File System volumes.  This locking mechanism simply locks the shared resource at the start of each
 *   function and unlocks the shared resource at the end of each function.  It is recommended that semaphores be used as the
 *   locking mechanism vs disabling interrupts.  Note that since the shared resource is locked for all lower level functions,
 *   lower level functions should not be called recursively.  The implementation of the EEFS_LibLock and EEFS_LibUnlock
 *   functions are defined as macros in the file eefs_macros.h.
 *
 *   The EEFS_Lock and EEFS_Unlock functions are intended to protect the following resources from shared access:
 *   1. File Descriptor Table - File Descriptors are maintained in a single table that is shared by all EEFS file systems.
 *   2. Inode Table - Some functions require that the state of the file system be preserved during critical sections of code.
 *   3. File Directory Descriptor Table - The File Directory Table is shared by all EEFS file systems.
 *   4. EEPROM interface routines - The implementation of these functions is unknown since they may be implementation specific.
 *
 * Time Stamps:
 *   Time Stamps are implemented by the function EEFS_LibTime.  Time stamps are based on the standard library time_t.  The
 *   implementation of the EEFS_LibTime function is defined as a macro in the file eefs_macros.h.  By default this macro is
 *   defined to use the standard library time function.
 *
 * Directories:
 *   The EEPROM File System only supports a single top level directory and does not support sub directories.  If you want to
 *   group files separately then they should be placed in different volumes.  For example /EEFS0_Apps for apps, /EEFS0_Tables
 *   for tables etc...
 *
 * CRC's:
 *   The EEPROM File System includes a crc in the File Allocation Table and a crc in the File Header for each file.  Currently
 *   the crc included in the File Allocation Table is calculated across the entire file system, including unused space
 *   (i.e. MaxEepromSize).  The crc included in each File Header is calculated only across the File Header and the File Data
 *   and does not include unused space at the end of the file.  Note that the crc's are currently NOT used by the file system
 *   and are NOT automatically updated by the file system when files are modified, this must be done manually.  The crc
 *   included in the File Allocation Table is only used by the bootstrap code to verify the integrity of the file system at
 *   boot time.
 *
 *   The original design was that the crc included in the File Allocation Table would cover only the File Allocation Table and
 *   to validate the entire file system you would also need to verify the crc included in each File Header.  The advantage
 *   to maintaining separate crc's is that if a file changes, only the file crc must be updated, where in the current
 *   implementation the same EEPROM address must be repeatedly rewritten to update the crc for every change made to the
 *   file system.
 *
 * Micro EEPROM File System:
 *   The Micro version of the EEPROM file system allows bootstrap code access to files in an EEPROM File System.  The full
 *   implementation of the EEPROM File System is too large to be used in bootstrap code so a simple single function version
 *   was developed that returns the starting address of a file given its filename.  The bootstrap code can then boot the
 *   system from a kernel image that is contained in an EEPROM File System.  This software is designed to use very little
 *   memory, and is independent of the file system size (i.e. maximum number of files).  This means that the bootstrap image
 *   will NOT have to be updated whenever the size of the EEPROM File System changes.
 *
 * Building a File System Image:
 *   EEPROM File System images are created using the geneepromfs tool.  This command line tool reads an input file that
 *   describes the files that will be included in the file system and outputs an EEPROM File System image ready to be
 *   burned into EEPROM.
 * 
 * References:
 *
 */

#ifndef _eefs_fileapi_
#define _eefs_fileapi_

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_config.h"
#include "eefs_version.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

/*
 * Macro Definitions
 */

#define EEFS_FILESYS_MAGIC              0xEEF51234
#define EEFS_MAX_FILENAME_SIZE          40

/*
 * File Attributes
 */

#define EEFS_ATTRIBUTE_NONE             0
#define EEFS_ATTRIBUTE_READONLY         1

/*
 * File Modes
 */

/* These defines are used as a bit mask and are stored as the mode in the File Descriptor Table.
 * The bit mask will be EEFS_FREAD if the file is open for read access, EEFS_WRITE if the file
 * is open for write access, or (EEFS_FREAD | EEFS_FWRITE) if the file is open for both read
 * and write access. */
#define EEFS_FREAD                      1       /* (O_RDONLY + 1) */
#define EEFS_FWRITE                     2       /* (O_WRONLY + 1) */
#define EEFS_FCREAT                     4

/*
 * Error Codes
 */

#define EEFS_SUCCESS                   (0)
#define EEFS_ERROR                    (-1)
#define EEFS_INVALID_ARGUMENT         (-2)
#define EEFS_UNSUPPORTED_OPTION       (-3)
#define EEFS_PERMISSION_DENIED        (-4)
#define EEFS_FILE_NOT_FOUND           (-5)
#define EEFS_NO_FREE_FILE_DESCRIPTOR  (-6)
#define EEFS_NO_SPACE_LEFT_ON_DEVICE  (-7)
#define EEFS_NO_SUCH_DEVICE           (-8)
#define EEFS_DEVICE_IS_BUSY           (-9)
#define EEFS_READ_ONLY_FILE_SYSTEM   (-10)

/*
 * Type Definitions
 */

typedef struct
{
    uint32                              Crc;
    uint32                              Magic;
    uint32                              Version;
    uint32                              FreeMemoryOffset;
    uint32                              FreeMemorySize;
    uint32                              NumberOfFiles;
} EEFS_FileAllocationTableHeader_t;

typedef struct
{
    uint32                              FileHeaderOffset;   /* relative offset of the file header from the beginning of the file system */
    uint32                              MaxFileSize;        /* max size of the slot in bytes (note this number does NOT include the file header) */
} EEFS_FileAllocationTableEntry_t;

typedef struct
{
    EEFS_FileAllocationTableHeader_t    Header;
    EEFS_FileAllocationTableEntry_t     File[EEFS_MAX_FILES];
} EEFS_FileAllocationTable_t;

typedef struct
{
    uint32                              Crc;
    uint32                              InUse;  /* if InUse is FALSE then the file has been deleted */
    uint32                              Attributes;
    uint32                              FileSize;
    time_t                              ModificationDate;
    time_t                              CreationDate;
    char                                Filename[EEFS_MAX_FILENAME_SIZE];
} EEFS_FileHeader_t;

typedef struct
{
    void                               *FileHeaderPointer;
    uint32                              MaxFileSize;
} EEFS_InodeTableEntry_t;

typedef struct
{
    uint32                              BaseAddress;
    void                               *FreeMemoryPointer;
    uint32                              FreeMemorySize;
    uint32                              NumberOfFiles;
    EEFS_InodeTableEntry_t              File[EEFS_MAX_FILES];
} EEFS_InodeTable_t;

typedef struct
{
    uint32                              InUse;
    uint32                              Mode;
    void                               *FileHeaderPointer;
    void                               *FileDataPointer;
    uint32                              ByteOffset;
    uint32                              FileSize;
    uint32                              MaxFileSize;
    EEFS_InodeTable_t                  *InodeTable;
    uint32                              InodeIndex;
} EEFS_FileDescriptor_t;

typedef struct
{
    uint32                              InUse;
    uint32                              InodeIndex;
    EEFS_InodeTable_t                  *InodeTable;
} EEFS_DirectoryDescriptor_t;

typedef struct
{
    uint32                              InodeIndex;
    char                                Filename[EEFS_MAX_FILENAME_SIZE];
    uint32                              InUse;
    void                               *FileHeaderPointer;
    uint32                              MaxFileSize;
} EEFS_DirectoryEntry_t;

typedef struct
{
    uint32                              InodeIndex;
    uint32                              Crc;
    uint32                              Attributes;
    uint32                              FileSize;
    time_t                              ModificationDate;
    time_t                              CreationDate;
    char                                Filename[EEFS_MAX_FILENAME_SIZE];
} EEFS_Stat_t;

/*
 * Exported Functions
 */

/* Initialize global data shared by all file systems */
void                            EEFS_LibInit(void);

/* Initializes the Inode Table.  Returns EEFS_SUCCESS on success, EEFS_NO_SUCH_DEVICE or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibInitFS(EEFS_InodeTable_t *InodeTable, uint32 BaseAddress);

/* Clears the Inode Table.  Returns EEFS_SUCCESS on success, EEFS_DEVICE_IS_BUSY or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibFreeFS(EEFS_InodeTable_t *InodeTable);

/* Opens the specified file for read or write access.  This function supports the following Flags (O_RDONLY, O_WRONLY,
 * O_RDWR, O_TRUNC, O_CREAT).  Files can always be opened for shared read access, however files cannot be opened more than
 * once for shared write access.  Returns a file descriptor on success, EEFS_NO_FREE_FILE_DESCRIPTOR, EEFS_PERMISSION_DENIED,
 * EEFS_INVALID_ARGUMENT, or EEFS_FILE_NOT_FOUND on error. */
int32                           EEFS_LibOpen(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Flags, uint32 Attributes);

/* Creates a new file and opens it for writing.  If the file already exists then the existing file is opened for write
 * access and the file is truncated.  If the file does not already exist then a new file is created.  Since we don't know
 * the size of the file yet all remaining free eeprom is allocated for the new file. When the file is closed then the
 * MaxFileSize is updated to be the actual size of the file + EEFS_DEFAULT_CREAT_SPARE_BYTES.  Note that since all free eeprom
 * is allocated for the file while it is open, only one new file can be created at a time. Returns a file descriptor on
 * success, EEFS_NO_FREE_FILE_DESCRIPTOR, EEFS_PERMISSION_DENIED, EEFS_INVALID_ARGUMENT, or EEFS_NO_SPACE_LEFT_ON_DEVICE
 * on error.*/
int32                           EEFS_LibCreat(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes);

/* Closes a file.  Returns EEFS_SUCCESS on success or EEFS_INVALID_ARGUMENT on error.  If a new file is being created then the
 * MaxFileSize is updated to be the actual size of the file + EEFS_DEFAULT_CREAT_SPARE_BYTES. Note that the File Allocation
 * Table is not updated until the file is closed to reduce the number of EEPROM Writes. */
int32                           EEFS_LibClose(int32 FileDescriptor);

/* Read from a file.  Returns the number of bytes read, 0 bytes if we have reached the end of file, or EEFS_INVALID_ARGUMENT
 * on error. */
int32                           EEFS_LibRead(int32 FileDescriptor, void *Buffer, uint32 Length);

/* Write to a file.  Returns the number of bytes written, 0 bytes if we have run out of memory or EEFS_INVALID_ARGUMENT
 * on error. */
int32                           EEFS_LibWrite(int32 FileDescriptor, void *Buffer, uint32 Length);

/* Set the file pointer to a specific offset in the file.  This implementation does not support seeking beyond the end of a file.  
 * If a ByteOffset is specified that is beyond the end of the file then the file pointer is set to the end of the file.  If 
 * a ByteOffset is specified that is less than the start of the file then an error is returned.  Returns the current file pointer 
 * on success, or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibLSeek(int32 FileDescriptor, int32 ByteOffset, uint16 Origin);

/* Removes the specified file from the file system.  Note that this just marks the file as deleted and does not free the memory
 * in use by the file.  Once a file is deleted, the only way the slot can be reused is to manually write a new file into the
 * slot, i.e. there is no way to reuse the memory through a EEFS api function.  Returns a file descriptor on success,
 * EEFS_PERMISSION_DENIED, EEFS_FILE_NOT_FOUND or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibRemove(EEFS_InodeTable_t *InodeTable, char *Filename);

/* Renames the specified file.  Returns EEFS_SUCCESS on success, EEFS_PERMISSION_DENIED, EEFS_FILE_NOT_FOUND,
 * or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibRename(EEFS_InodeTable_t *InodeTable, char *OldFilename, char *NewFilename);

/* Returns file information for the specified filename in StatBuffer.  Returns EEFS_SUCCESS on success, EEFS_FILE_NOT_FOUND,
 * or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibStat(EEFS_InodeTable_t *InodeTable, char *Filename, EEFS_Stat_t *StatBuffer);

/* Returns file information for the specified file descriptor in StatBuffer.  Returns EEFS_SUCCESS on success or
 * EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibFstat(int32 FileDescriptor, EEFS_Stat_t *StatBuffer);

/* Sets the Attributes for the specified file. Currently the only attribute that is supported is the EEFS_ATTRIBUTE_READONLY
 * attribute.  Returns EEFS_SUCCESS on success, EEFS_FILE_NOT_FOUND or EEFS_INVALID_ARGUMENT on error.  To read file
 * attributes use the stat function. */
int32                           EEFS_LibSetFileAttributes(EEFS_InodeTable_t *InodeTable, char *Filename, uint32 Attributes);

/* Opens a file system for reading the file directory.  This should be followed by calls to EEFS_ReadDir() and EEFS_CloseDir().
 * Note that currently only one process can read the file directory at a time.  Returns a pointer to a directory descriptor
 * on success and a NULL pointer on error. */
EEFS_DirectoryDescriptor_t     *EEFS_LibOpenDir(EEFS_InodeTable_t *InodeTable);

/* Read the next file directory entry.  Returns a pointer to a EEFS_DirectoryEntry_t if successful or NULL if no more file
 * directory entries exist or an error occurs.  Note that all entries are returned, even empty slots. (The InUse flag will be
 * set to FALSE for empty slots) */
EEFS_DirectoryEntry_t          *EEFS_LibReadDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor);

/* Close file system for reading the file directory.  Returns EEFS_SUCCESS on success or EEFS_INVALID_ARGUMENT on error. */
int32                           EEFS_LibCloseDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor);

/* Returns TRUE if any files in the file system are open. */
uint8                           EEFS_LibHasOpenFiles(EEFS_InodeTable_t *InodeTable);

/* Returns TRUE if a directory is open. */
uint8                           EEFS_LibHasOpenDir(EEFS_InodeTable_t *InodeTable);

/* Returns a pointer to the specified File Descriptor, or NULL if the specified
 * File Descriptor is not valid */
EEFS_FileDescriptor_t          *EEFS_LibFileDescriptor2Pointer(int32 FileDescriptor);

/* Checks file system integrity and dumps the contents of the Inode Table and all File Headers */
int32                           EEFS_LibChkDsk(EEFS_InodeTable_t *InodeTable, uint32 Flags);

/* Returns the number of file descriptors currently in use */
uint32                          EEFS_LibGetFileDescriptorsInUse(void);

/* Returns the file descriptors high water mark - useful for determining if the file descriptor table
 * is large enough */
uint32                          EEFS_LibGetFileDescriptorsHighWaterMark(void);

/* Returns the max number of files the file system can support */
uint32                          EEFS_LibGetMaxFiles(void);

/* Returns the max number of file descriptors */
uint32                          EEFS_LibGetMaxOpenFiles(void);

/* Prints the filenames of all open files for debugging */
void                            EEFS_LibPrintOpenFiles(void);

#endif 

/************************/
/*  End of File Comment */
/************************/

