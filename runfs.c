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

#include "runfs.h"

// allocate a runfs inode structure.
// return 0 on success, and set *inode_data 
// return -ENOMEM on OOM
// return negative on failure to initialize the inode
static int runfs_make_inode( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, void** inode_data ) {
   
   int rc = 0;
   pid_t calling_tid = fskit_fuse_get_pid();
   struct runfs_inode* inode = RUNFS_CALLOC( struct runfs_inode, 1 );
   
   if( inode == NULL ) {
      return -ENOMEM;
   }
   
   rc = runfs_inode_init( inode, calling_tid, RUNFS_VERIFY_DEFAULT );
   if( rc != 0 ) {
      // phantom process?
      free( inode );
      return rc;
   }
   
   *inode_data = (void*)inode;
   
   return rc;
}

// create a runfs file 
// return 0 on success
// return -ENOMEM on OOM 
// return negative on failure to initialize the inode
int runfs_create( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data ) {
   
   runfs_debug("runfs_create(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, route_metadata, fent, mode, inode_data );
}

// create sockets, FIFOs, device files, etc.
// return 0 on success, and set *inode_data
// return -ENOMEM on OOM 
// return negative on failure to initialize the inode
int runfs_mknod( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   
   runfs_debug("runfs_mknod(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, route_metadata, fent, mode, inode_data );
}

// create a directory 
// return 0 on success, and set *inode_data
// return -ENOMEM on OOM 
// return negative on failure to initialize the inode
int runfs_mkdir( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* dent, mode_t mode, void** inode_data ) {
   
   runfs_debug("runfs_mkdir(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, route_metadata, dent, mode, inode_data );
}

// read a file 
// return the number of bytes read on success
// return 0 on EOF 
// return -ENOSYS if the inode is not initialize (should *never* happen)
int runfs_read( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   runfs_debug("runfs_read(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   struct runfs_inode* inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
   int num_read = 0;
   
   if( inode == NULL ) {
      return -ENOSYS;
   }
   
   if( offset >= inode->size ) {
      return 0;
   }
   
   // copy data out, if we have any 
   if( inode->contents != NULL ) {
      
      num_read = buflen;
      
      if( (unsigned)(offset + buflen) >= inode->size ) {
         
         num_read = inode->size - offset;
         
         if( num_read < 0 ) {
            num_read = 0;
         }
      }
   }
   
   if( num_read > 0 ) {
      
      memcpy( buf, inode->contents + offset, num_read );
   }
   
   return num_read;
}

// write to a file 
// return the number of bytes written, and expand the file in RAM if we write off the edge.
// return -ENOSYS if for some reason we don't have an inode (should *never* happen)
// return -ENOMEM on OOM
int runfs_write( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   runfs_debug("runfs_write(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   struct runfs_inode* inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
   size_t new_contents_len = inode->contents_len;
   
   if( inode == NULL ) {
      return -ENOSYS;
   }
   
   if( new_contents_len == 0 ) {
      new_contents_len = 1;
   }
   
   // expand contents?
   while( offset + buflen > new_contents_len ) {
      new_contents_len *= 2;
   }
   
   if( new_contents_len > inode->contents_len ) {
      
      // expand
      char* tmp = (char*)realloc( inode->contents, new_contents_len );
      
      if( tmp == NULL ) {
         return -ENOMEM;
      }
      
      inode->contents = tmp;
      
      memset( inode->contents + inode->contents_len, 0, new_contents_len - inode->contents_len );
      
      inode->contents_len = new_contents_len;
   }
   
   // write in 
   memcpy( inode->contents + offset, buf, buflen );
   
   // expand size?
   if( (unsigned)(offset + buflen) > inode->size ) {
      inode->size = offset + buflen;
   }
   
   return buflen;
}

// truncate a file 
// return 0 on success, and reset the size and RAM buffer 
// return -ENOMEM on OOM 
// return -ENOSYS if for some reason we don't have an inode (should *never* happen)
// use under the FSKIT_INODE_SEQUENTIAL consistency discipline--the entry will be write-locked when we call this method.
int runfs_truncate( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, off_t new_size, void* inode_data ) {
   
   runfs_debug("runfs_truncate(%s) from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   struct runfs_inode* inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
   size_t new_contents_len = inode->contents_len;
   
   if( inode == NULL ) {
      return -ENOSYS;
   }
   
   // expand?
   if( (unsigned)new_size >= inode->contents_len ) {
      
      if( new_contents_len == 0 ) {
         new_contents_len = 1;
      }
      
      while( (unsigned)new_size > new_contents_len ) {
         
         new_contents_len *= 2;
      }
      
      char* tmp = (char*)realloc( inode->contents, new_contents_len );
      
      if( tmp == NULL ) {
         return -ENOMEM;
      }
      
      inode->contents = tmp;
      
      memset( inode->contents + inode->contents_len, 0, new_contents_len - inode->contents_len );
      
      inode->contents_len = new_contents_len;
   }
   else {
      
      memset( inode->contents + new_size, 0, inode->contents_len - new_size );
   }
   
   // new size 
   inode->size = new_size;
   
   return 0;
}

// remove a file or directory 
// return 0 on success, and free up the given inode_data
int runfs_detach( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, void* inode_data ) {
   
   runfs_debug("runfs_detach('%s') from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   struct runfs_inode* inode = (struct runfs_inode*)inode_data;
   
   if( inode != NULL ) {
      runfs_inode_free( inode );
      runfs_safe_free( inode );
   }
   
   return 0;
}

// stat an entry 
// garbage-collect an entry (and its children) if the process that created it died.
// return 0 on success 
// return -ENOENT if the path does not exist
// return -EIO if the inode is invalid 
// needs per-inode sequential consistency 
int runfs_stat( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, struct stat* sb ) {
   
   runfs_debug("runfs_stat('%s') from %d\n", fskit_route_metadata_get_path( route_metadata ), fskit_fuse_get_pid() );
   
   int rc = 0;
   struct runfs_state* runfs = (struct runfs_state*)fskit_core_get_user_data( core );
   
   fskit_entry_rlock( fent );
   
   struct runfs_inode* inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
   
   if( inode == NULL ) {
       
      fskit_entry_unlock( fent );
      return 0;
   }
   
   if( inode->deleted ) {
       
      fskit_entry_unlock( fent );
      runfs_debug("%s was deleted\n", fskit_route_metadata_get_path( route_metadata ));
      return -ENOENT;
   }
   
   pid_t pid = pstat_get_pid( inode->ps );
   
   rc = runfs_inode_is_valid( inode );
   if( rc < 0 ) {
      
      char path[PATH_MAX+1];
      pstat_get_path( inode->ps, path );
      
      runfs_error( "runfs_inode_is_valid(path=%s, pid=%d) rc = %d\n", path, pid, rc );
      
      // no longer valid
      rc = 0;
   }
   
   if( rc == 0 ) {
      
      // no longer valid.  detach 
      // upgrade to write-lock 
      fskit_entry_unlock( fent );
      fskit_entry_wlock( fent );
      
      inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
      if( inode == NULL ) {
          
          fskit_entry_unlock( fent );
          return -ENOENT;
      }
      
      if( inode->deleted ) {
          
          // someone else raced us 
          fskit_entry_unlock( fent );
          return -ENOENT;
      }
      
      inode->deleted = true;
      fskit_entry_set_user_data( fent, NULL );
      
      runfs_inode_free( inode );
      runfs_safe_free( inode );
      
      uint64_t inode_number = fskit_entry_get_file_id( fent );
      rc = runfs_deferred_remove( runfs, fskit_route_metadata_get_path( route_metadata ), fent );
      
      if( rc != 0 ) {
          runfs_error("runfs_deferred_remove('%s' (%" PRIX64 ") rc = %d\n", fskit_route_metadata_get_path( route_metadata ), inode_number, rc );
      }
      else {
          runfs_debug("Detached '%s' because it is orphaned (PID %d)\n", fskit_route_metadata_get_path( route_metadata ), pid );
          rc = -ENOENT;
      }
      
      fskit_entry_unlock( fent );
   }
   else {
       
      fskit_entry_unlock( fent );
      runfs_debug("'%s' (created by %d) is still valid\n", fskit_route_metadata_get_path( route_metadata ), pid );
      rc = 0;
   }
   
   return rc;
}

// read a directory
// stat each node in it, and remove ones whose creating process has died
// we need concurrent per-inode locking (i.e. read-lock the directory)
int runfs_readdir( struct fskit_core* core, struct fskit_route_metadata* route_metadata, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents ) {
   
   runfs_debug("runfs_readdir(%s, %zu) from %d\n", fskit_route_metadata_get_path( route_metadata ), num_dirents, fskit_fuse_get_pid() );
   
   int rc = 0;
   struct fskit_entry* child = NULL;
   struct runfs_inode* inode = NULL;
   
   struct runfs_state* runfs = (struct runfs_state*)fskit_core_get_user_data( core );
   
   int* omitted = RUNFS_CALLOC( int, num_dirents );
   if( omitted == NULL ) {
       return -ENOMEM;
   }
   
   int omitted_idx = 0;
   
   for( unsigned int i = 0; i < num_dirents; i++ ) {
      
      // skip . and ..
      if( strcmp(dirents[i]->name, ".") == 0 || strcmp(dirents[i]->name, "..") == 0 ) {
         continue;
      }
      
      // find the associated fskit_entry
      child = fskit_dir_find_by_name( fent, dirents[i]->name );
      
      if( child == NULL ) {
         // strange, shouldn't happen...
         continue;
      }
      
      fskit_entry_rlock( child );
      
      inode = (struct runfs_inode*)fskit_entry_get_user_data( child );
      
      if( inode == NULL ) {
         // skip
         fskit_entry_unlock( child );
         continue;
      }
      
      // already marked for deletion?
      if( inode->deleted ) {
         // skip
         fskit_entry_unlock( child );
         
         omitted[ omitted_idx ] = i;
         omitted_idx++;
         continue;
      }
      
      // is this file still valid?
      int valid = runfs_inode_is_valid( inode );
      
      if( valid < 0 ) {
         
         char path[PATH_MAX+1];
         pstat_get_path( inode->ps, path );
         
         runfs_error( "runfs_inode_is_valid(path=%s, pid=%d) rc = %d\n", path, pstat_get_pid( inode->ps ), valid );
         
         valid = 0;
      }
      
      fskit_entry_unlock( child );
      
      if( valid == 0 ) {
         
         // not valid--creator has died.
         // upgrade the lock to a write-lock, so we can garbage-collect 
         fskit_entry_wlock( child );
      
         inode = (struct runfs_inode*)fskit_entry_get_user_data( child );
      
         if( inode == NULL ) {
             
             // no longer valid
             omitted[ omitted_idx ] = i;
             omitted_idx++;
             fskit_entry_unlock( child );
             continue;
         }
         
         if( inode->deleted ) {
             // someone raced us 
             fskit_entry_unlock( child );
             
             omitted[ omitted_idx ] = i;
             omitted_idx++;
             continue;
         }
      
         // flag deleted
         inode->deleted = true;
         
         uint64_t child_id = fskit_entry_get_file_id( child );
         char* child_fp = fskit_fullpath( fskit_route_metadata_get_path( route_metadata ), dirents[i]->name, NULL );
         if( child_fp == NULL ) {
             
            fskit_entry_unlock( child );
            rc = -ENOMEM;
            break;
         }
         
         // garbage-collect
         rc = runfs_deferred_remove( runfs, child_fp, child );
         fskit_entry_unlock( child );
         
         if( rc != 0 ) {
            
            runfs_error("runfs_deferred_remove('%s' (%" PRIX64 ")) rc = %d\n", child_fp, child_id, rc );
         }
         
         free( child_fp );
         
         // omit this child from the listing
         omitted[ omitted_idx ] = i;
         omitted_idx++;
      }
   }
   
   for( int i = 0; i < omitted_idx; i++ ) {
      
      fskit_readdir_omit( dirents, omitted[i] );
   }
   
   runfs_safe_free( omitted );
   
   return rc;
}

// run! 
int main( int argc, char** argv ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_fuse_state state;
   struct fskit_core* core = NULL;
   struct runfs_state runfs;
   
   // setup runfs state 
   memset( &runfs, 0, sizeof(struct runfs_state) );
   
   runfs.deferred_unlink_wq = runfs_wq_new();
   if( runfs.deferred_unlink_wq == NULL ) {
      exit(1);
   }
   
   rc = runfs_wq_init( runfs.deferred_unlink_wq );
   if( rc != 0 ) {
      fprintf(stderr, "runfs_wq_init rc = %d\n", rc );
      exit(1);
   }
   
   // set up fskit state
   rc = fskit_fuse_init( &state, &runfs );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }
   
   // make sure the fs can access its methods through the VFS
   fskit_fuse_setting_enable( &state, FSKIT_FUSE_SET_FS_ACCESS );
   
   core = fskit_fuse_get_core( &state );
   
   // plug core into runfs
   runfs.core = core;
   
   // add handlers.  reads and writes must happen sequentially, since we seek and then perform I/O
   // NOTE: FSKIT_ROUTE_ANY matches any path, and is a macro for the regex "/([^/]+[/]*)+"
   rh = fskit_route_create( core, FSKIT_ROUTE_ANY, runfs_create, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_create(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_mkdir( core, FSKIT_ROUTE_ANY, runfs_mkdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_mkdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_mknod( core, FSKIT_ROUTE_ANY, runfs_mknod, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_mknod(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_readdir( core, FSKIT_ROUTE_ANY, runfs_readdir, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_readdir(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_read( core, FSKIT_ROUTE_ANY, runfs_read, FSKIT_INODE_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_read(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_write( core, FSKIT_ROUTE_ANY, runfs_write, FSKIT_INODE_SEQUENTIAL );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_write(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_trunc( core, FSKIT_ROUTE_ANY, runfs_truncate, FSKIT_INODE_SEQUENTIAL );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_trunc(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
      
   rh = fskit_route_detach( core, FSKIT_ROUTE_ANY, runfs_detach, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_detach(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_stat( core, FSKIT_ROUTE_ANY, runfs_stat, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_stat(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   // set the root to be owned by the effective UID and GID of user
   fskit_chown( core, "/", 0, 0, geteuid(), getegid() );
   
   // begin taking deferred requests 
   rc = runfs_wq_start( runfs.deferred_unlink_wq );
   if( rc != 0 ) {
      fprintf(stderr, "runfs_wq_start rc = %d\n", rc );
      exit(1);
   }
   
   // run 
   rc = fskit_fuse_main( &state, argc, argv );
   
   // shutdown
   fskit_fuse_shutdown( &state, NULL );
   
   runfs_wq_stop( runfs.deferred_unlink_wq );
   runfs_wq_free( runfs.deferred_unlink_wq );
   runfs_safe_free( runfs.deferred_unlink_wq );
   
   return rc;
}

