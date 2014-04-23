eefs
====

EEPROM File System

This is the EEPROM File System Project (EEFS). It is a simple file system for memory devices such as EEPROM, RAM, ROM, etc. Currently it is not intended for block oriented devices such as disks and flash devices.

It can be used as a simple file system to boot an embedded system running vxWorks, RTEMS, or even no operating system. An EEFS image can be created on the development host, providing a single file to burn into an image that is loaded on a target. The file system is easy to understand, debug, and dump. 

There are drivers for RTEMS, vxWorks, and there is a standalone API for systems that do not have a file system. 

There is even a "microeefs" interface that allows the lookup of a file from a single function. This allows the bootloader to locate an image in EEPROM by the file name with a minimal amount of code. 
Future releases will include the ability to allow multiple EEFS volumes ( volumes in RAM and EEPROM at the same time ) 


