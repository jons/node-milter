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


/** demultiplexer *************************************************************/


/**
 * all queued events reach node.js from this point; this task is queued on the
 * libuv side, in a loop, via uv_async_send
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
  ev = local->first;
  if (NULL != ev)
  {
    local->first = ev->Next();
    if (NULL == local->first)
      local->last = NULL;
    ev->Detatch();
  }
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

  // launch the appropriate node.js callback for the given event
  ev->Fire(isolate, local);

  // complete the work
  // signal the waiting milter worker thread
  // that thread is still responsible for the event heap memory
  ev->Done();

  // TODO: check return value of Done() from pthread_cond_signal
  ev->Unlock();
}


/** multiplexer ***************************************************************/


/**
 * queue an event from the libmilter side.
 */
int generate_event (bindings_t *local, MilterEvent *event)
{
  int retval;

  // lock the queue
  if (pthread_mutex_lock(&local->lck_queue))
  {
    fprintf(stderr, "node-milter: queue lock failed\n");
    return SMFIS_TEMPFAIL;
  }

  // lock the event
  if (!event->Lock())
  {
    pthread_mutex_unlock(&local->lck_queue);
    // TODO: handle error

    fprintf(stderr, "node-milter: event lock failed\n");
    return SMFIS_TEMPFAIL;
  }

  // enqueue the event while holding both locks
  if (NULL == local->last)
  {
    local->first = event;
    local->last = event;
  }
  else
  {
    local->last->Append(event);
    local->last = event;
  }

  // unlock the queue to allow other libmilter threads to append events
  pthread_mutex_unlock(&local->lck_queue);

  for (;;)
  {
    // let the node loop know we await a result
    uv_async_send(&local->trigger);

    // wait on the event's return condition
    event->Wait();
    // TODO: check error condition
    if (event->IsDone()) break;
    fprintf(stderr, "node-milter: incomplete task, spurious wakeup\n");
  }
  // retrieve return code
  retval = event->Result();

  event->Unlock();
  // TODO: check error condition

  return retval;
}


/** callbacks provided to libmilter *******************************************/


/**
 * complicated MTA-to-milter bullshit nobody needs to fiddle with right now
 *
 * http://www-01.ibm.com/support/knowledgecenter/ssw_aix_71/com.ibm.aix.networkcomm/libmilter_xxfi_negotiate.htm
 */
sfsistat fi_negotiate (SMFICTX *context,
                           unsigned long f0,
                           unsigned long f1,
                           unsigned long f2,
                           unsigned long f3,
                           unsigned long *pf0,
                           unsigned long *pf1,
                           unsigned long *pf2,
                           unsigned long *pf3)
{
  *pf2 = 0;
  *pf3 = 0;
  return SMFIS_ALL_OPTS;
}


/**
 * an incoming connection from the MTA has occurred.
 */
