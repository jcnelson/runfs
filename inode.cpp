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
#include "inode.h"

// set up a pidfile inode 
int runfs_inode_init( struct runfs_inode* inode, pid_t pid, int verify_discipline ) {
   
   int rc = 0;
   int flags = 0;
   
   rc = pstat( pid, &inode->ps, flags );
   if( rc != 0 ) {
      return rc;
   }
   
   inode->verify_discipline = verify_discipline;
   
   return 0;
}


// verify that a given process created the given file
// return 0 if not equal 
// return 1 if equal 
// return negative on error
int runfs_inode_is_created_by_proc( struct runfs_inode* inode, struct pstat* proc_stat, int verify_discipline ) {
   
   if( !proc_stat->running ) {
   
      fskit_debug("PID %d is not running\n", proc_stat->pid );
      return 0;
   }
   
   if( proc_stat->pid != inode->ps.pid ) {
      
      fskit_debug("PID mismatch: %d != %d\n", inode->ps.pid, proc_stat->pid );
      return 0;
   }
   
   if( verify_discipline & RUNFS_VERIFY_INODE ) {
      if( proc_stat->deleted || inode->ps.bin_stat.st_ino != proc_stat->bin_stat.st_ino ) {
         
         fskit_debug("%d: Inode mismatch: %ld != %ld\n", inode->ps.pid, inode->ps.bin_stat.st_ino, proc_stat->bin_stat.st_ino );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_SIZE ) {
      if( proc_stat->deleted || inode->ps.bin_stat.st_size != proc_stat->bin_stat.st_size ) {
         
         fskit_debug("%d: Size mismatch: %jd != %jd\n", inode->ps.pid, inode->ps.bin_stat.st_size, proc_stat->bin_stat.st_size );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_MTIME ) {
      if( proc_stat->deleted || inode->ps.bin_stat.st_mtim.tv_sec != proc_stat->bin_stat.st_mtim.tv_sec || inode->ps.bin_stat.st_mtim.tv_nsec != proc_stat->bin_stat.st_mtim.tv_nsec ) {
         
         fskit_debug("%d: Modtime mismatch: %ld.%d != %ld.%d\n", inode->ps.pid, inode->ps.bin_stat.st_mtim.tv_sec, inode->ps.bin_stat.st_mtim.tv_nsec, proc_stat->bin_stat.st_mtim.tv_sec, proc_stat->bin_stat.st_mtim.tv_nsec );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_PATH ) {
      if( proc_stat->deleted || strcmp(proc_stat->path, inode->ps.path) != 0 ) {
         
         fskit_debug("%d: Path mismatch: %s != %s\n", inode->ps.pid, inode->ps.path, proc_stat->path );
         return 0;
      }
   }
      
   return 1;
}


// verify that an inode is still valid.
// that is, there's a process with the given PID running, and it's an instance of the same program that created it.
// to speed this up, only check the hash of the process binary if the modtime has changed
// return 1 if valid 
// return 0 if not valid 
// return negative on error
int runfs_inode_is_valid( struct runfs_inode* inode, pid_t pid ) {
   
   int rc = 0;
   struct pstat ps;
   
   memset( &ps, 0, sizeof(struct pstat) );
   
   rc = pstat( pid, &ps, 0 );
   if( rc < 0 ) {
      fskit_error("pstat(%d) rc = %d\n", pid, rc );
      return rc;
   }
   
   rc = runfs_inode_is_created_by_proc( inode, &ps, inode->verify_discipline );
   if( rc < 0 ) {
      fskit_error("runfs_inode_is_created_by_proc(%d) rc = %d\n", pid, rc );
      return rc;
   }
   
   return rc;
}

// free a pid inode
int runfs_inode_free( struct runfs_inode* inode ) {
   
   if( inode->contents != NULL ) {
      
      free( inode->contents );
      inode->contents = NULL;
   }
   
   memset( inode, 0, sizeof(struct runfs_inode) );
   return 0;
}

