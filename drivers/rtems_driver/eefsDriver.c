/*
**
**      Copyright (c) 2010-2014, United States government as represented by the 
**      administrator of the National Aeronautics Space Administration.  
**      All rights reserved. This software was created at NASAs Goddard 
**      Space Flight Center pursuant to government contracts.
**
**      This is governed by the NASA Open Source Agreement and may be used, 
**      distributed and modified only pursuant to the terms of that agreement.
*/

/*
** NASA EEPROM File System ( EEFS ) RTEMS driver
**   This is an RTEMS file system driver for the EEPROM file system.
**   The EEPROM File system defines the disk structure, inode table, and API.
**   This driver allows the EEFS to be used as a native RTEMS file system.
**
** Alan Cudmore
** NASA/GSFC Code 582.0
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <rtems.h>
#include <rtems/libio.h>
#include <rtems/libio_.h>
#include <rtems/seterr.h>
#include <dirent.h>

/*
** EEFS includes
*/
#include "common_types.h"
#include "eefs_fileapi.h"
#include "eefs_macros.h"

/*
** Prototypes that are not in headers
*/
int32 EEFS_LibFindFile(EEFS_InodeTable_t *InodeTable, char *Filename);

/*
** External variables used by this driver.
** The following variables are used to set the address
** for each EEFS bank or device.
** This is necessary to provide flexibility and to avoid
** Hardcoding the addresses in the driver.
*/
extern uint32_t rtems_eefs_a_address;
extern uint32_t rtems_eefs_b_address;

/*
** Defines for EEFS RTEMS devices
*/
#define EEFS_BANK1_DEVICE  "/dev/eefsa"
#define EEFS_BANK2_DEVICE  "/dev/eefsb"

#define EEFS_VOLUME_SEMAPHORE_TIMEOUT    RTEMS_NO_TIMEOUT

/*
** Define EEFS Node types
*/
#define ROOT_INODE 0x1234
#define FILE_INODE 0xFFFF

/*
** Special Inode Number for a Pending MKNOD
** This limits the number of files allowed in an
** EEFS volume to 65518.
** Given the 2MB bank/volume limit, this should
** not be a problem.
*/
#define EEFS_PENDING_INODE 0xFFEE

/*
** Define for dev in stat struct 
*/
#define EEFS_DEVICE 0xEEF5

/*
** Define for success return code
*/
#define RC_OK 0

/*
** Macros for paths and node types
*/
#define rtems_eefs_current_dir(_p) \
  ((_p[0] == '.') && ((_p[1] == '\0') || rtems_filesystem_is_separator (_p[1])))

#define rtems_eefs_parent_dir(_p) \
  ((_p[0] == '.') && (_p[1] == '.') && \
   ((_p[2] == '\0') || rtems_filesystem_is_separator (_p[2])))

#define eefs_info_mount_table(_mt) ((eefs_info_t*) ((_mt)->fs_info))

#define eefs_info_pathloc(_pl)     ((eefs_info_t*) ((_pl)->mt_entry->fs_info))

#define eefs_info_iop(_iop)        (eefs_info_pathloc (&((_iop)->pathinfo)))


/*
** EEFS File system info.
*/
typedef struct eefs_info_s 
{
    /*
    ** File system flags
    */
    uint32_t flags;

    /*
    ** Mutex to protect FS data structures
    */
    rtems_id eefs_mutex;

    /*
    ** Variable to coordinate a mknod/eval path call. 
    ** Because of the way the EEFS works when creating a new file, we cannot 
    ** actually create a new file when the "mknod" call is made. 
    ** This would be OK, but in the RTEMS "open" call, the "mknod" is called,
    ** followed by the "eval path" call, then the "open" call.
    ** If we don't create the file in the EEFS in the "mknod" call, the 
    ** "eval path" call will fail. 
    ** The solution is to record the pending file create, then allow the 
    ** open to actually create it.
    */
    uint32_t mknod_pending;

    /*
    ** File name for the pending mknod call
    */
    char  mknod_pending_name[EEFS_MAX_FILENAME_SIZE];

    /*
    ** Keep a local copy of the EEFS inode table
    */
    EEFS_InodeTable_t                 eefs_inode_table;

	
} eefs_info_t;

/*
** Forward declarations for the OPS tables used in the
** file system
*/
const rtems_filesystem_operations_table  rtems_eefs_ops;
const rtems_filesystem_file_handlers_r   rtems_eefs_file_handlers;
const rtems_filesystem_file_handlers_r   rtems_eefs_dir_handlers;

/* 
** rtems_eefs_initialize 
**     This function handles the EEFS file system initialization when the 
**     filesystem is mounted by RTEMS. It creates the filesystem specific
**     data and populates the EEFS inode table.
**
** PARAMETERS:
**     mt_entry- RTEMS file system mount entry
**     data    - pointer for extra data, not used.
**
** RETURNS:
**     RC_OK or -1 if error occured (errno set appropriately)
*/
int rtems_eefs_initialize ( rtems_filesystem_mount_table_entry_t *mt_entry,
                                   const void *data )
{
   eefs_info_t     *fs;
   rtems_status_code  sc;
   int32              Status;

   mt_entry->mt_fs_root.handlers = &rtems_eefs_dir_handlers;
   mt_entry->mt_fs_root.ops      = &rtems_eefs_ops;

   /*
   ** Allocate file system specific data structure
   */
   fs = malloc (sizeof (eefs_info_t));
   if (!fs)
   {
      rtems_set_errno_and_return_minus_one (ENOMEM);
   }

   mt_entry->fs_info                  = fs;
   mt_entry->mt_fs_root.node_access   = (void *)ROOT_INODE;
   mt_entry->mt_fs_root.node_access_2 = (void *)EEFS_FILE_NOT_FOUND;
  
   /*
   ** Initalize the EEFS
   */
   if ( strcmp(mt_entry->dev, EEFS_BANK1_DEVICE) == 0 )
   {
      #ifdef EEFS_DEBUG
         printf("Mounting EEFS Bank 1 at: 0x%08X\n", (unsigned int )rtems_eefs_a_address);
      #endif
      Status =  EEFS_LibInitFS( &(fs->eefs_inode_table),  (uint32)rtems_eefs_a_address );
   }
   else 
   {
      #ifdef EEFS_DEBUG
         printf("Mounting EEFS Bank 2 at: 0x%08X\n", (unsigned int )rtems_eefs_a_address);
      #endif
      Status =  EEFS_LibInitFS( &(fs->eefs_inode_table), (uint32 )rtems_eefs_b_address);
   }
    
   if (Status != EEFS_SUCCESS)
   {
      #ifdef EEFS_DEBUG
         printf("Error: EEFS_LibInitFS call failed\n");
      #endif
      free(fs);
      rtems_set_errno_and_return_minus_one (EIO);
   }
	
   /*
   ** Create the semaphore for the internal data
   */
   sc = rtems_semaphore_create (rtems_build_name('E', 'E', 'F', 's'), 1,
         RTEMS_PRIORITY |
         RTEMS_BINARY_SEMAPHORE |
         RTEMS_INHERIT_PRIORITY |
         RTEMS_NO_PRIORITY_CEILING |
         RTEMS_LOCAL,
         0,
         &fs->eefs_mutex
         );
   if (sc != RTEMS_SUCCESSFUL)
   {
      rtems_set_errno_and_return_minus_one (ENOMEM);
   }
   return(RC_OK);
}

