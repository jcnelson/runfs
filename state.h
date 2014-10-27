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

#ifndef _RUNFS_STATE_H_
#define _RUNFS_STATE_H_

#include "util.h"
#include "fskit/fskit.h"

// entry to delete (i.e. the process that created it died)
struct runfs_deletion {
   char* path;          // path to the entry to delete  
   uint64_t inode_num;  // inode number of the entry to delete
   bool dir;            // directory or not?
};

// compare two deletion records
struct runfs_deletion_comp {
   bool operator()( const struct runfs_deletion& rd1, const struct runfs_deletion& rd2 ) const {
      return rd1.inode_num < rd2.inode_num;
   }
};

// set of deletion records, ordered on inode 
typedef set< struct runfs_deletion, runfs_deletion_comp > runfs_deletion_set;

// global filesystem state 
struct runfs_state {
   
   runfs_deletion_set* to_delete;       // entries to be reaped
   
   runfs_deletion_set* to_delete_1;     // buffers to hold pending requests (to be swapped)
   runfs_deletion_set* to_delete_2;
   
   pthread_t delete_thread;     // thread that deletes inodes from to_delete
   sem_t delete_sem;            // signal the deletion thread when there's more work 
   bool have_delete;
   
   bool running;
   
   pthread_rwlock_t lock;       // lock giverning access to this structure
   
   struct fskit_core* core;     // fskit state
};

extern "C" {

int runfs_state_init( struct runfs_state* state, struct fskit_core* core );
int runfs_state_shutdown( struct runfs_state* state );

int runfs_state_sched_delete( struct runfs_state* state, char const* path, uint64_t inode_num, bool isdir );

}

#endif