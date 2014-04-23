
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
** File:
**  $Id: eefstool_driver.h 1.1 2009/12/10 14:37:49GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2009/12/10 14:37:49GMT-05:00 $
** $Revision: 1.1 $
** $Log: eefstool_driver.h  $
** Revision 1.1 2009/12/10 14:37:49GMT-05:00 acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/inc/project.pj
** 
*/

void *eefstool_copy_from_device(void *dest, const void *src, size_t n);

void *eefstool_copy_to_device(void *dest, const void *src, size_t n);

void eefstool_flush_device(void);

void eefstool_lock(void);

void eefstool_unlock(void);

void  eefstool_open_device (void);

void  eefstool_close_device(void);