/* 
** rtems_eefs_shutdown
**     Function to unmount/shutdown the instance of the EEFS file system.
**
** PARAMETERS:
**     mt_entry -- Original mount entry for the file system to shut down.
**
** RETURNS:
**     RC_OK or -1 if error occured (errno set appropriately)
*/
static int rtems_eefs_shutdown (rtems_filesystem_mount_table_entry_t* mt_entry)
{
   eefs_info_t *fs = eefs_info_mount_table (mt_entry);

   rtems_semaphore_delete (fs->eefs_mutex);
   free (fs);
   return(RC_OK);
}

/* 
** rtems_eefs_evaluate_for_make 
**     The following routine evaluate path for a new node to be created.
**     'pathloc' is returned with a pointer to the parent of the new node.
**     'name' is returned with a pointer to the first character in the
**     new node name.  The parent node is verified to be a directory.
**
** PARAMETERS:
**     path    - path for evaluation
**     pathloc - IN/OUT (start point for evaluation/parent directory for
**               creation)
**     name    - new node name
**
** RETURNS:
**     RC_OK, filled pathloc for parent directory and name of new node on
**     success, or -1 if error occured (errno set appropriately)
*/
static int rtems_eefs_evaluate_for_make(
   const char                         *path,       /* IN     */
   rtems_filesystem_location_info_t   *pathloc,    /* IN/OUT */
   const char                        **name        /* OUT    */
)
{
   eefs_info_t       *fs =  pathloc->mt_entry->fs_info;
   rtems_status_code  sc = RTEMS_SUCCESSFUL;
   int32              InodeIndex;
   int                i;
   int                slashFound;
   int                pathLen;

   #ifdef EEFS_DEBUG
      printf("eefs_evaluate_for_make\n");
   #endif 

   /*
   **  - Make sure the pathloc is the ROOT_INODE
   **  - Make sure the new filename does not contain any directory separators
   **  - Make sure the file does not already exist in the EEFS
   **  Return the name
   */
   if ((int)pathloc->node_access == ROOT_INODE )
   {

       pathLen = strlen(path);

       /*
       ** Check to see if the path has a ./ to strip from the front
       */ 
       if ( path[0] == '.' && path[1] == '/' )
       {
          path = path + 2;
          pathLen = pathLen - 2;
       }

       /*
       ** Check to see if the path has any separators
       **  it should not have any, since the EEFS does not 
       **  support sub-directories.
       */
       slashFound = 0;
       for ( i = 0; i < pathLen; i++ )
       {
          if ( path[i] == '/' )
          {
             slashFound = 1;
          }
       }

       if ( slashFound == 0 )
       {
          sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                      EEFS_VOLUME_SEMAPHORE_TIMEOUT);
          if (sc != RTEMS_SUCCESSFUL)
          {
             rtems_set_errno_and_return_minus_one(EIO);
          }

          /*
          ** Check to see if the file exists in the EEFS 
          */
          InodeIndex = EEFS_LibFindFile(&(fs->eefs_inode_table), (char *)path);
          if ( InodeIndex == EEFS_FILE_NOT_FOUND )
          {
             /*
             ** file does not exist, so it's ready to create
             */
             *name = path;
             pathloc->handlers = &rtems_eefs_file_handlers;
             rtems_semaphore_release(fs->eefs_mutex);
          }
          else
          {
             rtems_semaphore_release(fs->eefs_mutex);
             rtems_set_errno_and_return_minus_one (ENOENT);
          }
       }
       else
       {
          /*
          ** Path contains the / character
          */
          rtems_set_errno_and_return_minus_one (ENOTSUP);
       }    
   }
   else
   {
      rtems_set_errno_and_return_minus_one (ENOENT);
   }
   return(RC_OK);
}
/* 
** rtems_eefs_eval_path 
**
** Evaluate the path to a node that wishes to be accessed. The pathloc is
** returned with the ino to the node to be accessed.
**
** The routine starts from the root stripping away any leading path separators
** breaking the path up into the node names and checking an inode exists for
** that node name. Permissions are checked to insure access to the node is
** allowed. A path to a node must be accessable all the way even if the end
** result is directly accessable. As a user on Linux try "ls /root/../tmp" and
** you will see if fails.
**
** The whole process is complicated by crossmount paths where we head down into
** this file system only to return to the top and out to a another mounted file
** system. For example we are mounted on '/e' and the user enters "ls
** /e/a/b/../../dev". We need to head down then back up.
**
** PARAMETERS:
**     path    - path for evaluation
**     pathlen - length of path string
**     pathloc - IN/OUT (start point for evaluation/parent directory for
**               path evaluation)
**     flags   - permission flags - not used on EEFS
**
** RETURNS:
**     RC_OK, filled pathloc for parent directory and name of node on
**     success, or -1 if error occured (errno set appropriately)
*/

