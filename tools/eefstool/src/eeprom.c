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
**  $Id: eeprom.c 1.1 2011/08/09 13:33:34GMT-05:00 acudmore Exp  $
**
** Purpose:  This file contains eeprom writing code.  Some
**           of the functions (Write_vb, Write_Virtual_Buffer,
**           Set_Page_Window) are modified from Alan's original
**           code for EEPROM read/write through a parallel
**           port using the BDM library.  The function 
**           Write_vb was modified to add a little-endian
**           to big-endian conversion and the polling for
**           the EEPROM Write to complete (this is needed
**           before every 4-byte Write).
**
** Modified (see above) by: Ji-Wei Wu, NASA/GSFC Code 582, 1/6/2004
*/
/*
** eeprom.c
**
** Purpose: This file contains routines that are used to write EEPROM.
**
** Written by: Alan Cudmore NASA/GSFC Code 582
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
#include "mms_types.h"

/*
** Defines
**
*/
#define SIZE_OF_BUFFER 512 

/*
** This macro defines the mask for buffer usage
** 128 = 0x80
** 127 = 0x7F
** ~(127) = 0xFFFFFF80, which is the mask.
*/
#define BUFF_MASK (~((unsigned int)(SIZE_OF_BUFFER - 1)))

/*
** Global Variables
*/
unsigned long window_set = FALSE;
unsigned long lower_address = 0;
unsigned long upper_address = 0;
unsigned char virtual_buffer[SIZE_OF_BUFFER];
int           DelayValue = 8000;

/*
** Function prototypes
*/
void  Write_Virtual_Buffer ( void );
void  Set_Page_Window( unsigned long address);
void  PollEEPROM(void);
void  store_ee_word(unsigned long adr, unsigned long v);
void  store_ee_half(unsigned long adr,unsigned short v);
void  store_ee_byte(unsigned long address, unsigned char v, unsigned char flush_buffer);
void  Write_vb(unsigned long *dest, unsigned long *src, unsigned long dwords );
void  Set_Page_Window( unsigned long address);

/*
** Code
*/

void PollEEPROM(void)
{
    unsigned long eeprom_status;

    /*
    ** Poll for the EEPROM Write to complete
    */
    bdmReadLongWord((unsigned long)(EEPROM_CONTROL_REGISTER ), &eeprom_status);
    while ((eeprom_status & EEPROM_BUSY_FLD) == 1)
    {
       bdmReadLongWord((unsigned long)(EEPROM_CONTROL_REGISTER ), &eeprom_status);
    }
}

/*
** Store a 32 bit word in eeprom
*/
void store_ee_word(unsigned long adr, unsigned long v)
{
    unsigned char *char_array;
    /* Non-zero for big-endian systems.  */
    static int big_endian_p;
    /* Which endian are we?  */
    {
        short tmp = 1;
        big_endian_p = *(char *) &tmp == 0;
    }

    char_array = (unsigned char *)&v;

    if(big_endian_p)
    {
        store_ee_byte(adr, char_array[0], FALSE);
        store_ee_byte(adr+1, char_array[1], FALSE);
        store_ee_byte(adr+2, char_array[2], FALSE);
        store_ee_byte(adr+3, char_array[3], FALSE);
    }
    else
    {
        store_ee_byte(adr, char_array[3], FALSE);
        store_ee_byte(adr+1, char_array[2], FALSE);
        store_ee_byte(adr+2, char_array[1], FALSE);
        store_ee_byte(adr+3, char_array[0], FALSE);
    }

}

/*
** Store a 16 bit word in EEPROM
*/
void store_ee_half(unsigned long adr,unsigned short v)
{
    unsigned char *char_array;
    /* Non-zero for big-endian systems.  */
    static int big_endian_p;
    /* Which endian are we?  */
    {
        short tmp = 1;
        big_endian_p = *(char *) &tmp == 0;
    }

    char_array = (unsigned char *)&v;

    if(big_endian_p)
    {
        store_ee_byte(adr,   char_array[0], FALSE);
        store_ee_byte(adr+1, char_array[1], FALSE);
    }
    else
    {
        store_ee_byte(adr,   char_array[1], FALSE);
        store_ee_byte(adr+1, char_array[0], FALSE);
    }

}
/*
** Store a byte in EEPROM
*/
void store_ee_byte(unsigned long address, unsigned char v, unsigned char flush_buffer)
{

   if (  window_set == FALSE )
   {
      Set_Page_Window(address);
   }
   else if  ( flush_buffer == TRUE )
   {
      Write_Virtual_Buffer();
      return;
   }
   else if ( address <  lower_address || address > upper_address )
   {
      Write_Virtual_Buffer();
      Set_Page_Window(address);
    }
    /*
    ** Add the data to the buffer
    */
    virtual_buffer[address-lower_address] = v;
    
}

