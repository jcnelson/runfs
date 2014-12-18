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

#include "runfs.h"

static int runfs_make_inode( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, void** inode_data ) {
   
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

int runfs_create( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, void** inode_data, void** handle_data ) {
   
   fskit_debug("runfs_create(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, grp, fent, mode, inode_data );
}

// track sockets, etc.
int runfs_mknod( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   
   fskit_debug("runfs_mknod(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, grp, fent, mode, inode_data );
}

// track directories 
int runfs_mkdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* dent, mode_t mode, void** inode_data ) {
   
   fskit_debug("runfs_mkdir(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
   return runfs_make_inode( core, grp, dent, mode, inode_data );
}


int runfs_read( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   fskit_debug("runfs_read(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
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


int runfs_write( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {
   
   fskit_debug("runfs_write(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
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


int runfs_detach( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, void* inode_data ) {
   
   fskit_debug("runfs_detach(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
   struct runfs_inode* inode = (struct runfs_inode*)inode_data;
   
   if( inode != NULL ) {
      runfs_inode_free( inode );
      free( inode );
   }
   
   return 0;
}

// stat an entry 
// remove the entry if its creating process has died
int runfs_stat( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct stat* sb ) {
   
   fskit_debug("runfs_stat(%s) from %d\n", grp->path, fskit_fuse_get_pid() );
   
   int rc = 0;
   struct runfs_inode* inode = (struct runfs_inode*)fskit_entry_get_user_data( fent );
   
   if( inode == NULL ) {
      return 0;
   }
   
   if( inode->deleted ) {
      fskit_debug("%s was deleted\n", grp->path);
      return -ENOENT;
   }
   
   rc = runfs_inode_is_valid( inode, inode->ps.pid );
   if( rc < 0 ) {
      
      fskit_error( "runfs_inode_is_valid(path=%s, pid=%d) rc = %d\n", inode->ps.path, inode->ps.pid, rc );
      return -EIO;
   }
   
   if( rc == 0 ) {
      
      // no longer valid--totally remove from the filesystem
      inode->deleted = true;
      
      if( fent->type == FSKIT_ENTRY_TYPE_DIR ) {
         fskit_deferred_remove_all( core, grp->path, fent );
      }
      else {
         fskit_deferred_remove( core, grp->path, fent );
      }
      
      fskit_debug("%s is no longer valid (PID %d)\n", grp->path, inode->ps.pid);
      rc = -ENOENT;
   }
   else {
      rc = 0;
   }
   
   return rc;
}

// read a directory
// stat each node in it, and remove ones whose creating process has died
int runfs_readdir( struct fskit_core* core, struct fskit_match_group* grp, struct fskit_entry* fent, struct fskit_dir_entry** dirents, size_t num_dirents ) {
   
   fskit_debug("runfs_readdir(%s, %zu) from %d\n", grp->path, num_dirents, fskit_fuse_get_pid() );
   
   int rc = 0;
   struct fskit_entry* child = NULL;
   struct runfs_inode* inode = NULL;
   
   // entries to omit in the listing
   vector<int> omitted_idx;
   
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
         omitted_idx.push_back( i );
         continue;
      }
      
      // is this file still valid?
      int valid = runfs_inode_is_valid( inode, inode->ps.pid );
      if( valid < 0 ) {
         
         fskit_error( "runfs_inode_is_valid(path=%s, pid=%d) rc = %d\n", inode->ps.path, inode->ps.pid, valid );
         fskit_entry_unlock( child );
         
         rc = -EIO;
         break;
      }
      
      if( valid == 0 ) {
         
         // not valid--unlink
         inode->deleted = true;
         
         char* child_fp = fskit_fullpath( grp->path, child->name, NULL );
         if( child_fp == NULL ) {
            rc = -ENOMEM;
            break;
         }
         
         // totally remove from the filesystem
         if( child->type == FSKIT_ENTRY_TYPE_DIR ) {
            fskit_deferred_remove_all( core, child_fp, child );
         }
         else {
            fskit_deferred_remove( core, child_fp, child );
         }
         
         free( child_fp );
         
         omitted_idx.push_back( i );
      }
      
      fskit_entry_unlock( child );
   }
   
   for( unsigned int i = 0; i < omitted_idx.size(); i++ ) {
      
      fskit_readdir_omit( dirents, omitted_idx[i] );
   }
   
   return rc;
}

// run! 
int main( int argc, char** argv ) {
   
   int rc = 0;
   int rh = 0;
   struct fskit_fuse_state state;
   struct fskit_core* core = NULL;
   
   rc = fskit_fuse_init( &state, NULL );
   if( rc != 0 ) {
      fprintf(stderr, "fskit_fuse_init rc = %d\n", rc );
      exit(1);
   }
   
   // make sure the fs can access its methods through the VFS
   fskit_fuse_setting_enable( &state, FSKIT_FUSE_SET_FS_ACCESS );
   
   core = fskit_fuse_get_core( &state );
   
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
   
   rh = fskit_route_read( core, FSKIT_ROUTE_ANY, runfs_read, FSKIT_CONCURRENT );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_read(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
      exit(1);
   }
   
   rh = fskit_route_write( core, FSKIT_ROUTE_ANY, runfs_write, FSKIT_INODE_SEQUENTIAL );
   if( rh < 0 ) {
      fprintf(stderr, "fskit_route_write(%s) rc = %d\n", FSKIT_ROUTE_ANY, rh );
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
   
   // run 
   rc = fskit_fuse_main( &state, argc, argv );
   
   // shutdown
   fskit_fuse_shutdown( &state, NULL );
   
   return rc;
}

