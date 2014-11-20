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

#include "inode.h"

// get the sha256 of the data referred to by file descriptor.
// new_checksum must be at least SHA256_DIGEST_LENGTH bytes long
// read to EOF 
static int sha256_fd( int fd, unsigned char* new_checksum ) {
   
   SHA256_CTX context;
   unsigned char buf[32768];
   ssize_t num_read = 1;
   int rc = 0;
   
   SHA256_Init( &context );
   
   while( num_read > 0 ) {
      
      num_read = read( fd, buf, 32768 );
      
      if( num_read < 0 ) {
         
         rc = -errno;
         
         fskit_error("sha256_file: I/O error reading FD %d, errno=%d\n", fd, rc );
         
         SHA256_Final( new_checksum, &context );
         
         return rc;
      }
      
      SHA256_Update( &context, buf, num_read );
   }
   
   SHA256_Final( new_checksum, &context );
   
   return 0;
}


// get the (printable, null-terminated) sha256 of a path  
// ret_sha256 must be at least SHA256_DIGEST_LENGTH bytes long
int runfs_sha256( char const* bin_path, unsigned char* ret_sha256 ) {
   
   int fd = 0;
   int rc = 0;
   
   // sha256 the binary...
   fd = open( bin_path, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      fskit_error("open(%s) rc = %d\n", bin_path, rc );
      return rc;
   }
   
   rc = sha256_fd( fd, ret_sha256 );
   if( rc != 0 ) {
      
      fskit_error("sha256_fd(%d) rc = %d\n", fd, rc );
      close( fd );
      
      return -EPERM;
   }
   
   close( fd );
   
   return rc;
}


// stat a process by pid.
int runfs_stat_proc_by_pid( pid_t pid, char** ret_proc_path, struct stat* sb ) {
   
   char* proc_path = NULL;
   int rc = 0;
   
   rc = runfs_os_get_proc_path( pid, &proc_path );
   if( rc != 0 ) {
      return rc;
   }
   
   rc = stat( proc_path, sb );
   if( rc != 0 ) {
      
      rc = -errno;
      free( proc_path );
      return rc;
   }
   
   *ret_proc_path = proc_path;
   return 0;
}



// set up a pidfile inode 
int runfs_inode_init( struct runfs_inode* inode, pid_t pid, int verify_discipline ) {
   
   char* proc_path = NULL;
   struct stat sb;
   int rc = 0;
   
   if( inode->proc_path != NULL ) {
      return -EINVAL;
   }
   
   rc = runfs_stat_proc_by_pid( pid, &proc_path, &sb );
   if( rc != 0 ) {
      return rc;
   }
   
   if( verify_discipline & RUNFS_VERIFY_HASH ) {
      // only compute this if we have to (since it's slow)
      rc = runfs_sha256( proc_path, inode->proc_sha256 );
      if( rc != 0 ) {
      
         free( proc_path );
         return rc;
      }
   }
   
   inode->verify_discipline = verify_discipline;
   inode->pid = pid;
   inode->proc_path = proc_path;
   inode->proc_size = sb.st_size;
   inode->proc_mtime.tv_sec = sb.st_mtim.tv_sec;
   inode->proc_mtime.tv_nsec = sb.st_mtim.tv_nsec;
   inode->proc_inode = sb.st_ino;
   
   return 0;
}


// verify that a given process created the given file
// return 0 if not equal 
// return 1 if equal 
// return negative on error
int runfs_inode_is_created_by_proc( struct runfs_inode* inode, pid_t pid, int verify_discipline ) {
   
   int rc = 0;
   char* proc_path = NULL;
   unsigned char proc_sha256[SHA256_DIGEST_LENGTH];
   struct stat sb;
   
   rc = runfs_stat_proc_by_pid( pid, &proc_path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      return rc;
   }
   
   if( verify_discipline & RUNFS_VERIFY_INODE ) {
      if( inode->proc_inode != sb.st_ino ) {
         
         free( proc_path );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_SIZE ) {
      if( inode->proc_size != sb.st_size ) {
         
         free( proc_path );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_MTIME ) {
      if( inode->proc_mtime.tv_sec != sb.st_mtim.tv_sec || inode->proc_mtime.tv_nsec != sb.st_mtim.tv_nsec ) {
         
         free( proc_path );
         return 0;
      }
   }
   
   if( verify_discipline & RUNFS_VERIFY_HASH ) {
      
      // first get the hash...
      rc = runfs_sha256( proc_path, proc_sha256 );
      if( rc != 0 ) {
      
         free( proc_path );
         return rc;
      }
      
      // then compare it 
      if( memcmp( proc_sha256, inode->proc_sha256, SHA256_DIGEST_LENGTH ) != 0 ) {
         
         free( proc_path );
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
   
   rc = runfs_os_is_proc_running( pid );
   if( rc <= 0 ) {
      return rc;
   }
   
   rc = runfs_inode_is_created_by_proc( inode, pid, inode->verify_discipline );
   if( rc <= 0 ) {
      return rc;
   }
   
   return 1;
}

// free a pid inode
int runfs_inode_free( struct runfs_inode* inode ) {
   
   if( inode->proc_path != NULL ) {
      
      free( inode->proc_path );
      inode->proc_path = NULL;
   }
   
   if( inode->contents != NULL ) {
      
      free( inode->contents );
      inode->contents = NULL;
   }
   
   memset( inode, 0, sizeof(struct runfs_inode) );
   return 0;
}

