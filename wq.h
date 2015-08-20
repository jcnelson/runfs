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

#ifndef _RUNFS_WQ_H_
#define _RUNFS_WQ_H_

#include "os.h"
#include "util.h"

struct runfs_wreq;

// runfs workqueue callback type
typedef int (*runfs_wq_func_t)( struct runfs_wreq* wreq, void* cls );

// runfs workqueue request
struct runfs_wreq {

   // callback to do work
   runfs_wq_func_t work;

   // user-supplied arguments
   void* work_data;
   
   struct runfs_wreq* next;     // pointer to next work element
};

// runfs workqueue
struct runfs_wq {
   
   // worker thread
   pthread_t thread;

   // is the thread running?
   volatile bool running;

   // things to do
   struct runfs_wreq* work;
   struct runfs_wreq* tail;

   // lock governing access to work
   pthread_mutex_t work_lock;

   // semaphore to signal the availability of work
   sem_t work_sem;
};

struct runfs_wq* runfs_wq_new();
int runfs_wq_init( struct runfs_wq* wq );
int runfs_wq_start( struct runfs_wq* wq );
int runfs_wq_stop( struct runfs_wq* wq );
int runfs_wq_free( struct runfs_wq* wq );

int runfs_wreq_init( struct runfs_wreq* wreq, runfs_wq_func_t work, void* work_data );
int runfs_wreq_free( struct runfs_wreq* wreq );

int runfs_wq_add( struct runfs_wq* wq, struct runfs_wreq* wreq );

#endif
