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
** Filename: eefs_vxworks.c
**
** Purpose: This file contains vxWorks Device Driver functions for the EEPROM File System.
** 
*/

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"
#include "eefs_vxworks.h"
#include "dirent.h"
#include "iosLib.h"
#include "errnoLib.h"
#include "semLib.h"
#include "sys/stat.h"
#include "stdlib.h"
#include "string.h"

/*
 * Macro Definitions
 */

/* EEFS_OpenFileDescriptor_t Types */
#define EEFS_FILE               1
#define EEFS_DIRECTORY          2

/*
 * Type Definitions
 */

/* Since the vxWorks open can open both a file or a directory I added a new file descriptor data type that
 * keeps track of the appropriate EEFS data structures based on what is open.  The Type field identifies
 * what is open, a file or a directory. */
typedef struct {
    int32                       Type;
    int32                       FileDescriptor;
    EEFS_DirectoryDescriptor_t *DirectoryDescriptor;
} EEFS_OpenFileDescriptor_t;

/*
 * Local Data
 */

SEM_ID                          EEFS_semId;

/*
 * Local Function Prototypes
 */

int                             EEFS_Creat(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path, int Mode);
int                             EEFS_Remove(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path);
int                             EEFS_Open(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path, int Flags, int Mode);
int                             EEFS_Close(EEFS_OpenFileDescriptor_t *OpenFileDescriptor);
int                             EEFS_Read(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, void *Buffer, int Length);
int                             EEFS_Write(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, void *Buffer, int Length);
int                             EEFS_Ioctl(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int Function, int Arg);
int                             EEFS_Seek(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int ByteOffset);
int                             EEFS_Ftell(EEFS_OpenFileDescriptor_t *OpenFileDescriptor);
int                             EEFS_Funread(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int *UnreadBytes);
int                             EEFS_ReadDir(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, DIR *Directory);
int                             EEFS_Fstat(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, struct stat *StatBuffer);
int                             EEFS_Rename(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, char *NewFilename);
int                             EEFS_ChkDsk(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int Arg);
int                             EEFS_FreeSpace(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, uint32 *FreeCount);
int                             EEFS_FreeSpace64(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, uint64 *FreeCount);
char                           *EEFS_ExtractFilename(char *Path);

/*
 * Function Definitions
 */

/* Adds the eeprom file system driver to the vxWorks driver table and returns a index or driver number to the slot in
 * the driver table where the driver was installed.  This function should only be called once during startup.  Returns 
 * a driver number on success or ERROR if there was an error. */
int EEFS_DrvInstall(void)
{
    int DriverNumber;

    if ((EEFS_semId = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE)) != NULL) {

        EEFS_LibInit();
        
        DriverNumber = iosDrvInstall(
                (FUNCPTR) EEFS_Creat,   /* create */
                (FUNCPTR) EEFS_Remove,  /* delete */
                (FUNCPTR) EEFS_Open,    /* open */
                (FUNCPTR) EEFS_Close,   /* close */
                (FUNCPTR) EEFS_Read,    /* read */
                (FUNCPTR) EEFS_Write,   /* write */
                (FUNCPTR) EEFS_Ioctl    /* ioctl */
                );

        return (DriverNumber);
    }
    else { /* error creating semaphore */
        
        return(ERROR);
    }
    
} /* End of EEFS_DrvInstall() */

/* Add a eeprom file system device to the vxWorks device list. The device name must start with a slash, ex. /eefs1.  The
 * base address is the start address of the file system in eeprom.  The device descriptor is initialized by this function 
 * however you must allocate memory for the device descriptor.  Returns OK on success or ERROR if there was an error. */
int EEFS_DevCreate(int DriverNumber, char *DeviceName, uint32 BaseAddress, EEFS_DeviceDescriptor_t *DeviceDescriptor)
{
    int ReturnCode;

    if (DeviceDescriptor != NULL) {

        if (EEFS_LibInitFS(&DeviceDescriptor->InodeTable, BaseAddress) == EEFS_SUCCESS) {

            if (iosDevAdd((DEV_HDR *)DeviceDescriptor, DeviceName, DriverNumber) != ERROR) {
                ReturnCode = OK;
            }
            else { /* error adding device */
                ReturnCode = ERROR;
            }
        }
        else { /* error initializing file system */
            ReturnCode = ERROR;
        }
    }
    else { /* invalid device descriptor */
        ReturnCode = ERROR;
    }

    return (ReturnCode);
    
} /* End of EEFS_DevCreate() */