int rtems_eefs_eval_path (const char*                        path,
                           size_t                            pathlen,
                           int                               flags,
                           rtems_filesystem_location_info_t* pathloc)
{
   int                               returnCode = 0;
   int32                             InodeIndex;
   eefs_info_t                      *fs =  pathloc->mt_entry->fs_info;
   rtems_filesystem_location_info_t  newloc;
   rtems_status_code                 sc = RTEMS_SUCCESSFUL;

   #ifdef EEFS_DEBUG
      printf("eefs_eval_path: %s\n",path);
   #endif 

   /*
   ** get rid of a leading ( ./ ) 
   */
   if ( path[0] == '.' && path[1] == '/' )
   {
      path = path + 2;
      pathlen = pathlen - 2;
   }

   /*
   ** Lock FS 
   */
   sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
   if (sc != RTEMS_SUCCESSFUL)
   {
       rtems_set_errno_and_return_minus_one(EIO);
   }
    
   if ((*path == '\0') || (pathlen == 0))
   {
       pathloc->node_access = (void *)ROOT_INODE;
       pathloc->node_access_2 = (void *)EEFS_FILE_NOT_FOUND;
       pathloc->handlers = &rtems_eefs_dir_handlers;
   }
   else if (rtems_eefs_current_dir (path))
   {
       pathloc->node_access = (void *)ROOT_INODE;
       pathloc->node_access_2 = (void *)EEFS_FILE_NOT_FOUND;
       pathloc->handlers = &rtems_eefs_dir_handlers;
   }     
   else if (rtems_eefs_parent_dir (path))
   {
       /*
       ** This case is for a path that is for the file system above
       ** ( for example: "../dev" )
       ** The path must be evaluated by the parent file system 
       */
       pathloc->node_access = (void *)ROOT_INODE;
       pathloc->node_access_2 = (void *)EEFS_FILE_NOT_FOUND;
       rtems_semaphore_release(fs->eefs_mutex);
       newloc = pathloc->mt_entry->mt_point_node;
       *pathloc = newloc;
       /*
       ** call the parent's eval path function
       */
       return (*pathloc->ops->evalpath_h)(path, pathlen,
                                          flags, pathloc);
    }
    else 
    {
       /*
       ** Check to see if there is a mknod pending
       ** This is a case where the file does not actually exist yet
       */
       if ((fs->mknod_pending == TRUE) && (strncmp(fs->mknod_pending_name, path, EEFS_MAX_FILENAME_SIZE) == 0))
       {
             pathloc->node_access = (void *)FILE_INODE;
             pathloc->node_access_2 = (void *)EEFS_PENDING_INODE;
             pathloc->handlers = &rtems_eefs_file_handlers;
             rtems_semaphore_release(fs->eefs_mutex);
             return(RC_OK);
       }

       /*
       ** in the EEFS, the default must be a file.
       ** Sub-directories are not alowed
       */
       pathloc->node_access = (void *)FILE_INODE;

       /*
       ** Find out if the file exists.
       ** It will be an inode number or EEFS_FILE_NOT_FOUND
       */
       InodeIndex = EEFS_LibFindFile(&(fs->eefs_inode_table), (char *)path); 
       if ( InodeIndex == EEFS_FILE_NOT_FOUND )
       {
          rtems_semaphore_release(fs->eefs_mutex);
          rtems_set_errno_and_return_minus_one(ENOENT);
       }
       else
       {
          returnCode = 0;
          pathloc->node_access_2 = (void *)InodeIndex;
          pathloc->handlers = &rtems_eefs_file_handlers;
       }       
    
   }
   rtems_semaphore_release(fs->eefs_mutex);
   return (returnCode);
}

/* 
** rtems_eefs_statvfs
**     Return information about the mounted EEFS file system.
**     used for determining available space.
**
** PARAMETERS:
**     loc - IN - RTEMS location info structure for the file system to evaluate
**                ( assumed to be the EEFS by the time this gets called )
**     buf      - pointer to the users statvfs buffer
**
** RETURNS:
**     RC_OK with filled statvfs buffer, or -1 if error occured (errno set appropriately)
*/
int rtems_eefs_statvfs(
 rtems_filesystem_location_info_t  *loc,     /* IN  */
 struct statvfs                    *buf      /* OUT */
)
{
    eefs_info_t     *fs           = loc->mt_entry->fs_info;
    uint32_t         FreeBlocks;
    uint32_t         TotalBlocks;
    uint32_t         FreeInodes;

    #ifdef EEFS_DEBUG
       printf("eefs_statvfs\n");
    #endif

    FreeBlocks = fs->eefs_inode_table.FreeMemorySize / 512;
    TotalBlocks =  (2048 * 1024) / 512; 
    FreeInodes =  EEFS_MAX_FILES - fs->eefs_inode_table.NumberOfFiles;

    /*
    ** fill out the statvfs buffer
    */
    buf->f_bsize  = 512;              /* file system block size */
    buf->f_frsize = 512;              /* fragment size */
    buf->f_blocks = TotalBlocks;      /* size of fs in f_frsize units */
    buf->f_bfree  = FreeBlocks;       /* # free blocks */
    buf->f_bavail = FreeBlocks;       /* # free blocks for non-root */
    buf->f_files  = EEFS_MAX_FILES;   /* # inodes */
    buf->f_ffree  = FreeInodes;       /* # free inodes */
    buf->f_favail = FreeInodes;       /* # free inodes for non-root */
    buf->f_fsid   = 0;                /* file system ID */
    buf->f_flag   = 0;                /* mount flags */
    buf->f_namemax = EEFS_MAX_FILENAME_SIZE; /* max file system length */

    rtems_set_errno_and_return_minus_one(RC_OK);

}

/**************************************************************************/
/*   
**  File related functions
*/
/**************************************************************************/

