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
// read to EOF 
static unsigned char* sha256_fd( int fd ) {
   
   SHA256_CTX context;
   SHA256_Init( &context );
   unsigned char* new_checksum = RUNFS_CALLOC( unsigned char, SHA256_DIGEST_LENGTH );
   unsigned char buf[32768];
   
   ssize_t num_read = 1;
   while( num_read > 0 ) {
      
      num_read = read( fd, buf, 32768 );
      
      if( num_read < 0 ) {
         fskit_error("sha256_file: I/O error reading FD %d, errno=%d\n", fd, -errno );
         SHA256_Final( new_checksum, &context );
         free( new_checksum );
         return NULL;
      }
      
      SHA256_Update( &context, buf, num_read );
   }
   
   SHA256_Final( new_checksum, &context );
   
   return new_checksum;
}

// make a sha256 printable (null-terminated, printf-able)
char* sha256_printable( unsigned char const* sha256 ) {
   
   char* ret = RUNFS_CALLOC( char, 2 * SHA256_DIGEST_LENGTH + 1 );
   
   char buf[3];
   for( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
      sprintf(buf, "%02x", sha256[i] );
      ret[2*i] = buf[0];
      ret[2*i + 1] = buf[1];
   }
   
   return ret;
}

// get the (printable, null-terminated) sha256 of a path  
int runfs_sha256( char const* bin_path, char** ret_sha256 ) {
   
   int fd = 0;
   int rc = 0;
   unsigned char* proc_sha256 = NULL;
   char* proc_sha256_printable = NULL;
   
   // sha256 the binary...
   fd = open( bin_path, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      fskit_error("open(%s) rc = %d\n", bin_path, rc );
      return rc;
   }
   
   proc_sha256 = sha256_fd( fd );
   if( proc_sha256 == NULL ) {
      
      fskit_error("sha256_fd(%d) rc = %d\n", fd, rc );
      close( fd );
      return -EPERM;
   }
   
   close( fd );
   
   proc_sha256_printable = sha256_printable( proc_sha256 );
   if( proc_sha256_printable == NULL ) {
      
      free( proc_sha256 );
      return -ENOMEM;
   }
   
   free( proc_sha256 );
   
   *ret_sha256 = proc_sha256_printable;
   
   return rc;
}


// set up a pidfile inode 
int runfs_inode_init( struct runfs_inode* inode, pid_t pid ) {
   
   char* proc_path = NULL;
   char* proc_sha256 = NULL;
   struct stat sb;
   int rc = 0;
   
   if( inode->proc_path != NULL ) {
      return -EINVAL;
   }
   
   if( inode->proc_sha256 != NULL ) {
      return -EINVAL;
   }
   
   rc = runfs_os_get_proc_path( pid, &proc_path );
   if( rc != 0 ) {
      return rc;
   }
   
   rc = runfs_sha256( proc_path, &proc_sha256 );
   if( rc != 0 ) {
   
      free( proc_path );
      return rc;
   }
   
   rc = stat( proc_path, &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      free( proc_path );
      free( proc_sha256 );
      return rc;
   }
   
   inode->pid = pid;
   inode->proc_path = proc_path;
   inode->proc_sha256 = proc_sha256;
   inode->proc_size = sb.st_size;
   inode->proc_mtime.tv_sec = sb.st_mtim.tv_sec;
   inode->proc_mtime.tv_nsec = sb.st_mtim.tv_nsec;
   
   return 0;
}


// verify that a given process's binary has not been modified 
// return 1 if it has been modified, or does not exist
// return 0 if it has not been modified
// return negative on failure 
int runfs_inode_is_proc_modified( struct runfs_inode* inode ) {
   
   struct stat sb;
   int rc = 0;
   
   rc = stat( inode->proc_path, &sb );
   if( rc != 0 ) {
      rc = -errno;
      
      if( rc == -ENOENT ) {
         // modified--doesn't exist 
         return 1;
      }
      else {
         return rc;
      }
   }
   
   if( sb.st_mtim.tv_sec != inode->proc_mtime.tv_sec ||
       sb.st_mtim.tv_nsec != inode->proc_mtime.tv_nsec || 
       sb.st_size != inode->proc_size ) {
      
      // modified 
      return 1;
   }
   
   // not modified
   return 0;
}


// verify that a given process created the given file
// return 0 if not equal 
// return 1 if equal 
// return negative on error
int runfs_inode_is_created_by_proc( struct runfs_inode* inode, pid_t pid ) {
   
   int rc = 0;
   struct runfs_inode cmp;
   memset( &cmp, 0, sizeof(struct runfs_inode) );
   
   rc = runfs_inode_init( &cmp, pid );
   if( rc != 0 ) {
      
      runfs_inode_free( &cmp );
      return rc;
   }
   
   if( inode->pid == cmp.pid &&
       inode->proc_size == cmp.proc_size &&
       inode->proc_mtime.tv_sec == cmp.proc_mtime.tv_sec &&
       inode->proc_mtime.tv_nsec == cmp.proc_mtime.tv_nsec &&
       strcmp(inode->proc_path, cmp.proc_path) == 0 &&
       strcmp(inode->proc_sha256, cmp.proc_sha256) == 0 ) {
      
      rc = 1;
   }
   else {
      rc = 0;
   }
   
   runfs_inode_free( &cmp );
   
   return rc;
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
   if( rc < 0 ) {
      return rc;
   }
   
   if( rc == 0 ) {
      return 0;
   }
   
   if( inode->check_hash_always ) {
      
      // check everything
      rc = runfs_inode_is_created_by_proc( inode, pid );
      if( rc < 0 ) {
         return rc;
      }
      
      if( rc == 0 ) {
         return 0;
      }
   }
   else {
      
      // check only the modtime
      rc = runfs_inode_is_proc_modified( inode );
      if( rc < 0 ) {
         return rc;
      }
      
      if( rc > 0 ) {
         // modtime and/or size changed
         return 0;
      }
   }
   
   return 1;
}

// free a pid inode
int runfs_inode_free( struct runfs_inode* inode ) {
   
   if( inode->proc_path != NULL ) {
      
      free( inode->proc_path );
      inode->proc_path = NULL;
   }
   
   if( inode->proc_sha256 != NULL ) {
      
      free( inode->proc_sha256 );
      inode->proc_sha256 = NULL;
   }
   
   if( inode->contents != NULL ) {
      
      free( inode->contents );
      inode->contents = NULL;
   }
   
   memset( inode, 0, sizeof(struct runfs_inode) );
   return 0;
}

