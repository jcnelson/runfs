/*
   runfs: a self-cleaning filesystem for runtime state.
   Copyright (C) 2015  Jude Nelson

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
// return 0 on success
// return -ENOMEM on OOM 
// return negative on failure to stat the process identified
int runfs_inode_init( struct runfs_inode* inode, pid_t pid, int verify_discipline ) {
   
   int rc = 0;
   int flags = 0;
   
   inode->ps = pstat_new();
   if( inode->ps == NULL ) {
      return -ENOMEM;
   }
   
   rc = pstat( pid, inode->ps, flags );
   if( rc != 0 ) {
       
      runfs_safe_free( inode->ps );
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
   
   struct stat sb;
   struct stat inode_sb;
   char bin_path[PATH_MAX+1];
   char inode_path[PATH_MAX+1];
   
   pstat_get_stat( proc_stat, &sb );
   pstat_get_stat( inode->ps, &inode_sb );
   
   pstat_get_path( proc_stat, bin_path );
   pstat_get_path( inode->ps, inode_path );
   
   if( !pstat_is_running( proc_stat ) ) {
   
      runfs_debug("PID %d is not running\n", pstat_get_pid( proc_stat ) );
      return 0;
   }
   
   if( pstat_get_pid( proc_stat ) != pstat_get_pid( inode->ps ) ) {
      
      runfs_debug("PID mismatch: %d != %d\n", pstat_get_pid( inode->ps ), pstat_get_pid( proc_stat ) );
      return 0;
   }
   
   if( verify_discipline & RUNFS_VERIFY_INODE ) {
      
      if( pstat_is_deleted( proc_stat ) || inode_sb.st_ino != sb.st_ino ) {
         
         runfs_debug("%d: Inode mismatch: %ld != %ld\n", pstat_get_pid( inode->ps ), inode_sb.st_ino, sb.st_ino );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_SIZE ) {
      if( pstat_is_deleted( proc_stat ) || inode_sb.st_size != sb.st_size ) {
         
         runfs_debug("%d: Size mismatch: %jd != %jd\n", pstat_get_pid( inode->ps ), inode_sb.st_size, sb.st_size );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_MTIME ) {
      if( pstat_is_deleted( proc_stat )|| inode_sb.st_mtim.tv_sec != sb.st_mtim.tv_sec || inode_sb.st_mtim.tv_nsec != sb.st_mtim.tv_nsec ) {
         
         runfs_debug("%d: Modtime mismatch: %ld.%ld != %ld.%ld\n", pstat_get_pid( inode->ps ), inode_sb.st_mtim.tv_sec, inode_sb.st_mtim.tv_nsec, sb.st_mtim.tv_sec, sb.st_mtim.tv_nsec );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_PATH ) {
       
      if( pstat_is_deleted( proc_stat ) || strcmp(bin_path, inode_path) != 0 ) {
         
         runfs_debug("%d: Path mismatch: %s != %s\n", pstat_get_pid( inode->ps ), inode_path, bin_path );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_STARTTIME ) {
      
      if( pstat_get_starttime( proc_stat ) != pstat_get_starttime( inode->ps ) ) {
          
         runfs_debug("%d: Start time mismatch: %" PRIu64 " != %" PRIu64 "\n", pstat_get_pid( inode->ps ), pstat_get_starttime( proc_stat ), pstat_get_starttime( inode->ps ) );
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
int runfs_inode_is_valid( struct runfs_inode* inode ) {
   
   int rc = 0;
   struct pstat* ps = pstat_new();
   if( ps == NULL ) {
      return -ENOMEM;
   }
   
   pid_t pid = pstat_get_pid( inode->ps );
   
   rc = pstat( pid, ps, 0 );
   if( rc < 0 ) {
       
      runfs_safe_free( ps );
      runfs_error("pstat(%d) rc = %d\n", pid, rc );
      return rc;
   }
   
   rc = runfs_inode_is_created_by_proc( inode, ps, inode->verify_discipline );
   runfs_safe_free( ps );
   
   if( rc < 0 ) {
       
      runfs_error("runfs_inode_is_created_by_proc(%d) rc = %d\n", pid, rc );
      return rc;
   }
   
   return rc;
}

// free a pid inode
int runfs_inode_free( struct runfs_inode* inode ) {
   
   if( inode->contents != NULL ) {
      
      runfs_safe_free( inode->contents );
   }
   
   if( inode->ps != NULL ) {
      
      runfs_safe_free( inode->ps );
   }
   
   memset( inode, 0, sizeof(struct runfs_inode) );
   return 0;
}