/* 
** rtems_eefs_open
**     Open an existing file. See mknod for creation of a new file.
**
** PARAMETERS:
**     iop      - RTEMS iop structure, which keeps track of an open file.
**     pathname - pathname of the file to open
**     flags    - flags determining how the file is opened ( O_RDONLY etc.. )
**     mode     - permissions, which are not used on the EEFS
**
** RETURNS:
**     RC_OK with valid iop buffer, or -1 if error occured (errno set appropriately)
*/
static int rtems_eefs_open(
    rtems_libio_t *iop,
    const char    *pathname,
    uint32_t       flags,
    uint32_t       mode
)
{
   eefs_info_t     *fs;
   rtems_status_code  sc = RTEMS_SUCCESSFUL;
   int32              eefs_mode = 0;
   int32              eefs_fd;
   int32              eefs_status;
   EEFS_Stat_t        eefs_statbuffer;
   char              *cp1;
   char              *fileName = NULL;
   int                pathNameLen;
   int                i;
   int                slashFound = 0;

   #ifdef EEFS_DEBUG
      printf("eefs_open\n");
   #endif

   /*
   ** Get the file system info.
   */
   fs = eefs_info_iop (iop);

   /*
   ** Extract file from the pathname 
   */
   cp1 = (char *)pathname;
   pathNameLen = strlen(pathname);
   for ( i = 0; i < pathNameLen; i++ )
   {
      if ( pathname[i] == '/' )
      {
         slashFound = 1;
         cp1 = (char *)&(pathname[i]);
      }
   }
   if ( slashFound == 1 )
   {
      fileName = cp1 + 1;
   }
   else
   {
      fileName = cp1;
   }

   sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
   if (sc != RTEMS_SUCCESSFUL)
   {
       rtems_set_errno_and_return_minus_one(EIO);
   }

   /*
   ** If there is a mknod pending on this file, create a new file
   ** rather than opening an old file.
   */
   if ((fs->mknod_pending == TRUE) && (strncmp(fs->mknod_pending_name, fileName, EEFS_MAX_FILENAME_SIZE) == 0))
   {
      fs->mknod_pending = FALSE; 
      fs->mknod_pending_name[0] = 0;
   
      /*
      ** Create a file in EEFS
      */
      eefs_fd = EEFS_LibCreat(&(fs->eefs_inode_table), fileName, 0);
      if ( eefs_fd < 0 )
      {
         rtems_semaphore_release(fs->eefs_mutex);
         if ( eefs_fd == EEFS_NO_SPACE_LEFT_ON_DEVICE || eefs_fd == EEFS_NO_FREE_FILE_DESCRIPTOR )
         {
            rtems_set_errno_and_return_minus_one(ENOSPC);
         }
         else   
         {
            rtems_set_errno_and_return_minus_one(EIO);
         }
      }
	
      /*
      ** Store the file descriptor. This allows other calls to use the open file.
      */
      iop->file_info = (void *)eefs_fd;
    
      /*
      ** Set up the correct size and offset
      */
      iop->offset = 0;
      iop->size = 0; 
      rtems_semaphore_release(fs->eefs_mutex);
   }
   else
   {
      /*
      ** Opening an existing file
      */	

      /*
      ** Open file in EEFS
      */
      eefs_fd =  EEFS_LibOpen(&(fs->eefs_inode_table), fileName, flags, eefs_mode);
      if ( eefs_fd < 0 )
      {
         rtems_semaphore_release(fs->eefs_mutex);
         rtems_set_errno_and_return_minus_one(EIO);
      }
      /*
      ** call stat to get info about the file
      */
      eefs_status = EEFS_LibFstat(eefs_fd, &eefs_statbuffer);
      if ( eefs_fd < 0 )
      {
         rtems_semaphore_release(fs->eefs_mutex);
         rtems_set_errno_and_return_minus_one(EIO);
      }

      /*
      ** Store the file descriptor. This allows other calls to use the open file.
      */
      iop->file_info = (void *)eefs_fd;
    
      /*
      ** Set up the correct size and offset
      */
      if (iop->flags & LIBIO_FLAGS_APPEND)
      {
         iop->offset = eefs_statbuffer.FileSize;
      }
	
      /*
      ** Set the file size
      */ 
      iop->size = eefs_statbuffer.FileSize; 
      rtems_semaphore_release(fs->eefs_mutex);
   }

   return(RC_OK);    
}

/* 
** rtems_eefs_close
**     close an open EEFS file.
**
** PARAMETERS:
**     iop      - RTEMS iop structure, which keeps track of an open file.
**
** RETURNS:
**     RC_OK or -1 if error occured (errno set appropriately)
*/
static int rtems_eefs_close(
    rtems_libio_t *iop
)
{
   eefs_info_t     *fs;
   rtems_status_code  sc = RTEMS_SUCCESSFUL;
   int32              eefs_fd = (int32 )iop->file_info;
   int32              eefs_status;

   #ifdef EEFS_DEBUG
      printf("eefs_close\n");
   #endif

   /*
   ** Get the file system info.
   */
   fs = eefs_info_iop (iop);
	
   sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
   if (sc != RTEMS_SUCCESSFUL)
   {
       rtems_set_errno_and_return_minus_one(EIO);
   }
	
   /*
   ** Close the file in the EEFS
   */
   eefs_status = EEFS_LibClose(eefs_fd);
   if ( eefs_status < 0  )
   {
      rtems_semaphore_release(fs->eefs_mutex);
      rtems_set_errno_and_return_minus_one(EIO);
   }
	
   rtems_semaphore_release(fs->eefs_mutex);

   return(RC_OK);
}

/* 
** rtems_eefs_read
**     read data from an open EEFS file.
**
** PARAMETERS:
**     iop      - RTEMS iop structure, which keeps track of an open file.
**     buffer   - pointer to the user's buffer for the data
**     count    - number of bytes of data requested by the user
**
** RETURNS:
**     number of bytes of data read, 0 for EOF, or -1 with errno set for errors.
*/
static ssize_t rtems_eefs_read(
                               rtems_libio_t *iop,
                               void          *buffer,
                               size_t         count
							   )
{	
    ssize_t            ret = 0;
    rtems_status_code  sc = RTEMS_SUCCESSFUL;
    eefs_info_t       *fs = iop->pathinfo.mt_entry->fs_info;
    int32              eefs_fd = (int32 )iop->file_info;
    int32              eefs_status;


    #ifdef EEFS_DEBUG
       printf("eefs_read\n");
    #endif

    sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
    {
        rtems_set_errno_and_return_minus_one(EIO);
    }
	
    /*
    ** Read the file using the EEFS API
    */ 
    eefs_status = EEFS_LibRead(eefs_fd, buffer, count);
    if ( eefs_status < 0 )
    {
       rtems_semaphore_release(fs->eefs_mutex);
       rtems_set_errno_and_return_minus_one(EIO);
    }    
    else
    {
       /*
       ** Set the amount read
       */
       ret = eefs_status;
    }
    
    rtems_semaphore_release(fs->eefs_mutex);
    return (ret);
}

