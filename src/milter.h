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
  bindings ()
  : first(NULL), last(NULL)
  {
    // TODO: deal with error case
    pthread_mutex_init(&lck_queue, NULL);
  }

  ~bindings ()
  {
    // TODO: lock queue, clear it, unlock it
    // ensure no further events can be delivered

    fcall.connect.Reset();
    fcall.close.Reset();
    pthread_mutex_destroy(&lck_queue);
  }

  uv_loop_t *loop;
  uv_work_t request;
  uv_async_t trigger;

  pthread_mutex_t lck_queue;
  MilterEvent *first, *last;

  struct
  {
    Persistent<Function> connect;
    Persistent<Function> close;
  } fcall;

  int retval;
};


struct envelope
{
  envelope (bindings_t *local) : local(local)
  { }

  bindings_t *local;

  Persistent<Object> object;
};

#endif
