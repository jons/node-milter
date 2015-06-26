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
#include "envelope.h"

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
  Locker locker (isolate);
  HandleScope scope (isolate);
  MilterEvent *ev;

  // grab the queue lock
  if (pthread_mutex_lock(&local->lck_queue))
  {
    fprintf(stderr, "trigger_event: queue lock failed\n");
    return;
  }

  // dequeue one event
  ev = local->first;
  if (NULL == ev)
  {
    pthread_mutex_unlock(&local->lck_queue);
    fprintf(stderr, "trigger_event: spurious wakeup\n");
    return;
  }
  local->first = ev->Next();
  if (NULL == local->first)
    local->last = NULL;
  ev->Detatch();

  if (!ev->Lock())
  {
    fprintf(stderr, "trigger_event: event lock failed\n");

    if (pthread_mutex_unlock(&local->lck_queue))
    {
      fprintf(stderr, "trigger_event: queue unlocked failed\n");
    }
    return;
  }

  // XXX: for top speed, move queue unlock to this point (with or without event lock success)
  if (pthread_mutex_unlock(&local->lck_queue))
  {
    fprintf(stderr, "trigger_event: queue unlock failed\n");
  }

  // launch the appropriate node.js callback for the given event
  ev->Fire(isolate, local);
}


/** multiplexer ***************************************************************/


/**
 * queue an event from the libmilter side.
 */
int generate_event (envelope_t *env, MilterEvent *event)
{
  bindings_t *local = env->local;
  int retval;

  // lock the queue
  if (pthread_mutex_lock(&local->lck_queue))
  {
    fprintf(stderr, "generate_event: queue lock failed\n");
    return SMFIS_TEMPFAIL;
  }

  // lock the event
  if (!event->Lock())
  {
    if (pthread_mutex_unlock(&local->lck_queue))
    {
      fprintf(stderr, "generate_event: queue unlock failed\n");
      // TODO: handle error?
    }

    fprintf(stderr, "generate_event: event lock failed\n");
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

  // unlock the queue to allow other libmilter threads to append events to it
  if (pthread_mutex_unlock(&local->lck_queue))
  {
    // TODO: handle error?
    fprintf(stderr, "generate_event: queue unlock failed\n");
  }

  for (;;)
  {
    // let the node loop know we await a result
    uv_async_send(&local->trigger);

    // wait on the event's return condition
    event->Wait();
    // TODO: check error condition
    if (event->IsDone()) break;
    fprintf(stderr, "generate_event: incomplete task, spurious wakeup\n");
  }
  // retrieve return code
  retval = event->Result();

  if (!event->Unlock())
  {
    // TODO: handle error?
    fprintf(stderr, "generate_event: event unlock failed\n");
  }

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
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "connect \"%s\" \"%s\"\n", event->Host(), event->Address());
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_unknown (SMFICTX *context, const char *command)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterUnknown *event = new MilterUnknown(env, command);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "unknown \"%s\"\n", command);
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_helo (SMFICTX *context, char *helo)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterHELO *event = new MilterHELO(env, helo);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "helo \"%s\"\n", helo);
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * client "MAIL FROM" command
 */
sfsistat fi_envfrom (SMFICTX *context, char **argv)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterMAILFROM *event = new MilterMAILFROM(env, argv);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "envfrom \"%s\"\n", argv[0]); // argv[0] is guaranteed
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * client "RCPT TO" command
 */
sfsistat fi_envrcpt (SMFICTX *context, char **argv)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterRCPTTO *event = new MilterRCPTTO(env, argv);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "envrcpt \"%s\"\n", argv[0]); // argv[0] is guaranteed
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * client "DATA" command
 */
sfsistat fi_data (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterDATA *event = new MilterDATA(env);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "data\n");
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * detected a valid message header
 */
sfsistat fi_header (SMFICTX *context, char *name, char *value)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterHeader *event = new MilterHeader(env, name, value);
#ifdef DEBUG_MILTEREVENT_HEADERS_TOO
  fprintf(stderr, "header \"%s\" \"%s\"\n", name, value);
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * detected CRLFCRLF in data stream
 */
sfsistat fi_eoh (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterEndHeaders *event = new MilterEndHeaders(env);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "eoh\n");
#endif
  retval = generate_event(env, event);
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
  int retval;
  MilterMessageData *event = new MilterMessageData(env, segment, len);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "message-data (%lu bytes)\n", len);
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 * client "." command
 */
sfsistat fi_eom (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;
  MilterEndMessage *event = new MilterEndMessage(env);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "eom\n");
#endif
  retval = generate_event(env, event);
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
  int retval;
  MilterAbort *event = new MilterAbort(env);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "abort\n");
#endif
  retval = generate_event(env, event);
  delete event;
  return retval;
}


/**
 */
sfsistat fi_close (SMFICTX *context)
{
  envelope_t *env = (envelope_t *)smfi_getpriv(context);
  int retval;

  MilterClose *event = new MilterClose(env);
#ifdef DEBUG_MILTEREVENT
  fprintf(stderr, "close\n");
#endif
  retval = generate_event(env, event);
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
  fprintf(stderr, "milter_worker finishing\n");
  // TODO: any concurrency issues here?
  local->retval = r;
}


/**
 * cleanup after smfi_main finally stops.
 */
