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
   int fd = 0;
   ssize_t nr = 0;
   static size_t deleted_len = strlen( " (deleted)" );
   
   memset( proc_path, 0, PATH_MAX+1 );
   memset( bin_path, 0, PATH_MAX+1 );
   
   sprintf( proc_path, "/proc/%d/exe", pid );
   
   // open the process binary, if we can 
   fd = open( proc_path, O_RDONLY );
   if( fd < 0 ) {
      
      rc = -errno;
      fskit_error("open(%s) rc = %d\n", proc_path, rc );
      return rc;
   }
   
   // verify that it isn't deleted
   nr = readlink( proc_path, bin_path, PATH_MAX );
   if( nr < 0 ) {
   
      rc = -errno;
      close( fd );
      
      fskit_error("readlink(%s) rc = %d\n", proc_path, rc );
      
      return rc;
   }
   
   // on Linux, if the bin_path ends in " (deleted)", we're guaranteed that this binary no longer exists.
   if( strlen( bin_path ) > strlen( " (deleted)") && strcmp( bin_path + strlen(bin_path) - deleted_len, " (deleted)" ) == 0 ) {
      
      rc = -ENOENT;
      close( fd );
      
      fskit_error("%s\n", proc_path);
      return rc;
   }
   
   rc = fstat( fd, &sb );
   
   if( rc < 0 ) {
      
      rc = -errno;
      close( fd );
      
      fskit_error("stat(%s) rc = %d\n", proc_path, rc );
      return rc;
   }
   
   close( fd );
   
   if( !S_ISREG( sb.st_mode ) ) {
      
      // not a process binary 
      fskit_error("%s is not a regular file\n", bin_path );
      return -EPERM;
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

#endif