/* 
** rtems_eefs_write
**     Write data to an open EEFS file.
**
** PARAMETERS:
**     iop      - RTEMS iop structure, which keeps track of an open file.
**     buffer   - pointer to the user's buffer for the data
**     count    - number of bytes of data requested by the user to write
**
** RETURNS:
**     number of bytes of data written, 0 for EOF, or -1 with errno set for errors.
*/
static ssize_t rtems_eefs_write(
    rtems_libio_t   *iop,
    const void      *buffer,
    size_t           count
)
{
   rtems_status_code  sc = RTEMS_SUCCESSFUL;
   eefs_info_t     *fs = iop->pathinfo.mt_entry->fs_info;
   int32              eefs_fd = (int32 )iop->file_info;
   int32              eefs_status;

   #ifdef EEFS_DEBUG
      printf("eefs_write\n");
   #endif

   /*
   ** Get the file system info.
   */
   fs = eefs_info_iop (iop);

   sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
   if (sc != RTEMS_SUCCESSFUL)
   {
       rtems_set_errno_and_return_minus_one(EIO);
   }

   eefs_status = EEFS_LibWrite(eefs_fd, (void*)buffer, count); 
 
   if (eefs_status < 0)
   {
      rtems_semaphore_release(fs->eefs_mutex);
      rtems_set_errno_and_return_minus_one(EIO);
   }
   rtems_semaphore_release(fs->eefs_mutex);

   /*
   ** There should not be any reason in the EEFS why the amount of data could not be written
   **   So if it does not return the correct count, consider it an error
   */
   if ( eefs_status != count )
   {
      rtems_set_errno_and_return_minus_one(ENOSPC);
   }

   return (eefs_status);
}

/* 
** rtems_eefs_lseek
**     Seek in an open file. The EEFS only supports SEEK_SET.
**
** PARAMETERS:
**     iop      - RTEMS iop structure, which keeps track of an open file.
**     length   - the location or offset in the file
**     whence   - the seek command. Currently only SEEK_SET is supported.
**
** RETURNS:
**     RC_OK (0) on success, or -1 with errno set on an error.
*/
rtems_off64_t rtems_eefs_lseek(
  rtems_libio_t *iop,
  rtems_off64_t  length,
  int            whence
)
{
   rtems_status_code  sc = RTEMS_SUCCESSFUL;
   eefs_info_t       *fs = iop->pathinfo.mt_entry->fs_info;
   int32              eefs_fd = (int32 )iop->file_info;
   uint32             byte_offset = length & 0xFFFFFFFF;
   int32              eefs_status;

   #ifdef EEFS_DEBUG
      printf("eefs_lseek\n");
   #endif

   sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
   if (sc != RTEMS_SUCCESSFUL)
   {
       rtems_set_errno_and_return_minus_one(EIO);
   }

   if ( whence == SEEK_SET )
   {
      eefs_status = EEFS_LibLSeek(eefs_fd, byte_offset, whence );
      if ( eefs_status < 0 )
      {
         rtems_semaphore_release(fs->eefs_mutex);
         rtems_set_errno_and_return_minus_one(EIO);
      }
   }
   else
   {
      rtems_semaphore_release(fs->eefs_mutex);
      rtems_set_errno_and_return_minus_one(ENOTSUP);
   }

   rtems_semaphore_release(fs->eefs_mutex);
   return(RC_OK);
}

/* 
** rtems_eefs_fstat 
**     return information about a file in the EEFS 
**
** PARAMETERS:
**     loc - node description
**     buf - stat buffer provided by user
**
** RETURNS:
**     RC_OK on success, or -1 if error occured (errno set appropriately)
*/
int rtems_eefs_fstat(rtems_filesystem_location_info_t *loc, struct stat  *buf)
{
    rtems_status_code  sc         = RTEMS_SUCCESSFUL;
    eefs_info_t     *fs         = loc->mt_entry->fs_info;
    EEFS_FileHeader_t *HeaderPtr;
    uint32             eefsInode;

    #ifdef EEFS_DEBUG
       printf("eefs_fstat\n");
    #endif

    sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
    {
        rtems_set_errno_and_return_minus_one(EIO);
    }

    if ( (int)loc->node_access ==  ROOT_INODE )
    {
       buf->st_dev = EEFS_DEVICE; 
       buf->st_ino = ROOT_INODE;
       buf->st_mode  = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
       buf->st_rdev = 0;
       buf->st_size =  fs->eefs_inode_table.NumberOfFiles * sizeof( struct dirent );
       buf->st_blocks = fs->eefs_inode_table.NumberOfFiles; /* 1 */
       buf->st_blksize = sizeof( struct dirent ); /* 512; */
       buf->st_mtime = 0;

    }
    else if ( (int)loc->node_access_2 != EEFS_FILE_NOT_FOUND )
    {
       eefsInode = (uint32)loc->node_access_2;
       HeaderPtr =  (EEFS_FileHeader_t *)fs->eefs_inode_table.File[eefsInode].FileHeaderPointer; 	

       /*
       ** Fill out the user's stat buffer
       ** Most of these things don't apply to the EEFS
       */
       buf->st_dev = EEFS_DEVICE; 
       buf->st_ino = eefsInode + 1;
       buf->st_rdev = 0; 
       buf->st_mode  =  S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
       buf->st_size = HeaderPtr->FileSize;
       buf->st_blksize = 512;
       buf->st_blocks = buf->st_size / 512;
       buf->st_mtime = 0;
   }
   else
   {
       /*
       ** File not found
       */
       rtems_semaphore_release(fs->eefs_mutex);
       rtems_set_errno_and_return_minus_one(ENOENT);
   }

   rtems_semaphore_release(fs->eefs_mutex);

   return(RC_OK);
}

