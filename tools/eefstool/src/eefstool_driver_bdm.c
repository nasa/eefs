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
**  $Id: eefstool_driver_bdm.c 1.2 2011/08/09 11:42:56GMT-05:00 acudmore Exp  $
**
** Purpose:  
**
** $Date: 2011/08/09 11:42:56GMT-05:00 $
** $Revision: 1.2 $
** $Log: eefstool_driver_bdm.c  $
** Revision 1.2 2011/08/09 11:42:56GMT-05:00 acudmore 
** EEFS 2.0 changes for eefstool
** Revision 1.3 2010/05/13 15:59:06EDT acudmore 
** fixed bugs in bdm code
** Revision 1.2 2010/05/13 09:41:07EDT acudmore 
** Update EEFS tool
** Revision 1.1 2010/05/06 15:09:03EDT acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eefs/tools/eefstool/src/project.pj
** Revision 1.2 2010/04/30 16:13:04EDT acudmore 
** Updated eefstool BDM port to support MMS ETU
** Revision 1.1 2009/12/16 10:41:38EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/MMS-REPOSITORY/dev/eeprom-fsw/eeprom-filesystem/tools/eefstool/src/project.pj
** Revision 1.1 2009/12/10 14:37:52EST acudmore 
** Initial revision
** Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/src/project.pj
** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <BDMlib.h>
#include "common_types.h"
#include "eefs_filesys.h"
#include "eefstool_opts.h"
#include "cf_regs.h"
#include "mms_types.h" 

/*
** Prototypes from eeprom.c
*/
void Write_Virtual_Buffer(void);
void store_ee_byte(unsigned long address, unsigned char v, unsigned char flush_buffer);

/*
** Global variables
*/
extern CommandLineOptions_t CommandLineOptions;


/*
** Code
*/
void cleanExit (int exit_code)
{
  if (bdmIsOpen ())
  {
    bdmSetDriverDebugFlag (0);
    bdmClose ();
  }
  exit (exit_code);
}

void showError (char *msg)
{
    printf ("%s failed: %s\n", msg, bdmErrorString ());
    cleanExit (1);
}

