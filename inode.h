/*
   runfs: a self-cleaning filesystem for runtime state.
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU General
   Public License, but WITHOUT ANY WARRANTY; without even the implied 
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   Alternatively, you are free to use this program under the terms of the 
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or 
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#ifndef _RUNFS_INODE_H_
#define _RUNFS_INODE_H_

#include "util.h"
#include "fskit/fskit.h"

#include "pstat/libpstat.h"

#define RUNFS_PIDFILE_BUF_LEN   50

#define RUNFS_VERIFY_INODE      0x1
#define RUNFS_VERIFY_MTIME      0x2
#define RUNFS_VERIFY_SIZE       0x8
#define RUNFS_VERIFY_PATH       0x10

#define RUNFS_VERIFY_ALL        0x1F

#define RUNFS_VERIFY_DEFAULT    (RUNFS_VERIFY_INODE | RUNFS_VERIFY_MTIME | RUNFS_VERIFY_SIZE)

// information for an inode
struct runfs_inode {
   
   struct pstat ps;                                     // process owner status
   
   char* contents;                                      // contents of the file
   off_t size;                                          // size of the file
   size_t contents_len;                                 // size of the contents buffer
   
   bool deleted;                                        // if true, then consider the associated fskit entry deleted
   int verify_discipline;                               // bit flags of RUNFS_VERIFY_* that control how strict we are in verifying the accessing process
};

extern "C" {

int runfs_inode_init( struct runfs_inode* inode, pid_t pid, int verify_discipline );
int runfs_inode_free( struct runfs_inode* inode );
int runfs_inode_is_valid( struct runfs_inode* inode, pid_t pid );

}

#endif 