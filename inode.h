/*
   runfs: a self-cleaning filesystem for runtime state
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _RUNFS_INODE_H_
#define _RUNFS_INODE_H_

#include "util.h"
#include "fskit/fskit.h"
#include <openssl/sha.h>

#define RUNFS_PIDFILE_BUF_LEN   50

// information for an inode
struct runfs_inode {
   pid_t pid;                   // the pid of the process
   char* proc_path;             // path to the process.  set to NULL if not initialized   
   
   struct timespec proc_mtime;  // modtime of the process.
   char* proc_sha256;           // sha256 of the process image.  set to NULL if not initialized
   off_t proc_size;             // size of the process binary
   
   char* contents;              // contents of the file
   off_t size;                  // size of the file
   size_t contents_len;         // size of the contents buffer
   
   bool deleted;                // if true, then consider the associated fskit entry deleted
   bool check_hash_always;      // if true, always check the hash of the creator process when verifying that it's still valid
};

extern "C" {

int runfs_inode_init( struct runfs_inode* inode, pid_t pid );
int runfs_inode_free( struct runfs_inode* inode );
int runfs_inode_is_valid( struct runfs_inode* inode, pid_t pid );

}

#endif 