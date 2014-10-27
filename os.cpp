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

#include "os.h"
#include "fskit/fskit.h"
#include "util.h"

#ifdef _BUILD_LINUX
// Linux-specific implementation 

#include <sys/syscall.h>

pid_t gettid() {
   return syscall(__NR_gettid);
}

// get the path to this process's binary
int runfs_os_get_proc_path( pid_t pid, char** ret_proc_path ) {
   
   int rc = 0;
   
   char proc_path[PATH_MAX+1];
   char bin_path[PATH_MAX+1];
   struct stat sb;
   
   memset( proc_path, 0, PATH_MAX+1 );
   memset( bin_path, 0, PATH_MAX+1 );
   
   sprintf( proc_path, "/proc/%d/exe", pid );
   
   while( true ) {

      // resolve the symlink to the process binary 
      ssize_t nr = readlink( proc_path, bin_path, PATH_MAX );
      if( nr < 0 ) {
      
         rc = -errno;
         fskit_error("readlink(%s) rc = %d\n", proc_path, rc );
         return rc;
      }
      
      rc = stat( bin_path, &sb );
      if( rc < 0 ) {
         
         rc = -errno;
         fskit_error("stat(%s) rc = %d\n", proc_path, rc );
         return rc;
      }
      
      if( S_ISREG( sb.st_mode ) ) {
         break;
      }
      
      else if( S_ISLNK( sb.st_mode ) ) {
         
         // try again 
         memcpy( proc_path, bin_path, PATH_MAX+1 );
         memset( bin_path, 0, PATH_MAX+1 );
         continue;
      }
      else {
         
         // not a process binary 
         fskit_error("%s is not an executable\n", bin_path );
         return -EPERM;
      }
   }
   
   *ret_proc_path = strdup( proc_path );
   return 0;
}

// is a process running?
int runfs_os_is_proc_running( pid_t pid ) {
   
   int rc = 0;
   
   struct stat sb;
   char proc_path[PATH_MAX+1];
   memset( proc_path, 0, PATH_MAX+1 );
   
   sprintf( proc_path, "/proc/%d", pid );
   
   rc = stat( proc_path, &sb );
   if( rc != 0 ) {
      rc = -errno;
   }
   
   if( rc == 0 ) {
      rc = 1;
   }
   else if( rc == -ENOENT ) {
      rc = 0;
   }
   
   return rc;
}

// get the identifier of the schedulable entity that issued a request to FUSE.
// On Linux, this is the task ID.
pid_t runfs_os_get_sched_id() {
   return gettid();
}

/*
// get the actual PID, given the FUSE-given TID 
// on Linux, this is the "PPid:" field in /proc/$tid/status
// return 0 on success and set *ret_pid 
// return negative on error
int runfs_os_fuse_tid_to_pid( pid_t tid, pid_t* ret_pid ) {
   
   int rc = 0;
   int fd = 0;
   pid_t ppid = 0;
   ssize_t nr = 0;
   size_t num_read = 0;
   size_t buflen = 4 * NAME_MAX + 4;  // big enough to hold the prefix of /proc/tid/stat
   char* buf = NULL;
   char* tok = NULL;
   char* buftmp = NULL;
   char* toktmp = NULL;
   char* tmp = NULL;
   
   char proc_path[PATH_MAX+1];
   memset( proc_path, 0, PATH_MAX+1 );
   
   sprintf( proc_path, "/proc/%d/stat", tid );
   
   buf = RUNFS_CALLOC( char, buflen );
   if( buf == NULL ) {
      return -ENOMEM;
   }
   
   fd = open( proc_path, O_RDONLY );
   if( fd < 0 ) {
      rc = -errno;
      return rc;
   }
   
   while( num_read < buflen ) {
      
      nr = read( fd, buf + nr, buflen - nr );
      if( nr < 0 ) {
         rc = -errno;
         
         if( rc == -EINTR ) {
            // only interrupted. try again 
            continue;
         }
         
         break;
      }
      if( nr == 0 ) {
         break;
      }
      
      num_read += nr;
   }
   
   close( fd );
   
   // find tgid--it's the fourth token (see proc(5))
   buftmp = buf;
   for( int i = 0; i < 4; i++ ) {
      tok = strtok_r( buftmp, " \t", &toktmp );
      buftmp = NULL;
   }
   
   ppid = (pid_t)strtol( tok, &tmp, 10 );
   if( tmp == NULL ) {
      return -EIO;
   }
   
   free( buf );
   *ret_pid = ppid;
   
   return rc;
}
*/

#endif