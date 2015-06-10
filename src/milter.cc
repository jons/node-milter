/**
 * node bindings for milter
 */

#include <string.h>
#include <node/uv.h>
#include <node.h>
#include <v8.h>
#include <arpa/inet.h>
#include "libmilter/mfapi.h"
#include "forward.h"
#include "events.h"

using namespace v8;
using namespace node;


/** globals *******************************************************************/


static const char *g_name = "node-bindings";

static int g_flags = SMFIF_QUARANTINE;


/** helpers********************************************************************/


/**
 */
void queue_init (bindings_t *local)
{
  pthread_mutex_init(&local->lck_queue, NULL);
  local->first = NULL;
  local->last = NULL;
}


/**
 */
void queue_e (bindings_t *local, MilterEvent *ev)
{
  if (NULL == local->last)
  {
    local->first = ev;
    local->last = ev;
  }
  else
  {
    local->last->Append(ev);
    local->last = ev;
  }
}


/**
 */
MilterEvent *queue_d (bindings_t *local)
{
  MilterEvent *ev = local->first;
  if (NULL != ev)
  {
    if (local->last == local->first)
      local->last = NULL;
    local->first = ev->Next();
    ev->Detatch();
  }
  return ev;
}


/** multiplexer ***************************************************************/


/**
 * all queued events reach node.js from this point.
 */
void trigger_event (uv_async_t *h)
{
  bindings *local = (bindings *)h->data;
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  MilterEvent *ev;

  fprintf(stderr, "trigger_event local=%p\n", local);

  // grab the queue lock
  // dequeue one event
  // relinquish the queue lock
  pthread_mutex_lock(&local->lck_queue);
  fprintf(stderr, "trigger_event queue locked\n");
  ev = queue_d(local);
  pthread_mutex_unlock(&local->lck_queue);

  fprintf(stderr, "trigger_event event %p\n", ev);
  if (NULL == ev)
  {
    // TODO: more debugging information
    fprintf(stderr, "spurious wakeup\n");
    return;
  }

  ev->Lock();
  // TODO: return value must be zero for success
  fprintf(stderr, "trigger_event got event lock\n");

  Local<Object> context = Local<Object>::New(isolate, local->event_context);

  // launch the appropriate node.js callback for the given event
  ev->Fire(isolate, context);

  // complete the work
  // signal the waiting milter worker thread
  // that thread is still responsible for the event heap memory
  ev->Done();

  // TODO: check return value of Done() from pthread_cond_signal
  ev->Unlock();
}


/**
 */