/* 
** rtems_eefs_fopen 
**     stub implementation to allow fopen to work properly 
**
** RETURNS:
**     RC_OK 
*/
static int rtems_eefs_ftruncate(
    rtems_libio_t   *iop __attribute__((unused)),
    rtems_off64_t    count __attribute__((unused))
)
{
    #ifdef EEFS_DEBUG
       printf("eefs_fopen\n");
    #endif

    return(RC_OK);
}

/* 
** rtems_eefs_node_type
**     Returns the type of EEFS node. In the EEFS there are no
**     sub-directories, so there is only one directory node ( the root ).
**     Anything else has to be a file.
**
** PARAMETERS:
**     loc - node description structure
**
** RETURNS:
**     RTEMS_FILESYSTEM_DIRECTORY or RTEMS_FILESYSTEM_MEMORY_FILE
*/
static rtems_filesystem_node_types_t rtems_eefs_node_type(
     rtems_filesystem_location_info_t        *pathloc                 /* IN */
)
{
    #ifdef EEFS_DEBUG
       printf("eefs_node_type:");
    #endif

    if ((int)pathloc->node_access == ROOT_INODE)
    {
        #ifdef EEFS_DEBUG
           printf("Directory\n");
        #endif
        return RTEMS_FILESYSTEM_DIRECTORY;
    }
    else 
    {
        #ifdef EEFS_DEBUG
           printf("File\n");
        #endif
        return RTEMS_FILESYSTEM_MEMORY_FILE;
    }
}

/* 
** rtems_eefs_node_mknod
**     Creates a new filesystem node. For the EEFS this is kind of a kludge because of the way
**     the file size is determined for a new file. Normally a new node is created in a filesystem,
**     then the file is opened for writing, rewound to the start, then written. The EEFS creates the 
**     inode when a new file is opened for writing/create so this step is not really needed. In 
**     addition to that, when you open a new file for writing in the EEFS, it just grabs space from
**     the available free space until you close the file. The files in the EEFS are 
**     usually pre-allocated, so this is the only way we can know how much space the file 
**     will need. 
**     When this function is called, it will just set up some variables that indicate that 
**     a mknod is pending. When the eval_path is called after this by RTEMS, it will check to see
**     if the file has been created, which will pass. Finally when the file is opened the 
**     rtems_eefs_open function will see the "pending" status and complete the file creation/open
**     step. 
**     This would probably not be necessary if the 
**
** PARAMETERS:
**     loc - node description structure
**
** RETURNS:
**     RTEMS_FILESYSTEM_DIRECTORY or RTEMS_FILESYSTEM_MEMORY_FILE
*/
int rtems_eefs_mknod (const char                       *name,
                       mode_t                            mode,
                       dev_t                             dev,
                       rtems_filesystem_location_info_t *pathloc)
{
    rtems_status_code  sc;
    eefs_info_t       *fs = pathloc->mt_entry->fs_info;

    #ifdef EEFS_DEBUG
       printf("eefs_mknod\n");
    #endif

    /*
    ** See if the new mode is a directory
    ** Directories are not supported in EEFS
    */
    if ( S_ISDIR(mode) )
    {
       rtems_set_errno_and_return_minus_one(ENOTSUP);
    }

    /*
    ** Grab the semaphore
    */
    sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
    {
        rtems_set_errno_and_return_minus_one( EIO );
    }

    /*
    ** Check to see if another mknod is pending
    */
    if ( fs->mknod_pending == TRUE )
    {
       rtems_semaphore_release(fs->eefs_mutex);
       rtems_set_errno_and_return_minus_one(EIO);
    }

    /*
    ** set the mknod pending variable
    */
    fs->mknod_pending = TRUE;

    /* 
    ** Copy the name
    */
    strncpy (fs->mknod_pending_name, name, EEFS_MAX_FILENAME_SIZE);

    /*
    ** Release the semaphore
    */
    rtems_semaphore_release(fs->eefs_mutex);

    return(RC_OK);
}

/*
** Free node info
*/
static int rtems_eefs_free_node_info(
     rtems_filesystem_location_info_t        *pathloc                 /* IN */
)
{
    #ifdef EEFS_DEBUG
       printf("eefs_free_node_info\n");
    #endif

    /*
    ** no dynamic info to free
    */
    return 0;
}

/*
** rtems_eefs_rename - rename a file 
*/
int rtems_eefs_rename(
 rtems_filesystem_location_info_t  *old_parent_loc,  /* IN */
 rtems_filesystem_location_info_t  *old_loc,         /* IN */
 rtems_filesystem_location_info_t  *new_parent_loc,  /* IN */
 const char                        *name             /* IN */
)
{
   rtems_status_code    sc = RTEMS_SUCCESSFUL;
   eefs_info_t         *fs = old_loc->mt_entry->fs_info;
   EEFS_FileHeader_t    FileHeader;
   uint32               inode;

   if ((int)old_loc->node_access == ROOT_INODE)
   {
      rtems_set_errno_and_return_minus_one( ENOTSUP );
   }
   else if ((int)old_loc->node_access == FILE_INODE)
   {
      if ((int)old_loc->node_access_2 != EEFS_FILE_NOT_FOUND )
      {
         inode = (int)old_loc->node_access_2;
         if ( inode < EEFS_MAX_FILES )
         {
             sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
             if (sc != RTEMS_SUCCESSFUL)
             {
                rtems_set_errno_and_return_minus_one(EIO);
             }
             EEFS_LIB_EEPROM_READ(&FileHeader, fs->eefs_inode_table.File[inode].FileHeaderPointer,
                            sizeof(EEFS_FileHeader_t));
             if ( FileHeader.InUse == TRUE )
             {
                /*
                ** Rename
                */
                strncpy(FileHeader.Filename, name, EEFS_MAX_FILENAME_SIZE);

                EEFS_LIB_EEPROM_WRITE(fs->eefs_inode_table.File[inode].FileHeaderPointer, 
                                   &FileHeader, sizeof(EEFS_FileHeader_t));
                EEFS_LIB_EEPROM_FLUSH;
                rtems_semaphore_release(fs->eefs_mutex);
            }
            else
            {
                rtems_semaphore_release(fs->eefs_mutex);
                rtems_set_errno_and_return_minus_one(ENOENT);
            }
         }
         else
         {
            rtems_set_errno_and_return_minus_one(ENOENT);
         }
      }
      else
      {
         rtems_set_errno_and_return_minus_one(ENOENT);
      }
   }
   else
   {
      rtems_set_errno_and_return_minus_one( ENOTSUP );
   }
   return(0);
}
  
