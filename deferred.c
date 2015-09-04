/*
   runfs: a self-cleaning filesystem for runtime state.
   Copyright (C) 2015  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include "deferred.h"
#include "runfs.h"

// deferred remove-all context
struct runfs_deferred_remove_ctx {

   struct fskit_core* core;
   char* fs_path;               // path to the entry to remove
   fskit_entry_set* children;   // the (optional) children to remove (not yet garbage-collected)
};


// helper to asynchronously try to unlink an inode and its children
static int runfs_deferred_remove_cb( struct runfs_wreq* wreq, void* cls ) {

   struct runfs_deferred_remove_ctx* ctx = (struct runfs_deferred_remove_ctx*)cls;
   struct fskit_detach_ctx* dctx = NULL;
   int rc = 0;
   
   runfs_debug("DEFERRED: remove '%s'\n", ctx->fs_path );
   
   // remove the children 
   if( ctx->children != NULL ) {
       
      dctx = fskit_detach_ctx_new();
      if( dctx == NULL ) {
         return -ENOMEM;
      }

      rc = fskit_detach_ctx_init( dctx );
      if( rc != 0 ) {
         return rc;
      }

      // proceed to detach
      while( true ) {
         
         rc = fskit_detach_all_ex( ctx->core, ctx->fs_path, &ctx->children, dctx );
         if( rc == 0 ) {
             break;
         }
         else if( rc == -ENOMEM ) {
             continue;
         }
         else {
             break;
         }
      }
      
      fskit_detach_ctx_free( dctx );
      runfs_safe_free( dctx );
      
      fskit_entry_set_free( ctx->children );
   }

   runfs_safe_free( ctx->fs_path );
   runfs_safe_free( ctx );
   
   return 0;
}


// Garbage-collect the given inode, and queue it for unlinkage.
// If the inode is a directory, recursively garbage-collect its children as well, and queue them and their descendents for unlinkage
// return 0 on success
// NOTE: child must be write-locked
int runfs_deferred_remove( struct runfs_state* runfs, char const* child_path, struct fskit_entry* child ) {

   struct runfs_deferred_remove_ctx* ctx = NULL;
   struct fskit_core* core = runfs->core;
   struct runfs_wreq* work = NULL;
   fskit_entry_set* children = NULL;
   int rc = 0;

   // asynchronously unlink it and its children
   ctx = RUNFS_CALLOC( struct runfs_deferred_remove_ctx, 1 );
   if( ctx == NULL ) {
       return -ENOMEM;
   }
   
   work = RUNFS_CALLOC( struct runfs_wreq, 1 );
   if( work == NULL ) {
       
       runfs_safe_free( ctx );
       return -ENOMEM;
   }
   
   // set up the deferred unlink request 
   ctx->core = core;
   ctx->fs_path = strdup( child_path );
   
   if( ctx->fs_path == NULL ) {
       
       runfs_safe_free( work );
       runfs_safe_free( ctx );
       return -ENOMEM;
   }
   
   // garbage-collect this child
   rc = fskit_entry_tag_garbage( child, &children );
   if( rc != 0 ) {
       
       runfs_safe_free( ctx );
       runfs_safe_free( work );
       
       char name[FSKIT_FILESYSTEM_NAMEMAX+1];
       fskit_entry_copy_name( child, name, FSKIT_FILESYSTEM_NAMEMAX );
       
       runfs_error("fskit_entry_garbage_collect('%s') rc = %d\n", name, rc );
       return rc;
   }
   
   ctx->children = children;
   
   // deferred removal 
   runfs_wreq_init( work, runfs_deferred_remove_cb, ctx );
   runfs_wq_add( runfs->deferred_unlink_wq, work );
   
   return 0;
}

