/*
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
**
*/

/*
** Filename: eefs_filesys.h
**
** Purpose: This file contains the higher level api interface function to the eefs_fileapi.c code.  This layer of the file
**   system supports multiple devices and volumes where the eefs_fileapi.c layer only focuses on a single file system.
**   Most of the functions in this file are essentually wrappers for the lower level function contained in eefs_fileapi.c.
**   All api functions are designed to be as similar to a standard unix filesystem api as possible.
**
** Design Notes:
**
** The Device Table:
**   The device table defines multiple devices or instances of the eeprom file system.  The device table is initialized by
**   calling the function EEFS_InitFS and requires a unique device name and the base address if the file system in eeprom.
**
**   EEFS_InitFS("/EEDEV0", (uint32)FileSystemBaseAddress1);
**   EEFS_InitFS("/EEDEV1", (uint32)FileSystemBaseAddress2);
**
** The Volume Table:
**   In order to access a device it must be mounted by calling the function EEFS_Mount.  Mounting the device maps a volume name
**   or mount point to the device.
**
**   EEFS_Mount("/EEDEV0", "/EEFS0");
**   EEFS_Mount("/EEDEV1", "/EEFS1");
**
**   Once the file system is mounted then it can be accessed by providing the path of the file as follows:
**   "/MountPoint/Filename"
**
** References:
**
**   See eefs_fileapi.h for additional information.
*/

#ifndef _eefs_filesys_
#define _eefs_filesys_

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"

/*
 * Macro Definitions
 */

#define EEFS_MAX_VOLUMES                2
#define EEFS_MAX_MOUNTPOINT_SIZE        16
#define EEFS_MAX_DEVICES                2
#define EEFS_MAX_DEVICENAME_SIZE        16
#define EEFS_MAX_PATH_SIZE              64

/*
 * Type Definitions
 */

typedef struct
{
    uint32                              InUse;
    uint32                              BaseAddress;
    char                                DeviceName[EEFS_MAX_DEVICENAME_SIZE];
    EEFS_InodeTable_t                   InodeTable;
} EEFS_Device_t;

typedef struct
{
    uint32                              InUse;
    char                                DeviceName[EEFS_MAX_DEVICENAME_SIZE];
    char                                MountPoint[EEFS_MAX_MOUNTPOINT_SIZE];
} EEFS_Volume_t;

typedef struct
{
    char                                MountPoint[EEFS_MAX_PATH_SIZE];
    char                                Filename[EEFS_MAX_PATH_SIZE];
} EEFS_SplitPath_t;

/*
 * Exported Functions
 */

/* Adds a device to the DeviceTable and calls EEFS__InitFS to initialize the file system.  Note that the DeviceName
 * and the BaseAddress of the file system must be unique. */
int32                           EEFS_InitFS(char *DeviceName, uint32 BaseAddress);

/* Mounts a device by mapping a MountPoint to a DeviceName.  The device must be initialized prior
   to mounting it by calling EEFS_InitFS. */
int32                           EEFS_Mount(char *DeviceName, char *MountPoint);

/* Unmounts a volume by removing it from the VolumeTable. */
int32                           EEFS_UnMount(char *MountPoint);

/* Opens the specified file for reading or writing. */
int32                           EEFS_Open(char *Path, uint32 Flags);

/* Creates a new file and opens it for writing. */
int32                           EEFS_Creat(char *Path, uint32 Mode);

/* Closes a file. */
int32                           EEFS_Close(int32 FileDescriptor);

/* Read from a file. */
int32                           EEFS_Read(int32 FileDescriptor, void *Buffer, uint32 Length);

/* Write to a file. */
int32                           EEFS_Write(int32 FileDescriptor, void *Buffer, uint32 Length);

/* Set the file pointer to a specific offset in the file.  If the ByteOffset is specified that is beyond the end of the
 * file then the file pointer is set to the end of the file.  Currently only SEEK_SET is implemented. */
int32                           EEFS_LSeek(int32 FileDescriptor, uint32 ByteOffset, uint16 Origin);

/* Removes the specified file from the file system.  Note that this just marks the file as deleted and does not free the memory
 * in use by the file.  Once a file is deleted, the only way the slot can be reused is to manually write a new file into the
 * slot, i.e. there is no way to reuse the memory through a EEFS api function */
int32                           EEFS_Remove(char *Path);

/* Renames the specified file.  Note that you cannot move a file by renaming it to a different volume. */
int32                           EEFS_Rename(char *OldPath, char *NewPath);

/* Returns file information for the specified file in StatBuffer. */
int32                           EEFS_Stat(char *Path, EEFS_Stat_t *StatBuffer);

/* Sets the Attributes for the specified file. Currently the only attribute that is supported is the EEFS_ATTRIBUTE_READONLY
 * attribute.  To read file attributes use the stat function. */
int32                           EEFS_SetFileAttributes(char *Path, uint32 Attributes);

/* Opens a Volume for reading the file directory.  This should be followed by calls to EEFS_ReadDir and EEFS_CloseDir. */
EEFS_DirectoryDescriptor_t     *EEFS_OpenDir(char *MountPoint);

/* Read the next file directory entry.  Returns a pointer to a EEFS_DirectoryEntry_t if successful or NULL if no more file
 * directory entries exist or an error occurs.  Note that all entries are returned, even empty slots. (The InUse flag will be
 * set to FALSE for empty slots) */
EEFS_DirectoryEntry_t          *EEFS_ReadDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor);

/* Close file system for reading the file directory. */
int32                           EEFS_CloseDir(EEFS_DirectoryDescriptor_t *DirectoryDescriptor);

#endif 