/*
** Unlink -- delete a file
*/
int rtems_eefs_unlink(
 rtems_filesystem_location_info_t      *parent_loc,   /* IN */
 rtems_filesystem_location_info_t      *pathloc       /* IN */
)
{
   rtems_status_code    sc = RTEMS_SUCCESSFUL;
   eefs_info_t       *fs = pathloc->mt_entry->fs_info;
   EEFS_FileHeader_t    FileHeader;
   uint32               inode;

   #ifdef EEFS_DEBUG
       printf("eefs_unlink\n");
   #endif

   if ((int)pathloc->node_access == ROOT_INODE)
   {
      rtems_set_errno_and_return_minus_one( ENOTSUP );
   }
   else if ((int)pathloc->node_access == FILE_INODE)
   {
      if ((int)pathloc->node_access_2 != EEFS_FILE_NOT_FOUND )
      {
         inode = (int)pathloc->node_access_2;
         if ( inode < EEFS_MAX_FILES )
         {
             sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
             if (sc != RTEMS_SUCCESSFUL)
             {
                rtems_set_errno_and_return_minus_one(EIO);
             }
             EEFS_LIB_EEPROM_READ(&FileHeader, fs->eefs_inode_table.File[inode].FileHeaderPointer,
                            sizeof(EEFS_FileHeader_t));
             if ( FileHeader.InUse == TRUE )
             {
                FileHeader.InUse = FALSE;
                EEFS_LIB_EEPROM_WRITE(fs->eefs_inode_table.File[inode].FileHeaderPointer, 
                                   &FileHeader, sizeof(EEFS_FileHeader_t));
                EEFS_LIB_EEPROM_FLUSH;
                rtems_semaphore_release(fs->eefs_mutex);
            }
            else
            {
                rtems_semaphore_release(fs->eefs_mutex);
                rtems_set_errno_and_return_minus_one(ENOENT);
            }
         }
         else
         {
            rtems_set_errno_and_return_minus_one(ENOENT);
         }
      }
      else
      {
         rtems_set_errno_and_return_minus_one(ENOENT);
      }
   }
   else
   {
      rtems_set_errno_and_return_minus_one( ENOTSUP );
   }
   return(0);
}

/**************************************************************************/
/*  Directory related functions 
**
*/

/* eefs_dir_open --
**     Open fat-file which correspondes to the directory being opened and
**     set offset field of file control block to zero.
**
** PARAMETERS:
**     iop        - file control block
**     pathname   - name
**     flag       - flags
**     mode       - mode
**
** RETURNS:
**     RC_OK, if directory opened successfully, or -1 if error occured (errno
**     set apropriately)
**
** rtems_libio_t structure has the following fields:
**     rtems_off64_t   size
**     rtems_off64_t   offset
**     uint32_t        flags
**     uint32_t        data0
**     void           *data1
*/
int rtems_eefs_dir_open(rtems_libio_t *iop, const char *pathname, uint32_t flag,  uint32_t  mode)
{
    int                  rc = RC_OK;
    rtems_status_code    sc = RTEMS_SUCCESSFUL;
    eefs_info_t       *fs = iop->pathinfo.mt_entry->fs_info;


    #ifdef EEFS_DEBUG
       printf("eefs_dir_open\n");
    #endif

    sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
    {
        rtems_set_errno_and_return_minus_one( EIO );
    }

    if ((int)iop->pathinfo.node_access == ROOT_INODE)
    {
       iop->offset = 0;
       iop->data0 = 0;  /* Directory inode index */
       rtems_semaphore_release(fs->eefs_mutex);
       rc = RC_OK;
    }
    else
    {
      rtems_semaphore_release(fs->eefs_mutex);
      rtems_set_errno_and_return_minus_one(ENOTDIR);
    }
    return(rc);
}

/* 
 * rtems_eefs_dir_close --
 *
 * PARAMETERS:
 *     iop - file control block
 *
 * RETURNS:
 *     RC_OK
 */
int rtems_eefs_dir_close(rtems_libio_t *iop)
{
    #ifdef EEFS_DEBUG
       printf("eefs_dir_close\n");
    #endif

    /*
    ** Nothing to do. No state information is saved
    */
    return(RC_OK);
}


/*  eefs_dir_read --
 *      This routine will read the next directory entry based on the directory
 *      offset. The offset should be equal to -n- time the size of an
 *      individual dirent structure. If n is not an integer multiple of the
 *      sizeof a dirent structure, an integer division will be performed to
 *      determine directory entry that will be returned in the buffer. Count
 *      should reflect -m- times the sizeof dirent bytes to be placed in the
 *      buffer.
 *      If there are not -m- dirent elements from the current directory
 *      position to the end of the exisiting file, the remaining entries will
 *      be placed in the buffer and the returned value will be equal to
 *      -m actual- times the size of a directory entry.
 *
 * PARAMETERS:
 *     iop    - file control block
 *     buffer - buffer provided by user
 *     count  - count of bytes to read
 *
 * RETURNS:
 *     the number of bytes read on success, or -1 if error occured (errno
 *     set apropriately).
 */