/* Remove a eeprom file system device from the vxWorks device list.  Returns OK on success or ERROR if there
 * was an error.  */
int EEFS_DevDelete(EEFS_DeviceDescriptor_t *DeviceDescriptor)
{
    int ReturnCode;

    if (DeviceDescriptor != NULL) {

        if (EEFS_LibFreeFS(&DeviceDescriptor->InodeTable) == EEFS_SUCCESS) {

            iosDevDelete((DEV_HDR *)DeviceDescriptor);
            ReturnCode = OK;
        }
        else { /* error freeing file system */
            ReturnCode = ERROR;
        }
    }
    else { /* invalid device descriptor */
        ReturnCode = ERROR;
    }

    return(ReturnCode);
    
} /* End of EEFS_DevDelete() */

/* Create a new file or re-write an existing file.  The EEFS does not support directories so a new directory cannot be created
 * with this function.  Returns a pointer to a EEFS_OpenFileDescriptor_t on success or ERROR if there was an error. */
int EEFS_Creat(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path, int Mode)
{
    EEFS_OpenFileDescriptor_t  *OpenFileDescriptor;
    char                       *Filename;
    int                         ReturnCode;

    (void)Mode; /* not supported at this time */
    if (DeviceDescriptor != NULL) {

        if (Path != NULL) {

            Filename = EEFS_ExtractFilename(Path);
            if ((OpenFileDescriptor = malloc(sizeof(EEFS_OpenFileDescriptor_t))) != NULL) {

                memset(OpenFileDescriptor, 0, sizeof(EEFS_OpenFileDescriptor_t));

                OpenFileDescriptor->Type = EEFS_FILE;
                if ((OpenFileDescriptor->FileDescriptor = EEFS_LibCreat(&DeviceDescriptor->InodeTable, Filename, EEFS_ATTRIBUTE_NONE)) >= 0) {
                    return((int)OpenFileDescriptor);
                }

                /* if we get here then there was an error so free the open file descriptor */
                if (OpenFileDescriptor->FileDescriptor == EEFS_PERMISSION_DENIED) {
                    errnoSet(EACCES);
                }
                else if (OpenFileDescriptor->FileDescriptor == EEFS_NO_SPACE_LEFT_ON_DEVICE) {
                    errnoSet(ENOSPC);
                }
                else if (OpenFileDescriptor->FileDescriptor == EEFS_READ_ONLY_FILE_SYSTEM) {
                    errnoSet(EROFS);
                }
                else {
                    errnoSet(EINVAL);
                }

                free(OpenFileDescriptor);
                ReturnCode = ERROR;
            }
            else { /* error allocating open file descriptor */
                ReturnCode = ERROR;
                errnoSet(ENOMEM);
            }
        }
        else { /* invalid path */
            ReturnCode = ERROR;
            errnoSet(ENOENT);
        }
    }
    else { /* invalid device descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Creat() */

/* Delete a file from the file system.  Returns OK on success or ERROR if there was an error. */
int EEFS_Remove(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path)
{
    char     *Filename;
    int32     Status;
    int       ReturnCode;

    if (DeviceDescriptor != NULL) {

        if (Path != NULL) {

            Filename = EEFS_ExtractFilename(Path);
            Status = EEFS_LibRemove(&DeviceDescriptor->InodeTable, Filename);
            if (Status == EEFS_SUCCESS) {
                ReturnCode = OK;
            }
            else if (Status == EEFS_READ_ONLY_FILE_SYSTEM) {
                ReturnCode = ERROR;
                errnoSet(EROFS);
            }
            else { /* error deleting file */
                ReturnCode = ERROR;
                errnoSet(ENOENT);
            }
        }
        else { /* invalid path */
            ReturnCode = ERROR;
            errnoSet(ENOENT);
        }
    }
    else { /* invalid device descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return (ReturnCode);
    
} /* End of EEFS_Remove() */

/* Open a directory for reading or a file for reading and writing. Returns a pointer to a EEFS_OpenFileDescriptor_t
 *  on success or ERROR if there was an error. */
int EEFS_Open(EEFS_DeviceDescriptor_t *DeviceDescriptor, char *Path, int Flags, int Mode)
{
    EEFS_OpenFileDescriptor_t  *OpenFileDescriptor;
    char                       *Filename;
    int                         ReturnCode;
    
    if (DeviceDescriptor != NULL) {

        if (Path != NULL) {

            if ((Mode & FSTAT_DIR) == 0) { /* check to see if they are trying to create a new directory */

                Filename = EEFS_ExtractFilename(Path);
                if ((OpenFileDescriptor = malloc(sizeof(EEFS_OpenFileDescriptor_t))) != NULL) {

                    memset(OpenFileDescriptor, 0, sizeof(EEFS_OpenFileDescriptor_t));

                    /* the device name is stripped off the Path by vxWorks and the remainder is passed to this function.  If
                     * there is no filename then we want to open a directory else we want to open a file. */
                    if (strlen(Filename) == 0) {

                        if (Flags == O_RDONLY) {  /* can only open a directory read only */

                            OpenFileDescriptor->Type = EEFS_DIRECTORY;
                            if ((OpenFileDescriptor->DirectoryDescriptor = EEFS_LibOpenDir(&DeviceDescriptor->InodeTable)) != NULL) {
                                return((int)OpenFileDescriptor);
                            }
                            errnoSet(EBADF);
                        }
                        else {
                            errnoSet(EACCES);
                        }
                    }
                    else {

                        OpenFileDescriptor->Type = EEFS_FILE;
                        if ((OpenFileDescriptor->FileDescriptor = EEFS_LibOpen(&DeviceDescriptor->InodeTable, Filename, Flags, EEFS_ATTRIBUTE_NONE)) >= 0) {
                            return((int)OpenFileDescriptor);
                        }

                        if (OpenFileDescriptor->FileDescriptor == EEFS_PERMISSION_DENIED) {
                            errnoSet(EACCES);
                        }
                        else if (OpenFileDescriptor->FileDescriptor == EEFS_FILE_NOT_FOUND) {
                            errnoSet(ENOENT);
                        }
                        else if (OpenFileDescriptor->FileDescriptor == EEFS_READ_ONLY_FILE_SYSTEM) {
                            errnoSet(EROFS);
                        }
                        else {
                            errnoSet(EINVAL);
                        }
                    }

                    /* if we get here then there was an error so free the open file descriptor */
                    free(OpenFileDescriptor);
                    ReturnCode = ERROR;
                }
                else { /* error allocating open file descriptor */
                    ReturnCode = ERROR;
                    errnoSet(ENOMEM);
                }
            }
            else { /* can't create subdirectory */
                ReturnCode = ERROR;
                errnoSet(EINVAL);
            }
        }
        else { /* invalid path */
            ReturnCode = ERROR;
            errnoSet(ENOENT);
        }
    }
    else { /* invalid device descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Open() */

/* Close an open file or directory.  Returns OK on success or ERROR if there was an error. */
int EEFS_Close(EEFS_OpenFileDescriptor_t *OpenFileDescriptor)
{
    int       ReturnCode;
    
    if (OpenFileDescriptor != NULL) {
        
        if (OpenFileDescriptor->Type == EEFS_DIRECTORY) {

            if (EEFS_LibCloseDir(OpenFileDescriptor->DirectoryDescriptor) == EEFS_SUCCESS) {
                memset(OpenFileDescriptor, 0, sizeof(EEFS_OpenFileDescriptor_t));
                free(OpenFileDescriptor);
                ReturnCode = OK;
            }
            else { /* error closing directory */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else if (OpenFileDescriptor->Type == EEFS_FILE) {

            if (EEFS_LibClose(OpenFileDescriptor->FileDescriptor) == EEFS_SUCCESS) {
                memset(OpenFileDescriptor, 0, sizeof(EEFS_OpenFileDescriptor_t));
                free(OpenFileDescriptor);
                ReturnCode = OK;
            }
            else { /* error closing file */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else { /* invalid open file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = EEFS_ERROR;
        errnoSet(EBADF);
    }
    
    return(ReturnCode);
    
} /* End of EEFS_Close() */

/* Read data from an open file.  Returns the number of bytes read on success or ERROR if there was an error. */
int EEFS_Read(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, void *Buffer, int Length)
{
    int       BytesRead;
    int       ReturnCode;

    if (OpenFileDescriptor != NULL) {
        
        if (OpenFileDescriptor->Type == EEFS_FILE) {
            
             if ((BytesRead = EEFS_LibRead(OpenFileDescriptor->FileDescriptor, Buffer, Length)) >= 0) {

                 ReturnCode = BytesRead;
             }
             else {
                 ReturnCode = ERROR;
                 errnoSet(EIO);
             }
        }
        else { /* not a EEFS_FILE file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }
    
    return(ReturnCode);
    
} /* End of EEFS_Read() */

/* Write data to an open file.  Returns the number of bytes written on success or ERROR if there was an error. */
int EEFS_Write(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, void *Buffer, int Length)
{
    int       BytesWritten;
    int       ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_FILE) {

            if ((BytesWritten = EEFS_LibWrite(OpenFileDescriptor->FileDescriptor, Buffer, Length)) >= 0) {

                ReturnCode = BytesWritten;
            }
            else {
                ReturnCode = ERROR;
                errnoSet(EIO);
            }
        }
        else { /* not a EEFS_FILE file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Write() */

/* Perform device specific i/o control.  Returns ERROR if the requested operation is not
   supported by this device. */
int EEFS_Ioctl(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int Function, int Arg)
{
    switch (Function) {
        
        case FIOSEEK: /* set file read/write pointer */
            return(EEFS_Seek(OpenFileDescriptor, Arg));
            break;

        case FIOWHERE: /* return the current seek pointer */
            return(EEFS_Ftell(OpenFileDescriptor));
            break;

        case FIONREAD: /* return the number of unread bytes in the file */
            return(EEFS_Funread(OpenFileDescriptor, (int *)Arg));
            break;

        case FIOREADDIR: /* read an entry from a directory structure */
            return(EEFS_ReadDir(OpenFileDescriptor, (DIR *)Arg));
            break;

        case FIOFSTATGET: /* return file inode information */
            return(EEFS_Fstat(OpenFileDescriptor, (struct stat *)Arg));
            break;

        case FIORENAME: /* rename a file */
            return(EEFS_Rename(OpenFileDescriptor, (char *)Arg));
            break;

        case FIONFREE: /* return amount of free space */
            return(EEFS_FreeSpace(OpenFileDescriptor, (uint32 *)Arg));
            break;

        case FIONFREE64: /* return amount of free space */
            return(EEFS_FreeSpace64(OpenFileDescriptor, (uint64 *)Arg));
            break;

        case FIOCHKDSK: /* checkdsk */
            return(EEFS_ChkDsk(OpenFileDescriptor, Arg));
            break;

        default:
            errnoSet(ENOTSUP);
            return(ERROR);
            break;
    }
    
} /* End of EEFS_Ioctl() */

/* Set the file read/write pointer.  Returns OK on success or ERROR if there was an error. */
int EEFS_Seek(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int ByteOffset)
{
    int       ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_FILE) {

            if (EEFS_LibLSeek(OpenFileDescriptor->FileDescriptor, ByteOffset, SEEK_SET) >= 0) {

                ReturnCode = OK;
            }
            else { /* error in EEFS_LibLSeek */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else { /* not a EEFS_FILE file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Seek() */

/* Returns the current seek pointer.  Returns the current byte offset of the seek pointer in the file or ERROR if there
   was an error. */
int EEFS_Ftell(EEFS_OpenFileDescriptor_t *OpenFileDescriptor)
{
    EEFS_FileDescriptor_t  *FileDescriptorPointer;
    int                     ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_FILE) {

            if ((FileDescriptorPointer = EEFS_LibFileDescriptor2Pointer(OpenFileDescriptor->FileDescriptor)) != NULL) {

                return(FileDescriptorPointer->ByteOffset);
            }
            else { /* invalid EEFS file descriptor */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else { /* not a EEFS_FILE file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Ftell() */

/* Returns the number of unread bytes in the file.  The number of unread bytes is returned in the UnreadBytes argument.
 * Returns OK on success or ERROR if there was an error. */
int EEFS_Funread(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int *UnreadBytes)
{
    EEFS_FileDescriptor_t  *FileDescriptorPointer;
    int                     ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (UnreadBytes != NULL) {

            if (OpenFileDescriptor->Type == EEFS_FILE) {

                if ((FileDescriptorPointer = EEFS_LibFileDescriptor2Pointer(OpenFileDescriptor->FileDescriptor)) != NULL) {

                    *UnreadBytes = (FileDescriptorPointer->FileSize - FileDescriptorPointer->ByteOffset);
                    ReturnCode = OK;
                }
                else { /* invalid EEFS file descriptor */
                    ReturnCode = ERROR;
                    errnoSet(EBADF);
                }
            }
            else { /* not a EEFS_FILE file descriptor */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else { /* invalid argument */
            ReturnCode = ERROR;
            errnoSet(EINVAL);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Funread() */

/* Read an entry from a directory structure.  The directory information is returned in the Directory argument.
 * Returns OK on success or ERROR if there was an error. */
int EEFS_ReadDir(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, DIR *Directory)
{
    EEFS_DirectoryEntry_t          *DirectoryEntry;
    int                             ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_DIRECTORY) {

            if (Directory != NULL) {
            
                /* the vxWorks code uses the dd_cookie variable to keep track of where you are in the directory listing.  I
                 * have my own variable for this purpose, however the vxWorks code has a rewind command that sets dd_cookie
                 * to 0 when they want to restart the directory listing at the beginning.  So if dd_cookie is 0 then I reset my
                 * InodeIndex variable back to 0. */
                if (Directory->dd_cookie == 0) { OpenFileDescriptor->DirectoryDescriptor->InodeIndex = 0; }

                /* the EEFS function returns an entry for all slots, even empty ones, however for this implementation we want
                 * to skip the empty slots */
                DirectoryEntry = EEFS_LibReadDir(OpenFileDescriptor->DirectoryDescriptor);
                while ((DirectoryEntry != NULL) && (DirectoryEntry->InUse == FALSE)) {
                    DirectoryEntry = EEFS_LibReadDir(OpenFileDescriptor->DirectoryDescriptor);
                }

                if ((DirectoryEntry != NULL) && (DirectoryEntry->InUse == TRUE)) {
                    strncpy(Directory->dd_dirent.d_name, DirectoryEntry->Filename, _PARM_NAME_MAX);
                    Directory->dd_cookie++;
                }
                else {
                    Directory->dd_eof = TRUE;
                }

                ReturnCode = OK;
            }
            else { /* invalid argument */
                ReturnCode = ERROR;
                errnoSet(EINVAL);
            }
        }
        else { /* not a EEFS_DIRECTORY file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_ReadDir() */

/* Returns file inode information.  The stat information is returned in the StatBuffer argument.
 * Returns OK on success or ERROR if there was an error. */
int EEFS_Fstat(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, struct stat *StatBuffer)
{
    EEFS_Stat_t         EEFS_StatBuffer;
    int                 ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (StatBuffer != NULL) {

            memset(StatBuffer, 0, sizeof(struct stat));
            if (OpenFileDescriptor->Type == EEFS_FILE) {
                
                if (EEFS_LibFstat(OpenFileDescriptor->FileDescriptor, &EEFS_StatBuffer) == EEFS_SUCCESS) {
                    StatBuffer->st_mode = S_IFREG;            
                    StatBuffer->st_size = EEFS_StatBuffer.FileSize;
                    ReturnCode = OK;
                }
                else { /* error getting stat */
                    ReturnCode = ERROR;
                    errnoSet(EBADF);
                }
            }
            else { /* the only stat information returned for a directory is the st_mode */
                StatBuffer->st_mode = S_IFDIR;
                ReturnCode = OK;
            }
        }
        else { /* invalid stat buffer */
            ReturnCode = ERROR;
            errnoSet(EINVAL);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Fstat() */

/* Renames a file.  Returns OK on success or ERROR if there was an error. */
int EEFS_Rename(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, char *Path)
{
    EEFS_Stat_t             EEFS_StatBuffer;
    EEFS_FileDescriptor_t  *FileDescriptorPointer;
    char                   *NewFilename;
    int32                   Status;
    int                     ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (Path != NULL) {

            NewFilename = EEFS_ExtractFilename(Path);
            if (OpenFileDescriptor->Type == EEFS_FILE) {

                /* Get the old filename by using fstat */
                if (EEFS_LibFstat(OpenFileDescriptor->FileDescriptor, &EEFS_StatBuffer) == EEFS_SUCCESS) {

                    if ((FileDescriptorPointer = EEFS_LibFileDescriptor2Pointer(OpenFileDescriptor->FileDescriptor)) != NULL) {
                        
                        Status = EEFS_LibRename(FileDescriptorPointer->InodeTable, EEFS_StatBuffer.Filename, NewFilename);
                        if (Status == EEFS_SUCCESS) {
                            ReturnCode = OK;
                        }
                        else if (Status == EEFS_READ_ONLY_FILE_SYSTEM) {
                            ReturnCode = ERROR;
                            errnoSet(EROFS);
                        }
                        else { /* error renaming file */
                            ReturnCode = ERROR;
                            errnoSet(EACCES);
                        }
                    }
                    else { /* invalid EEFS file descriptor */
                        ReturnCode = ERROR;
                        errnoSet(EBADF);
                    }
                }
                else { /* error getting stat */
                    ReturnCode = ERROR;
                    errnoSet(EBADF);
                }
            }
            else { /* not a EEFS_FILE file descriptor */
                ReturnCode = ERROR;
                errnoSet(EBADF);
            }
        }
        else { /* invalid NewFilename */
            ReturnCode = ERROR;
            errnoSet(EINVAL);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_Rename() */

/* Checks the file system for errors and dumps the Inode Table */
int EEFS_ChkDsk(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, int Arg)
{
    int                     ReturnCode;

    /* unused parameter */
    (void)Arg;
    
    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_DIRECTORY) {

            ReturnCode = EEFS_LibChkDsk(OpenFileDescriptor->DirectoryDescriptor->InodeTable, 0);
        }
        else { /* not a EEFS_DIRECTORY file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_ChkDsk() */

/* Returns the file system free space */
int EEFS_FreeSpace(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, uint32 *FreeCount)
{
    int                     ReturnCode;
    
    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_DIRECTORY) {

            *FreeCount = OpenFileDescriptor->DirectoryDescriptor->InodeTable->FreeMemorySize;
            ReturnCode = OK;
        }
        else { /* not a EEFS_DIRECTORY file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);
    
} /* End of EEFS_FreeSpace() */

/* Returns the file system free space */
int EEFS_FreeSpace64(EEFS_OpenFileDescriptor_t *OpenFileDescriptor, uint64 *FreeCount)
{
    int                     ReturnCode;

    if (OpenFileDescriptor != NULL) {

        if (OpenFileDescriptor->Type == EEFS_DIRECTORY) {

            *FreeCount = OpenFileDescriptor->DirectoryDescriptor->InodeTable->FreeMemorySize;
            ReturnCode = OK;
        }
        else { /* not a EEFS_DIRECTORY file descriptor */
            ReturnCode = ERROR;
            errnoSet(EBADF);
        }
    }
    else { /* invalid open file descriptor */
        ReturnCode = ERROR;
        errnoSet(EBADF);
    }

    return(ReturnCode);

} /* End of EEFS_FreeSpace64() */

/* Strip leading slashes from the specified path */
char *EEFS_ExtractFilename(char *Path)
{
    uint32                  i;
    uint32                  Length = strlen(Path);

    for (i=0; i < Length; i++) {
        if (Path[i] != '/' && Path[i] != '.') break;
    }
    return(&Path[i]);
    
} /* End of EEFS_ExtractFilename() */

/************************/
/*  End of File Comment */
/************************/
