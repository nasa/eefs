# File:
#  $Id: Makefile 1.3 2011/08/09 11:44:56GMT-05:00 acudmore Exp  $
#
# Purpose:  This is the makefile for the bdmload program
#
# References:
#
#
# $Date: 2011/08/09 11:44:56GMT-05:00 $
# $Revision: 1.3 $
# $Log: Makefile  $
# Revision 1.3 2011/08/09 11:44:56GMT-05:00 acudmore 
# updated makefile
# Revision 1.2 2011/08/09 12:42:39EDT acudmore 
# Updated makefile for latest MMS tool changes
# Revision 1.1 2009/12/10 14:37:46EST acudmore 
# Initial revision
# Member added to project c:/MKSDATA/MKS-REPOSITORY/FSW-TOOLS-REPOSITORY/eeprom-filesystem/tools/eefstool/project.pj
# 

VPATH =  ./src:../../libraries/eepromfs:../../drivers/standalone_driver

INCPATH = -I./inc -I../../inc:../../drivers/standalone_driver

SYSINC = /opt/rtems-4.10/include
LIBDIR = /opt/rtems-4.10/lib

CFLAGS = -O -g -Wall -D_ix86_ -m32

BDMPROG = eefstool-bdm
IMGPROG = eefstool-img

all: $(BDMPROG)

IMGOBJS    = eefstool_driver_img.o eefstool_opts_img.o
BDMOBJS    = eefstool_driver_bdm.o eefstool_opts_bdm.o eeprom.o
COMMONOBJS = eefstool_main.o eefs_fileapi.o eefs_filesys.o eefs_swap.o

$(BDMPROG):  $(COMMONOBJS) $(BDMOBJS) /opt/rtems-4.10/lib/libBDM.a $(WIN9x_DRV)
	$(CC) $(CFLAGS) -L$(LIBDIR) -o $(BDMPROG) $(COMMONOBJS) $(BDMOBJS) -lBDM -ltblcf -lusb
	
$(IMGPROG):  $(COMMONOBJS) $(IMGOBJS) 
	$(CC) $(CFLAGS) -o $(IMGPROG) $(COMMONOBJS) $(IMGOBJS) 

eefstool_main.o: eefstool_main.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<
	
eefs_fileapi.o: eefs_fileapi.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefs_filesys.o: eefs_filesys.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefs_swap.o: eefs_swap.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefstool_driver_bdm.o: eefstool_driver_bdm.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefstool_opts_bdm.o: eefstool_opts_bdm.c
	gcc  $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefstool_driver_img.o: eefstool_driver_img.c
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eefstool_opts_img.o: eefstool_opts_img.c
	gcc  $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

eeprom.o: eeprom.c 
	gcc $(CFLAGS) $(INCPATH) -I$(SYSINC) -c $<

clean:
	rm -f $(BDMPROG) $(IMGPROG) $(BDMPROG).exe $(IMGPROG).exe *.o 