ssize_t rtems_eefs_dir_read(rtems_libio_t *iop, void *buffer, size_t count)
{
    rtems_status_code   sc = RTEMS_SUCCESSFUL;
    eefs_info_t      *fs = iop->pathinfo.mt_entry->fs_info;
    uint32_t            cmpltd = 0;
    uint32_t            start = 0;
    struct dirent       tmp_dirent;
    EEFS_FileHeader_t   FileHeader;

    #ifdef EEFS_DEBUG
       printf("eefs_dir_read\n");
    #endif

    /*
    ** calculate start and count - protect against using sizes that are not exact
    ** multiples of the -dirent- size. These could cause unexpected
    ** results
    */
    start = iop->offset / sizeof(struct dirent);
    count = (count / sizeof(struct dirent)) * sizeof(struct dirent);

    sc = rtems_semaphore_obtain(fs->eefs_mutex, RTEMS_WAIT,
                                EEFS_VOLUME_SEMAPHORE_TIMEOUT);
    if (sc != RTEMS_SUCCESSFUL)
    {
        rtems_set_errno_and_return_minus_one(EIO);
    }

    while (count > 0)
    {
       if ( (iop->data0) >= fs->eefs_inode_table.NumberOfFiles )
       {
          rtems_semaphore_release(fs->eefs_mutex);
          return(cmpltd); 
       }
    
       /*
       ** Read the file header from the EEFS
       */  
       EEFS_LIB_EEPROM_READ(&FileHeader, fs->eefs_inode_table.File[iop->data0].FileHeaderPointer,
                          sizeof(EEFS_FileHeader_t));
       if ( FileHeader.InUse == TRUE ) 
       {
          tmp_dirent.d_ino = iop->data0 + 1;
          tmp_dirent.d_off = count; /* sizeof(struct dirent); */
          tmp_dirent.d_reclen = sizeof(struct dirent);
          tmp_dirent.d_namlen = strlen(FileHeader.Filename);
          strcpy(tmp_dirent.d_name, FileHeader.Filename);

          /*
          ** Copy the temporary entry to the user's buffer
          */
          memcpy(buffer + cmpltd, &tmp_dirent, sizeof(struct dirent));

          /*
          ** Update counters / offset
          */
          (iop->offset) += (sizeof(struct dirent)); 
          cmpltd += (sizeof(struct dirent));
          count -= (sizeof(struct dirent));
          (iop->data0 ) ++;

          if (count <= 0)
          {
            break;
          }
       }
       else
       {
          /*
          ** Skip it.. It's a deleted entry
          */
          (iop->data0 ) ++;
       }

    } /* end while */

    rtems_semaphore_release(fs->eefs_mutex);
    return (cmpltd);
}

/* eefs_dir_lseek --
 *
 *  This routine will behave in one of three ways based on the state of
 *  argument whence. Based on the state of its value the offset argument will
 *  be interpreted using one of the following methods:
 *
 *     SEEK_SET - offset is the absolute byte offset from the start of the
 *                logical start of the dirent sequence that represents the
 *                directory
 *     SEEK_CUR - offset is used as the relative byte offset from the current
 *                directory position index held in the iop structure
 *     SEEK_END - N/A --> This will cause an assert.
 *
 * PARAMETERS:
 *     iop    - file control block
 *     offset - offset
 *     whence - predefine directive
 *
 * RETURNS:
 *     RC_OK on success, or -1 if error occured (errno
 *     set apropriately).
 */
rtems_off64_t rtems_eefs_dir_lseek(rtems_libio_t *iop, rtems_off64_t offset, int whence)
{
    #ifdef EEFS_DEBUG
       printf("eefs_dir_lseek\n");
    #endif

    switch (whence)
    {
        case SEEK_SET:
        case SEEK_CUR:
            break;
        /*
         * Movement past the end of the directory via lseek is not a
         * permitted operation
         */
        case SEEK_END:
        default:
            rtems_set_errno_and_return_minus_one( EINVAL );
            break;
    }
    return(RC_OK);
}

/* eefs_dir_chmod --
 *     Change the attributes of the directory. This currently does
 *     nothing and returns no error.
 *
 * PARAMETERS:
 *     pathloc - node description
 *     mode - the new mode
 *
 * RETURNS:
 *     RC_OK always
 */
int rtems_eefs_dir_fchmod(rtems_filesystem_location_info_t *pathloc,
                          mode_t                            mode)
{
    #ifdef EEFS_DEBUG
       printf("eefs_dir_fchmod\n");
    #endif

    return(RC_OK);
}

/*****************************************************************************************/
/*
** The RTEMS OPS tables needed for the driver
*/
const rtems_filesystem_operations_table  rtems_eefs_ops = 
{
    .evalpath_h     = rtems_eefs_eval_path,
    .evalformake_h  = rtems_eefs_evaluate_for_make,
    .link_h         = NULL, 
    .unlink_h       = rtems_eefs_unlink, 
    .node_type_h    = rtems_eefs_node_type, 
    .mknod_h        = rtems_eefs_mknod, 
    .chown_h        = NULL,
    .freenod_h      = rtems_eefs_free_node_info,
    .mount_h        = NULL, 
    .fsmount_me_h   = rtems_eefs_initialize,
    .unmount_h      = NULL, 
    .fsunmount_me_h = rtems_eefs_shutdown,
    .utime_h        = NULL, 
    .eval_link_h    = NULL,
    .symlink_h      = NULL, 
    .readlink_h     = NULL,
    .rename_h       = rtems_eefs_rename,
    .statvfs_h      = rtems_eefs_statvfs 
};

const rtems_filesystem_file_handlers_r rtems_eefs_file_handlers = 
{
   .open_h          = rtems_eefs_open,
   .close_h         = rtems_eefs_close,
   .read_h          = rtems_eefs_read,
   .write_h         = rtems_eefs_write,
   .ioctl_h         = NULL, 
   .lseek_h         = rtems_eefs_lseek, 
   .fstat_h         = rtems_eefs_fstat, 
   .fchmod_h        = NULL, 
   .ftruncate_h     = rtems_eefs_ftruncate,
   .fpathconf_h     = NULL, 
   .fsync_h         = NULL, 
   .fdatasync_h     = NULL, 
   .fcntl_h         = NULL, 
   .rmnod_h         = NULL 
};

const rtems_filesystem_file_handlers_r rtems_eefs_dir_handlers = 
{
   .open_h          = rtems_eefs_dir_open,
   .close_h         = rtems_eefs_dir_close,
   .read_h          = rtems_eefs_dir_read,
   .write_h         = NULL,
   .ioctl_h         = NULL,
   .lseek_h         = rtems_eefs_dir_lseek, 
   .fstat_h         = rtems_eefs_fstat,   /* Use the same for files and directories */
   .fchmod_h        = rtems_eefs_dir_fchmod,
   .ftruncate_h     = NULL,
   .fpathconf_h     = NULL, 
   .fsync_h         = NULL, 
   .fdatasync_h     = NULL,
   .fcntl_h         = NULL,
   .rmnod_h         = NULL 
};
