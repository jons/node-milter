/**
 */

#ifndef MILTER_H
#define MILTER_H

#include <node/uv.h>
#include <node.h>
#include <v8.h>
#include "forward.h"

using namespace node;
using namespace v8;


struct bindings
{
  uv_loop_t *loop;
  uv_work_t request;
  uv_async_t trigger;

  pthread_mutex_t lck_queue;
  MilterEvent *first, *last;

  //Persistent<Object, CopyablePersistentTraits<Object> > event_context;
  Persistent<Object> event_context;

  int retval;
};


struct envelope
{
  envelope (bindings_t *local) : local(local)
  { }

  bindings_t *local;

  Persistent<Value> object;
};

#endif