void  Write_vb(unsigned long *dest, unsigned long *src, unsigned long dwords )
{
    int            j;
    unsigned char  char_array[4];

    #ifdef EEFSTOOL_DEBUG
       printf("Writing out EEPROM buffer\n");
    #endif
    printf(".");
    fflush(stdout);

    /* Non-zero for big-endian systems.  */
    static int big_endian_p = 0;
    static int board_big_endian_p = 1;   /* Assume big-endian for coldfire */

    #ifdef EEFSTOOL_DEBUG
       printf("Before writing buffer to eeprom\n"); 
       printf("   --> dest = 0x%08X\n",dest); 
       printf("   --> src = 0x%08X\n",src); 
       printf("   --> dwords = 0x%08X\n",dwords); 
    #endif

    for ( j = 0; j <= dwords; j++ )
    {
        /*
        ** little-endian to big-endian conversion if needed
        */
        if (big_endian_p != board_big_endian_p)
        {
           char_array[3] = (unsigned char)src[j];
           char_array[2] = (unsigned char)(src[j] >> 8);
           char_array[1] = (unsigned char)(src[j] >> 16);
           char_array[0] = (unsigned char)(src[j] >> 24);
           src[j] = *(unsigned long *)char_array;
        }

        /* PollEEPROM();  */
        /*
        ** write
        */
        #ifdef EEFSTOOL_DEBUG
           printf("Doing bdmWriteLongWord \n");
        #endif

        if (bdmWriteLongWord((unsigned long)&dest[j], (unsigned long)src[j]) < 0)
           showError("bdmWriteLong");
        /*
        ** delay/poll
        */
        /* PollEEPROM(); */
        
        usleep(DelayValue); 
   }
   #ifdef EEFSTOOL_DEBUG
      printf("After writing buffer to eeprom\n"); 
   #endif
}

/*
** Write_Virtual_Buffer
*/
void Write_Virtual_Buffer(void)
{
    unsigned long                  j;
    unsigned long                 *buffer_ptr;
    unsigned long                 *eeprom_ptr;
    unsigned long                  lp;

    #ifdef EEFSTOOL_DEBUG  
       printf("In Write_Virtual_Buffer\n");
    #endif

    /*
    ** Is there anything to flush?
    */
    if ( window_set == TRUE )
    {
         #ifdef EEFSTOOL_DEBUG  
            printf("In Write_Virtual_Buffer -- Window set, so write\n");
         #endif

         /*
         ** Write out virtual buffer to EEPROM
         */

         /* Assign a local pointer to the buffer */
         buffer_ptr = (unsigned long *)virtual_buffer;
         eeprom_ptr = (unsigned long *)lower_address;

         /*
         ** do the write
         */
         Write_vb(eeprom_ptr, buffer_ptr,(upper_address-lower_address)/4);


         /*
         ** compare virtual buffer to EEPROM
         */
         buffer_ptr = (unsigned long *)virtual_buffer;
         eeprom_ptr = (unsigned long *)lower_address;
         for ( j = 0; j <= (upper_address-lower_address)/4; j++ )
         {
             bdmReadLongWord((unsigned long)&eeprom_ptr[j], &lp);
             if ( lp != buffer_ptr[j])
                 printf("EEPROM VERIFY ERROR: Addr = %lx, src=%lx, dest=%lx\n",(unsigned long)&eeprom_ptr[j], buffer_ptr[j], lp);
         }

    } /* end if window_set == TRUE */
    #ifdef EEFSTOOL_DEBUG
    else
    {
         printf("In Write_Virtual_Buffer -- Window not set, skip write\n");
    }
    #endif

    /*
    ** Make sure the window is invalidated
    */
    window_set = FALSE;
}


/*
** Set_Page_Window
*/
void Set_Page_Window( unsigned long address)
{
   unsigned long i,j;
   unsigned char *buffer_ptr;

   buffer_ptr = (unsigned char *)virtual_buffer;
   window_set = TRUE;

   /*
   ** Set address range
   */
   if( address >= (unsigned long)PROM_START && address <= (unsigned long)PROM_END)
   {
      lower_address = address & BUFF_MASK; /* for a SIZE_OF_BUFFER byte buffer */
      if (lower_address < PROM_START)
	   lower_address = PROM_START;
      upper_address = lower_address + SIZE_OF_BUFFER - 1;
      if (upper_address > PROM_END)
	   upper_address = PROM_END;
      #ifdef EEFSTOOL_DEBUG
         printf("PROM Window Set: From: 0x%X to 0x%X\n",(unsigned int)lower_address,
                                                      (unsigned int )upper_address); 
      #endif
   }
   else
   {
      lower_address = address & BUFF_MASK; /* for a SIZE_OF_BUFFER byte buffer */
      if (lower_address < EEPROM_START)
	   lower_address = EEPROM_START;
      upper_address = lower_address + SIZE_OF_BUFFER - 1;
      if (upper_address > EEPROM_END)
	   upper_address = EEPROM_END;
  
      #ifdef EEFSTOOL_DEBUG
         printf("EEPROM Window Set: From: 0x%X to 0x%X\n",(unsigned int)lower_address,
                                                      (unsigned int )upper_address); 
      #endif
   }
   
   /*
   ** Read in the current EEPROM contents
   */
   j = 0;
   for ( i = lower_address; i <= upper_address; i++ )
   {
       bdmReadByte((unsigned long)i, &buffer_ptr[j]);
       j = j + 1;
   }
}

