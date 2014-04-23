##############################################################################
## GNU Makefile

#
# Supported MAKEFILE targets:
#   clean - deletes object files, executables, output files, and gcov files
#   all   - makes geneepromfs.exe
#

#
# VPATH specifies the search paths for source files outside of the current directory.  Note that
# all object files will be created in the current directory even if the source file is not in the 
# current directory.
#
VPATH := .

#
# INCLUDES specifies the search paths for include files outside of the current directory.  
# Note that the -I is required. 
#
INCLUDES := -I.
INCLUDES += -I../../inc

#
# FLIGHT_OBJS specifies flight software object files.
#
OBJS := cmdlineopt.o
OBJS += parser.o
OBJS += geneepromfs.o 
###############################################################################

COMPILER=gcc
LINKER=gcc

#
# Compiler and Linker Options
#

WARNINGS = -Wall -W -ansi -Werror -Wstrict-prototypes -Wundef
DEBUGGER = -g

COPT = $(WARNINGS) $(DEBUGGER) -D_ix86_ -m32 

LOPT = -lm

###############################################################################
## Rule to make the specified TARGET
##
geneepromfs: $(OBJS) 
	$(LINKER) $(LOPT) $(OBJS) -o geneepromfs 

###############################################################################
##  "C" COMPILER RULE
##
%.o: %.c
	$(COMPILER) -c $(COPT) $(INCLUDES) $<

##############################################################################
##

all: geneepromfs


clean ::
	rm -f *.o *.exe

# end of file
