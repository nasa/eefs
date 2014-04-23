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
** Filename: eefs_vxworks.h
**
** Purpose: This file contains vxWorks Device Driver functions for the EEPROM File System.
**
** Design Notes:
**
** 1. Call EEFS_DrvInstall() to add the device driver to the vxWorks driver table.  This function should only be called
**    once during initialization.
** 2. Call EEFS_DevCreate() for each file system to add it to the vxWorks device list.  Separate device descriptos must
**    be declared for each file system.
** 
** For Example:
**
** EEFS_DeviceDescriptor_t EEFS_Bank1;
** EEFS_DeviceDescriptor_t EEFS_Bank2;
**
** Startup()
** {
**     int DriverNumber;
**
**     DriverNumber = EEFS_DrvInstall();
**     EEFS_DevCreate(DriverNumber, "/EEFS1", EEFS_Bank1StartAddress, &EEFS_Bank1);
**     EEFS_DevCreate(DriverNumber, "/EEFS2", EEFS_Bank2StartAddress, &EEFS_Bank2);
** }
**
** References:
*/

#ifndef _eefs_vxworks_
#define	_eefs_vxworks_

/*
 * Includes
 */

#include "common_types.h"
#include "eefs_fileapi.h"
#include "iosLib.h"

/*
 * Type Definitions
 */

typedef struct {
    DEV_HDR                     DeviceHeader;
    EEFS_InodeTable_t           InodeTable;
} EEFS_DeviceDescriptor_t;

/*
 * Exported Functions
 */

/* Adds the eeprom file system driver to the vxWorks driver table and returns a index or driver number to the slot in
 * the driver table where the driver was installed.  This function should only be called once during startup.  Returns
 * a driver number on success or ERROR if there was an error. */
int                 EEFS_DrvInstall(void);

/* Add a eeprom file system device to the vxWorks device list. The device name must start with a slash, ex. /eefs1.  The
 * base address is the start address of the file system in eeprom.  The device descriptor is initialized by this function
 * however you must allocate memory for the device descriptor.  Returns OK on success or ERROR if there was an error.  */
int                 EEFS_DevCreate(int DriverNumber, char *DeviceName, uint32 BaseAddress, EEFS_DeviceDescriptor_t *DeviceDescriptor);

/* Remove a eeprom file system device from the vxWorks device list.  Returns OK on success or ERROR if there
 * was an error.  */
int                 EEFS_DevDelete(EEFS_DeviceDescriptor_t *DeviceDescriptor);

#endif

/************************/
/*  End of File Comment */
/************************/