sfsistat fi_connect (SMFICTX *context, char *host, _SOCK_ADDR *sa)
{
  bindings_t *local = (bindings_t *)smfi_getpriv(context);
  envelope_t *env = new envelope_t(local);
  int retval;

  // link future events to the same envelope
  smfi_setpriv(context, env);

  // create connect event
  MilterConnect *event = new MilterConnect(env, host, (sockaddr_in *)sa);

  fprintf(stderr, "connect \"%s\" \"%s\"\n", event->Host(), event->Address());

  // lock the queue
  if (pthread_mutex_lock(&local->lck_queue))
  {
    delete event;
    fprintf(stderr, "node-milter: queue lock failed\n");
    return SMFIS_TEMPFAIL;
  }

  // lock the event
  if (!event->Lock())
  {
    delete event;
    pthread_mutex_unlock(&local->lck_queue);
    // TODO: handle error

    fprintf(stderr, "node-milter: event lock failed\n");
    return SMFIS_TEMPFAIL;
  }

  // enqueue the event
  queue_e(local, event);

  // unlock the queue
  pthread_mutex_unlock(&local->lck_queue);

  for (;;)
  {
    // let the node loop know we await a result
    uv_async_send(&local->trigger);

    // wait on the event's return condition
    event->Wait();
    if (event->IsDone()) break;
    fprintf(stderr, "node-milter: incomplete task\n");
  }
  // retrieve return code
  retval = event->Result();

  // destroy event
  event->Unlock();
  delete event;

  // remember this is a unique _pthread_ from libmilter, NOT a libuv or node thread!
  fprintf(stderr, "node-milter: connect retval=%d\n", retval);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_negotiate (SMFICTX *context,
                           unsigned long a,
                           unsigned long b,
                           unsigned long c,
                           unsigned long d,
                           unsigned long *e,
                           unsigned long *f,
                           unsigned long *g,
                           unsigned long *h)
{
  fprintf(stderr, "negotiate %lu %lu %lu %lu\n", a, b, c, d);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_unknown (SMFICTX *context, const char *command)
{
  fprintf(stderr, "unknown \"%s\"\n", command);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_helo (SMFICTX *context, char *helo)
{
  fprintf(stderr, "helo \"%s\"\n", helo);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_envfrom (SMFICTX *context, char **argv)
{
  fprintf(stderr, "envfrom \"%s\"\n", argv[0]);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_envrcpt (SMFICTX *context, char **argv)
{
  fprintf(stderr, "envrcpt \"%s\"\n", argv[0]);
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_data (SMFICTX *context)
{
  fprintf(stderr, "data\n");
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_header (SMFICTX *context, char *name, char *value)
{
  fprintf(stderr, "header\n");
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_eoh (SMFICTX *context)
{
  fprintf(stderr, "eoh\n");
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_body (SMFICTX *context, unsigned char *body, size_t len)
{
  fprintf(stderr, "body\n");
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_eom (SMFICTX *context)
{
  fprintf(stderr, "eom\n");
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_abort (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  fprintf(stderr, "abort\n");

  delete env;
  return SMFIS_CONTINUE;
}


/**
 */
sfsistat fi_close (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  fprintf(stderr, "close\n");

  delete env;
  return SMFIS_CONTINUE;
}


/**
 * run async version of smfi_main, which is blocking
 */
void milter_worker (uv_work_t *request)
{
  bindings_t *local = static_cast<bindings_t *>(request->data);
  int r;
  r = smfi_main(local);

  // TODO: any concurrency issues here?

  local->retval = r;
}


/**
 * cleanup after smfi_main finally stops.
 */
void milter_cleanup (uv_work_t *request, int status)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  bindings_t *local = static_cast<bindings_t *>(request->data);

  // immediately stop event delivery
  uv_close((uv_handle_t *)&local->trigger, NULL);

  // TODO: call a "end" callback provided during instantiation? unsure how to do this, probably with a persistent function

  local->event_context.Reset();

  pthread_mutex_destroy(&local->lck_queue);
  delete local;
}


/**
 */
void milter_start (const FunctionCallbackInfo<Value> &args)
{
  bindings_t *local = new bindings_t();
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  struct smfiDesc desc;
  int r;

  desc.xxfi_name      =(char *)g_name;
  desc.xxfi_version   = SMFI_VERSION;
  desc.xxfi_flags     = g_flags;
  desc.xxfi_connect   = fi_connect;
  desc.xxfi_unknown   = fi_unknown;
  desc.xxfi_helo      = fi_helo;
  desc.xxfi_envfrom   = fi_envfrom;
  desc.xxfi_envrcpt   = fi_envrcpt;
  desc.xxfi_data      = fi_data;
  desc.xxfi_header    = fi_header;
  desc.xxfi_eoh       = fi_eoh;
  desc.xxfi_body      = fi_body;
  desc.xxfi_eom       = fi_eom;
  desc.xxfi_abort     = fi_abort;
  desc.xxfi_close     = fi_close;
  desc.xxfi_negotiate = fi_negotiate;
#if 0
  // signal handler is not implemented yet
  desc.xxfi_signal    = fi_signal;
#endif

  // connect libuv
  local->request.data = local;
  local->trigger.data = local;

  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected string")));
    return;
  }

  queue_init(local);

  Local<String> connstr = args[0]->ToString();
  char *c_connstr = new char[connstr->Utf8Length()+1];
  connstr->WriteUtf8(c_connstr);
  bool ok = false;

  r = smfi_register(desc);
  if (MI_SUCCESS == r)
  {
    r = smfi_setconn(c_connstr);
    if (MI_SUCCESS == r)
    {
      local->loop = uv_default_loop();
      uv_async_init(local->loop, &local->trigger, trigger_event);
      uv_queue_work(local->loop, &local->request, milter_worker, milter_cleanup);
      ok = true;
    }
  }
  delete c_connstr;

  if (ok)
  {
    //Local<Object> obj = Object::New(isolate);

    //obj->Set(String::NewFromUtf8(isolate, "connect", String::kInternalizedString), );

    local->event_context.Reset(isolate, args.This());

    //args.GetReturnValue().Set(obj);
  }
  else
  {
    pthread_mutex_destroy(&local->lck_queue);

    // TODO: if not ok, send error number and description somewhere
    //       or put them in an object and return it, or throw an exception
  }
}


/**
 * module initialization
 */
void init (Handle<Object> target)
{
  Isolate *isolate = Isolate::GetCurrent();

  target->Set(String::NewFromUtf8(isolate, "SMFIS_CONTINUE"), Number::New(isolate, SMFIS_CONTINUE));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_REJECT"),   Number::New(isolate, SMFIS_REJECT));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_DISCARD"),  Number::New(isolate, SMFIS_DISCARD));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_ACCEPT"),   Number::New(isolate, SMFIS_ACCEPT));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_TEMPFAIL"), Number::New(isolate, SMFIS_TEMPFAIL));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_NOREPLY"),  Number::New(isolate, SMFIS_NOREPLY));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_SKIP"),     Number::New(isolate, SMFIS_SKIP));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_ALL_OPTS"), Number::New(isolate, SMFIS_ALL_OPTS));

  NODE_SET_METHOD(target, "start", milter_start);
}

NODE_MODULE(milter, init)