void milter_cleanup (uv_work_t *request, int status)
{
  Isolate *isolate = Isolate::GetCurrent();
  Locker locker (isolate);
  HandleScope scope (isolate);

  bindings_t *local = static_cast<bindings_t *>(request->data);

  fprintf(stderr, "milter_cleanup started\n");

  // immediately stop event delivery
  uv_close((uv_handle_t *)&local->trigger, NULL);

  //local->fcall.negotiate.Reset();
  local->fcall.connect.Reset();
  local->fcall.unknown.Reset();
  local->fcall.helo.Reset();
  local->fcall.envfrom.Reset();
  local->fcall.envrcpt.Reset();
  local->fcall.data.Reset();
  local->fcall.header.Reset();
  local->fcall.eoh.Reset();
  local->fcall.body.Reset();
  local->fcall.eom.Reset();
  local->fcall.abort.Reset();
  local->fcall.close.Reset();

  // TODO: call a "end" callback provided during instantiation? unsure how to do this, probably with a persistent function

  delete local;
}


/**
 */
void milter_start (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  Locker locker (isolate);
  HandleScope scope(isolate);

  bindings_t *local = new bindings_t();
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

  if (args.Length() < 13)
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
    if (!args[i]->IsFunction())
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
 */
void milter_setbacklog (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  if (!args[0]->IsNumber())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected number")));
    return;
  }
  int n = args[0]->ToNumber()->Value();
  int r = smfi_setbacklog(n);
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void milter_setdbg (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  if (!args[0]->IsNumber())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected number")));
    return;
  }
  int f = args[0]->ToNumber()->Value();
  int r = smfi_setdbg(f);
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void milter_settimeout (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  if (!args[0]->IsNumber())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected number")));
    return;
  }
  int timeo = args[0]->ToNumber()->Value();
  int r = smfi_settimeout(timeo);
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void milter_stop (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  int r = smfi_stop();
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 * module initialization
 */
void init (Handle<Object> target, Handle<Value> module, Handle<Context> context)
{
  Isolate *isolate = Isolate::GetCurrent();

  // macro contexts/locations
  target->Set(String::NewFromUtf8(isolate, "SMFIM_CONNECT", String::kInternalizedString), Number::New(isolate, SMFIM_CONNECT));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_HELO",    String::kInternalizedString), Number::New(isolate, SMFIM_HELO));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_ENVFROM", String::kInternalizedString), Number::New(isolate, SMFIM_ENVFROM));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_ENVRCPT", String::kInternalizedString), Number::New(isolate, SMFIM_ENVRCPT));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_DATA",    String::kInternalizedString), Number::New(isolate, SMFIM_DATA));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_EOM",     String::kInternalizedString), Number::New(isolate, SMFIM_EOM));
  target->Set(String::NewFromUtf8(isolate, "SMFIM_EOH",     String::kInternalizedString), Number::New(isolate, SMFIM_EOH));

  // negotiate flags
  target->Set(String::NewFromUtf8(isolate, "SMFIF_NONE",        String::kInternalizedString), Number::New(isolate, SMFIF_NONE));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_ADDHDRS",     String::kInternalizedString), Number::New(isolate, SMFIF_ADDHDRS));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_CHGBODY",     String::kInternalizedString), Number::New(isolate, SMFIF_CHGBODY));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_MODBODY",     String::kInternalizedString), Number::New(isolate, SMFIF_MODBODY));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_ADDRCPT",     String::kInternalizedString), Number::New(isolate, SMFIF_ADDRCPT));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_DELRCPT",     String::kInternalizedString), Number::New(isolate, SMFIF_DELRCPT));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_CHGHDRS",     String::kInternalizedString), Number::New(isolate, SMFIF_CHGHDRS));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_QUARANTINE",  String::kInternalizedString), Number::New(isolate, SMFIF_QUARANTINE));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_CHGFROM",     String::kInternalizedString), Number::New(isolate, SMFIF_CHGFROM));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_ADDRCPT_PAR", String::kInternalizedString), Number::New(isolate, SMFIF_ADDRCPT_PAR));
  target->Set(String::NewFromUtf8(isolate, "SMFIF_SETSYMLIST",  String::kInternalizedString), Number::New(isolate, SMFIF_SETSYMLIST));

  // normal callback return values
  target->Set(String::NewFromUtf8(isolate, "SMFIS_CONTINUE", String::kInternalizedString), Number::New(isolate, SMFIS_CONTINUE));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_REJECT",   String::kInternalizedString), Number::New(isolate, SMFIS_REJECT));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_DISCARD",  String::kInternalizedString), Number::New(isolate, SMFIS_DISCARD));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_ACCEPT",   String::kInternalizedString), Number::New(isolate, SMFIS_ACCEPT));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_TEMPFAIL", String::kInternalizedString), Number::New(isolate, SMFIS_TEMPFAIL));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_NOREPLY",  String::kInternalizedString), Number::New(isolate, SMFIS_NOREPLY));
  target->Set(String::NewFromUtf8(isolate, "SMFIS_SKIP",     String::kInternalizedString), Number::New(isolate, SMFIS_SKIP));

  // callback return val for negotiate only
  target->Set(String::NewFromUtf8(isolate, "SMFIS_ALL_OPTS", String::kInternalizedString), Number::New(isolate, SMFIS_ALL_OPTS));

  Envelope::Init(target);

  NODE_SET_METHOD(target, "start",      milter_start);
  NODE_SET_METHOD(target, "setbacklog", milter_setbacklog);
  NODE_SET_METHOD(target, "setdbg",     milter_setdbg);
  NODE_SET_METHOD(target, "settimeout", milter_settimeout);
  NODE_SET_METHOD(target, "stop",       milter_stop);
}

NODE_MODULE_CONTEXT_AWARE(milter, init)
