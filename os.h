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

#ifndef _RUNFS_OS_H_
#define _RUNFS_OS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <signal.h>
#include <libgen.h>
#include <regex.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <semaphore.h>
#include <pthread.h>

#include <utime.h>

#include <iostream>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <new>

using namespace std;


extern "C" {
   
int runfs_os_get_proc_path( pid_t pid, char** ret_proc_path );
int runfs_os_is_proc_running( pid_t );

}

#endif 