/*
** Configure the processor through the BDM
*/
void configureProcessor(void)
{
    printf("configureProcessor call started\n");

    /*
    ** Internal SRAM module enabled, address = 0x70000000
    ** All access types (supervisor and user) allowed
    */
    if (bdmWriteSystemRegister(BDM_REG_RAMBAR, 0x70000001) < 0)
        showError("I-SRAM enable");

    /*
    ** Base address of internal peripherals (MBAR) = 0x10000000
    ** All access types (supervisor and user) allowed
    */
    if (bdmWriteSystemRegister(BDM_REG_MBAR, MCF_MBAR+1) < 0)
        showError("MBAR setup");

    /*
    ** Disable all sources of interrupts
    */
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR0, 0x0) < 0)
        showError("Interrupt 0\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR1, 0x0) < 0)
        showError("Interrupt 1\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR2, 0x0) < 0)
        showError("Interrupt 2\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR3, 0x0) < 0)
        showError("Interrupt 3\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR4, 0x0) < 0)
        showError("Interrupt 4\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR5, 0x0) < 0)
        showError("Interrupt 5\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR6, 0x0) < 0)
        showError("Interrupt 6\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR7, 0x0) < 0)
        showError("Interrupt 7\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR8, 0x0) < 0)
        showError("Interrupt 8\n");
    if (bdmWriteByte(MBAR_BASE+MCFSIM_ICR9, 0x0) < 0)
        showError("Interrupt 9\n");

    /*
    ** Disable Watchdog Timer
    */
    if (bdmWriteByte(MBAR_BASE+MCFSIM_SYPCR, 0x0) < 0)
       showError("Disable watchdog timer\n");

    /*
    ** Disable and invalidate cache
    */
    if (bdmWriteSystemRegister(BDM_REG_CACR, 0x01000000) < 0)
        showError("Disable/invalidate cache\n");

    /*
    ** Configure Chip Select Modules
    */
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR0, 0x0) < 0)
       showError("CSAR 0\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR0, 0x01FF0001) < 0)
       showError("CSMR 0\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR0, 0x18) < 0)
        showError("CSCR 0\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR1, 0x0) < 0)
        showError("CSAR 1\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR1, 0x0) < 0)
        showError("CSMR 1\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR1, 0x0) < 0)
        showError("CSCR 1\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR2, 0x0) < 0)
        showError("CSAR 2\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR2, 0x0) < 0)
        showError("CSMR 2\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR2, 0x0) < 0)
        showError("CSCR 2\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR3, 0x0) < 0)
        showError("CSAR 3\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR3, 0x0) < 0)
        showError("CSMR 3\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR3, 0x0) < 0)
        showError("CSCR 3\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR4, 0x0) < 0)
        showError("CSAR 4\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR4, 0x0) < 0)
        showError("CSMR 4\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR4, 0x0) < 0)
       showError("CSCR 4\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSAR7, 0x0) < 0)
        showError("CSAR 7\n");
    if (bdmWriteLongWord(MBAR_BASE+MCFSIM_CSMR7, 0x0) < 0)
        showError("CSMR 7\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_CSCR7, 0x0) < 0)
        showError("CSCR 7\n");


    /*
    ** Disable the cache on the SRAM (leave cache enabled on PROM)
    */ 
    if (bdmWriteSystemRegister(BDM_REG_CACR, 0x01000000) < 0)
        showError("CACR\n");
    if (bdmWriteSystemRegister(BDM_REG_ACR0, 0x0) < 0)
        showError("ACR0\n");
    if (bdmWriteSystemRegister(BDM_REG_ACR1, 0x0) < 0)
        showError("ACR1\n");

    /*
    ** Clear the PAR and program the direction register
    */
    if (bdmWriteWord(MBAR_BASE+MCFSIM_PAR, 0x0) < 0)
        showError("PAR\n");
    if (bdmWriteWord(MBAR_BASE+MCFSIM_PADDR, 0x0) < 0)
        showError("PADDR\n");

    /*
    ** Power on EEPROM banks 1 and 2
    */
    bdmWriteLongWord(EEPROM_CONTROL_REGISTER, 0x0 ); 

    printf("Powered on EEPROM banks 1 and 2\n");

    /*
    ** Arm EEPROM writes
    */
    bdmWriteLongWord(EEPROM_ARM_REGISTER, EEPROM_ARM_VALUE_1);
    bdmWriteLongWord(EEPROM_ARM_REGISTER, EEPROM_ARM_VALUE_2);
   
    usleep(1000);
    printf("Armed EEPROM Writes\n");
    printf("configureProcessor call completed\n");
}

/*
** in eefstool_copy_from_device, assume the src is BDM
**
*/
void *eefstool_copy_from_device(void *dest, const void *src, size_t n)
{
  size_t    length = n;
  char     *temp_dest = (char *) dest;
  uint32    temp_src = (uint32) src;


  #ifdef EEFSTOOL_DEBUG
     printf("eefstool_copy_from_device called\n");
  #endif

  void *save = (void *)dest;

  while (length--)
  {
      bdmReadByte(temp_src, (unsigned char *)temp_dest); /* printf("0x%02X\n",*temp_dest); */
      temp_src = temp_src + 1;
      temp_dest = temp_dest + 1; 
  }
  return save;    
}

/*
** In eefstool_copy_to_bdm, assume the dest is the BDM
*/
void *eefstool_copy_to_device(void *dest, const void *src, size_t n)
{
  size_t    length = n;
  uint32    temp_dest = (uint32)dest;
  char     *temp_src =  (char *)src;

  #ifdef EEFSTOOL_DEBUG
     printf("eefstool_copy_to_device called: src = 0x%08X, dest = 0x%08X, size = %d\n",
           (unsigned long )dest, (unsigned long)src, n);
  #endif

  void *save = (void *)src;

  while (length--)
  {
      store_ee_byte(temp_dest,(unsigned char )temp_src[0], FALSE);
      temp_src = temp_src + 1;
      temp_dest = temp_dest + 1;
  }
  return save;
}

void eefstool_flush_device(void)
{
    #ifdef EEFSTOOL_DEBUG
       printf("eefstool_flush_device\n");
    #endif
    Write_Virtual_Buffer();
}

void eefstool_lock(void)
{
    return;
}

void eefstool_unlock(void)
{
   return;
}

void eefstool_open_device (void)
{

    /*
    ** Open the BDM port
    */
    if (bdmOpen (CommandLineOptions.BdmDeviceName) < 0)
    {
      showError ("Open");
    }
    
    if (bdmIsOpen () < 0)
    {
       showError ("BDM port being open");
    }

    /*
    ** Stop the processor to configure it
    */
    if (bdmStop() < 0)
    {
       showError("Stop");
    }

    /*
    ** Configure the processor 
    */
    /* Not necessary for the COTS board, but will be used for the ETU/Flight board */
    configureProcessor(); 

}

void eefstool_close_device(void)
{
    /*
    ** Flush the EEPROM
    */
    Write_Virtual_Buffer();

    /*
    ** Restart the processor
    */
    printf("Restarting the Coldfire.\n");
    if (bdmGo () < 0)
    {
       showError ("Go");
    }

    /*
    ** Exit the program
    */
    cleanExit (0);
}