sfsistat fi_connect (SMFICTX *context, char *host, _SOCK_ADDR *sa)
{
  bindings_t *local = (bindings_t *)smfi_getpriv(context);
  int retval;

  // envelope initializer
  // links future events in this thread to the same envelope in node.js
  envelope_t *env = new envelope_t(local);
  smfi_setpriv(context, env);

  // create event, deliver event, block, cleanup
  MilterConnect *event = new MilterConnect(env, host, (sockaddr_in *)sa);
  fprintf(stderr, "connect \"%s\" \"%s\"\n", event->Host(), event->Address());
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_unknown (SMFICTX *context, const char *command)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterUnknown *event = new MilterUnknown(env, command);
  fprintf(stderr, "unknown \"%s\"\n", command);
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_helo (SMFICTX *context, char *helo)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterHELO *event = new MilterHELO(env, helo);
  fprintf(stderr, "helo \"%s\"\n", helo);
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * client "MAIL FROM" command
 */
sfsistat fi_envfrom (SMFICTX *context, char **argv)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterMAILFROM *event = new MilterMAILFROM(env, argv);
  fprintf(stderr, "envfrom \"%s\"\n", argv[0]); // argv[0] is guaranteed
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * client "RCPT TO" command
 */
sfsistat fi_envrcpt (SMFICTX *context, char **argv)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterRCPTTO *event = new MilterRCPTTO(env, argv);
  fprintf(stderr, "envrcpt \"%s\"\n", argv[0]); // argv[0] is guaranteed
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * client "DATA" command
 */
sfsistat fi_data (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterDATA *event = new MilterDATA(env);
  fprintf(stderr, "data\n");
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * detected a valid message header
 */
sfsistat fi_header (SMFICTX *context, char *name, char *value)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterHeader *event = new MilterHeader(env, name, value);
  //fprintf(stderr, "header \"%s\" \"%s\"\n", name, value);
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * detected CRLFCRLF in data stream
 */
sfsistat fi_eoh (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterEndHeaders *event = new MilterEndHeaders(env);
  fprintf(stderr, "eoh\n");
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * nota bene: segment is a byte buffer of unspecified encoding! don't
 * assume it isn't utf-8 just because smtp doesn't truly support that yet!
 */
sfsistat fi_body (SMFICTX *context, unsigned char *segment, size_t len)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterMessageData *event = new MilterMessageData(env, segment, len);
  fprintf(stderr, "message-data (%lu bytes)\n", len);
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * client "." command
 */
sfsistat fi_eom (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterEndMessage *event = new MilterEndMessage(env);
  fprintf(stderr, "eom\n");
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 * probably triggered on client "RSET" or any other connection loss
 *
 * fi_close will still be called afterward, but fi_abort implies the client
 * definitely changed their mind about sending mail
 */
sfsistat fi_abort (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;
  MilterAbort *event = new MilterAbort(env);
  fprintf(stderr, "abort\n");
  retval = generate_event(local, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_close (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  bindings_t *local = env->local;
  int retval;

  MilterClose *event = new MilterClose(env);
  fprintf(stderr, "close\n");
  retval = generate_event(local, event);
  delete event;

  // final teardown sequence
  delete env;
  smfi_setpriv(context, NULL);

  return retval;
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

  desc.xxfi_name      =(char *)g_name;
  desc.xxfi_version   = SMFI_VERSION;
  desc.xxfi_flags     = g_flags;
  desc.xxfi_negotiate = fi_negotiate;
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
#if 0
  // signal handler is not implemented yet
  desc.xxfi_signal    = fi_signal;
#endif

  // connect libuv
  local->request.data = local;
  local->trigger.data = local;

  if (args.Length() < 2)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected string")));
    return;
  }
  // ya dats lazy
  for (int i = 1; i < 13; i++)
    if (!args[1]->IsFunction())
    {
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected function")));
      return;
    }

  Local<String> connstr = args[0]->ToString();
  char *c_connstr = new char[connstr->Utf8Length()+1];
  connstr->WriteUtf8(c_connstr);
  bool ok = false;

  local->fcall.connect.Reset (isolate, Local<Function>::Cast(args[1]));
  local->fcall.unknown.Reset (isolate, Local<Function>::Cast(args[2]));
  local->fcall.helo.Reset    (isolate, Local<Function>::Cast(args[3]));
  local->fcall.envfrom.Reset (isolate, Local<Function>::Cast(args[4]));
  local->fcall.envrcpt.Reset (isolate, Local<Function>::Cast(args[5]));
  local->fcall.data.Reset    (isolate, Local<Function>::Cast(args[6]));
  local->fcall.header.Reset  (isolate, Local<Function>::Cast(args[7]));
  local->fcall.eoh.Reset     (isolate, Local<Function>::Cast(args[8]));
  local->fcall.body.Reset    (isolate, Local<Function>::Cast(args[9]));
  local->fcall.eom.Reset     (isolate, Local<Function>::Cast(args[10]));
  local->fcall.abort.Reset   (isolate, Local<Function>::Cast(args[11]));
  local->fcall.close.Reset   (isolate, Local<Function>::Cast(args[12]));

  if (MI_SUCCESS == smfi_register(desc))
  {
    if (MI_SUCCESS == smfi_setconn(c_connstr))
    {
      local->loop = uv_default_loop();
      uv_async_init(local->loop, &local->trigger, trigger_event);
      uv_queue_work(local->loop, &local->request, milter_worker, milter_cleanup);
      ok = true;
    }
  }
  delete c_connstr;

  if (!ok)
  {
    delete local;

    // TODO: throw exception? report error?
  }

  args.GetReturnValue().Set(Boolean::New(isolate, ok));
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
