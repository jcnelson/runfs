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

#include "state.h"


// setup a runfs deletion 
int runfs_deletion_init( struct runfs_deletion* rd, char const* path, uint64_t inode_num, bool dir ) {
   
   memset( rd, 0, sizeof(struct runfs_deletion) );
   
   rd->inode_num = inode_num;
   rd->path = strdup( path );
   rd->dir = dir;
   
   return 0;
}

// free a runfs deletion 
int runfs_deletion_free( struct runfs_deletion* rd ) {
   
   if( rd->path != NULL ) {
      
      free( rd->path );
      rd->path = NULL;
   }
   
   memset( rd, 0, sizeof(struct runfs_deletion) );
   
   return 0;
}


// remove a directory tree and all of its children.
static int runfs_state_delete_tree( struct runfs_state* state, char const* root_dir ) {
   
   int rc = 0;
   struct fskit_entry* dir = NULL;
   
   dir = fskit_entry_resolve_path( state->core, root_dir, 0, 0, true, &rc );
   if( dir == NULL ) {
      
      if( rc != -ENOENT ) {
         fskit_error("fskit_entry_resolve_path(%s) rc = %d\n", root_dir, rc );
         return rc;
      }
      else {
         return 0;
      }
   }
   
   rc = fskit_detach_all( state->core, root_dir, dir->children );
   
   if( rc != 0 ) {
      fskit_error("fskit_entry_delete_all(%s) rc = %d\n", root_dir, rc );
      
      fskit_entry_unlock( dir );
      return rc;
   }
   
   fskit_entry_unlock( dir );
   
   rc = fskit_rmdir( state->core, root_dir, 0, 0 );
   
   if( rc != 0 ) {
      
      if( rc != -ENOENT ) {
         fskit_error("fskit_rmdir(%s) rc = %d\n", root_dir, rc );
      }
      else {
         rc = 0;
      }
   }
   
   return rc;
}

// main loop for reaping now-defunct inodes 
// NOTE: this method works by calling unlinkat().  This is so any processes waiting for activity from the filesystem 
// (i.e. kqueue, inotify, fam, etc) will get properly woken up.
static void* runfs_state_deletion_thread( void* arg ) {
   
   struct runfs_state* state = (struct runfs_state*)arg;
   
   struct runfs_deletion rd;
   runfs_deletion_set* to_delete = NULL;
   int rc = 0;
   
   while( state->running ) {
      
      // wait for work 
      sem_wait( &state->delete_sem );
      
      if( !state->running ) {
         break;
      }
      
      // extract the entries to delete 
      
      pthread_rwlock_wrlock( &state->lock );
      
      to_delete = state->to_delete;
      
      // swap pending deletion requests 
      if( state->to_delete == state->to_delete_1 ) {
         state->to_delete = state->to_delete_2;
      }
      else {
         state->to_delete = state->to_delete_1;
      }
      
      state->have_delete = false;
      
      pthread_rwlock_unlock( &state->lock );
      
      // delete all 
      for( runfs_deletion_set::iterator itr = to_delete->begin(); itr != to_delete->end(); itr++ ) {
         
         rd = *itr;
         
         
         char const* method = NULL;
         
         if( rd.dir ) {
            method = "runfs_state_delete_tree";
            rc = runfs_state_delete_tree( state, rd.path );
         }
         else {
            method = "fskit_unlink";
            rc = fskit_unlink( state->core, rd.path, 0, 0 );
         }
         
         if( rc != 0 ) {
            fskit_error("%s(%s) rc = %d\n", method, rd.path, rc );
            continue;
         }
         
         runfs_deletion_free( &rd );
         
         // cancellation...
         if( !state->running ) {
            break;
         }
      }
      
      to_delete->clear();
      to_delete = NULL;
   }
   
   return NULL;
}


// schedule an entry to be deleted 
int runfs_state_sched_delete( struct runfs_state* state, char const* path, uint64_t inode_num, bool isdir ) {
   
   int rc = 0;
   struct runfs_deletion rd;
   
   runfs_deletion_init( &rd, path, inode_num, isdir );
   
   pthread_rwlock_wrlock( &state->lock );
   
   if( state->to_delete->count( rd ) == 0 ) {
      state->to_delete->insert( rd );
         
      if( !state->have_delete ) {
         // wake up the deleter 
         sem_post( &state->delete_sem );
      }
      
      state->have_delete = true;
   }
   else {
      rc = -EEXIST;
   }
   
   pthread_rwlock_unlock( &state->lock );
   
   if( rc != 0 ) {
      runfs_deletion_free( &rd );
      rc = 0;
   }
   
   return rc;
}


// set up the runfs state 
int runfs_state_init( struct runfs_state* state, struct fskit_core* core ) {
   
   int rc = 0;
   pthread_attr_t attrs;
   
   memset( state, 0, sizeof(struct runfs_state) );
   pthread_attr_init( &attrs );
   
   state->core = core;
   
   state->to_delete_1 = new runfs_deletion_set();
   state->to_delete_2 = new runfs_deletion_set();
   state->to_delete = state->to_delete_1;
   
   pthread_rwlock_init( &state->lock, NULL );
   sem_init( &state->delete_sem, 0, 0 );
   
   // start reaping 
   state->running = true;
   rc = pthread_create( &state->delete_thread, &attrs, runfs_state_deletion_thread, state );
   if( rc != 0 ) {
      rc = -errno;
      return rc;
   }
   
   return 0;
}

int runfs_state_shutdown( struct runfs_state* state ) {
   
   if( !state->running ) {
      return -EINVAL;
   }
   
   state->running = false;
   
   // wake up the thread and join with it
   sem_post( &state->delete_sem );
   pthread_join( state->delete_thread, NULL );
   
   if( state->to_delete_1 != NULL ) {
      
      delete state->to_delete_1;
      state->to_delete_1 = NULL;
   }
   
   if( state->to_delete_2 != NULL ) {
      
      delete state->to_delete_2;
      state->to_delete_2 = NULL;
   }
   
   pthread_rwlock_destroy( &state->lock );
   sem_destroy( &state->delete_sem );
   
   memset( state, 0, sizeof(struct runfs_state) );
   
   return 0;
}
