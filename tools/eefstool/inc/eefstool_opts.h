
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
**  $Id: eefstool_opts.h 1.2 2011/08/09 11:34:24GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2011/08/09 11:34:24GMT-05:00 $
** $Revision: 1.2 $
** $Log: eefstool_opts.h  $
** Revision 1.2 2011/08/09 11:34:24GMT-05:00 acudmore 
** updated for eefs 2.0
** Revision 1.1 2009/12/10 14:37:49EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/inc/project.pj
** 
*/

#define FILENAME_SIZE 64

typedef struct 
{
    char            Filename1[FILENAME_SIZE];
    char            Filename2[FILENAME_SIZE];

    char            ImageFileName[FILENAME_SIZE];
    boolean         ImageFileNameEntered;

    char            BdmDeviceName[FILENAME_SIZE];
    boolean         BdmDeviceEntered;

    uint32          EEFSMemoryAddress;
    boolean         EEFSMemoryAddressEntered;

    boolean         DirectoryListingCommand;
    boolean         EepromUsageCommand;
    boolean         CopyFromCommand;
    boolean         CopyToCommand;
    boolean         DeleteCommand;
    boolean         RenameCommand;
    int             NeedArgs;
    boolean         CommandSelected;

} CommandLineOptions_t;

void SetCommandLineOptionsDefaults(CommandLineOptions_t *CommandLineOptions);
void ProcessCommandLineOptions(int argc, char *argv[], CommandLineOptions_t *CommandLineOptions);
void DisplayUsage(void);

