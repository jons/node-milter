/**
 * contextual information for events
 * it is apparently fashionable in node.js-speak to call these "batons"
 *
 * do you like 1980s C style standards for variable names? no? haha!
 *
 * do you like encapsulation? :)
 *
 * hahaha!
 */

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>
#include "libmilter/mfapi.h"
#include "milter.h"
#include "events.h"

using namespace v8;
using namespace node;


MilterEvent::MilterEvent (envelope_t *env)
    : ev_next(NULL),
      is_done(false),
      fi_envelope(env),
      fi_retval(0)
    {
      pthread_mutex_init(&this->pt_lock, NULL);
      pthread_cond_init(&this->pt_ready, NULL);
    }


bool MilterEvent::Lock () { return 0 == pthread_mutex_lock(&this->pt_lock); }

bool MilterEvent::Unlock () { return 0 == pthread_mutex_unlock(&this->pt_lock); }

bool MilterEvent::Wait () { return 0 == pthread_cond_wait(&this->pt_ready, &this->pt_lock); }

bool MilterEvent::Done ()
{
  this->is_done = (0 == pthread_cond_signal(&this->pt_ready));
  return this->is_done;
}

void MilterEvent::Append (MilterEvent *ev)
{
  assert(this->ev_next == NULL);
  this->ev_next = ev;
}

void MilterEvent::Detatch ()
{
  this->ev_next = NULL;
}

MilterEvent *MilterEvent::Next () const { return this->ev_next; }

bool MilterEvent::IsDone () const { return this->is_done; }

int MilterEvent::Result () const { return this->fi_retval; }

/*virtual*/ MilterEvent::~MilterEvent ()
{
  pthread_cond_destroy(&this->pt_ready);
  pthread_mutex_destroy(&this->pt_lock);
}

/**
 */
void MilterEvent::SetResult (int retval)
{
  this->is_done = true;
  this->fi_retval = retval;
}

/**
 * must be called during trigger_event
 * create new envelope on connect... or negotiate. hmm
 */
Local<Object> MilterEvent::CreateEnvelope (Isolate *isolate)
{
  char debugenv[1024] = {'\0'};
  Local<Object> object = Object::New(isolate);
  this->fi_envelope->object.Reset(isolate, object);
  // TODO: add relevant and meaningful contextual information
  snprintf(debugenv, sizeof(debugenv)-1, "%p", this->fi_envelope);
  object->Set(String::NewFromUtf8(isolate, "debugenv", String::kInternalizedString), String::NewFromUtf8(isolate, debugenv));
  return object;
}

/**
 * must be called during trigger_event
 * restore existing object for session
 */
Local<Object> MilterEvent::RestoreEnvelope (Isolate *isolate)
{
  return Local<Object>::New(isolate, this->fi_envelope->object);
}


/**
 * shared wrapper for Fire() implementors
 * all events are triggered in the same manner
 */
Handle<Value> MilterEvent::EventWrap (Isolate *isolate, Persistent<Function> &pfunc, unsigned int argc, Local<Value> *argv)
{
  TryCatch tc;
  Local<Function> fcall = Local<Function>::New(isolate, pfunc);
  Handle<Value> h = fcall->Call(isolate->GetCurrentContext()->Global(), argc, argv);
  if (tc.HasCaught())
  {
    FatalException(tc);
    // XXX: this value isn't special; it just coaxes the caller into telling libmilter to TEMPFAIL
    return Undefined(isolate);
  }
  return h;
}


/** MilterConnect *************************************************************/

MilterConnect::MilterConnect (envelope_t *env, const char *host, sockaddr_in *sa)
  : MilterEvent(env), sz_host(host)
{
  inet_ntop(AF_INET, &((sockaddr_in *)sa)->sin_addr, sz_addr, sizeof sz_addr);
}

void MilterConnect::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 3;
  Local<Value> env = this->CreateEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, this->sz_host),
    String::NewFromUtf8(isolate, this->sz_addr)
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.connect, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}

const char *MilterConnect::Host() const { return this->sz_host; }

const char *MilterConnect::Address() const { return this->sz_addr; }


/** MilterUnknown *************************************************************/

MilterUnknown::MilterUnknown (envelope_t *env, const char *command)
  : MilterEvent(env), sz_command(command) { }

void MilterUnknown::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, this->sz_command)
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.unknown, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterHELO ***************************************************************/

MilterHELO::MilterHELO (envelope_t *env, const char *helo)
  : MilterEvent(env), sz_helo(helo) { }

void MilterHELO::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, sz_helo)
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.helo, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterMAILFROM ***************************************************************/

MilterMAILFROM::MilterMAILFROM (envelope_t *env, char **argv)
  : MilterEvent(env), szpp_argv(argv) { }

void MilterMAILFROM::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, szpp_argv[0])
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.envfrom, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterRCPTTO ***************************************************************/

MilterRCPTTO::MilterRCPTTO (envelope_t *env, char **argv)
  : MilterEvent(env), szpp_argv(argv) { }

void MilterRCPTTO::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, szpp_argv[0])
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.envrcpt, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterDATA ***************************************************************/

MilterDATA::MilterDATA (envelope_t *env) : MilterEvent(env) { }

void MilterDATA::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.data, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterHeader ***************************************************************/

MilterHeader::MilterHeader (envelope_t *env, const char *name, const char *value)
  : MilterEvent(env), sz_name(name), sz_value(value) { }

void MilterHeader::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 3;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    String::NewFromUtf8(isolate, sz_name),
    String::NewFromUtf8(isolate, sz_value)
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.header, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterEndHeaders ***************************************************************/

MilterEndHeaders::MilterEndHeaders (envelope_t *env) : MilterEvent(env) { }

void MilterEndHeaders::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.eoh, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterMessageData ***************************************************************/

MilterMessageData::MilterMessageData (envelope_t *env, const unsigned char *buf, const size_t len)
  : MilterEvent(env), buf(buf), len(len) { }

void MilterMessageData::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 3;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
    Buffer::Use(isolate, (char *)buf, len), // TODO: unsure if safe
    Number::New(isolate, len)
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.body, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterEndMessage ***************************************************************/

MilterEndMessage::MilterEndMessage (envelope_t *env) : MilterEvent(env) { }

void MilterEndMessage::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.eom, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterAbort ***************************************************************/

MilterAbort::MilterAbort (envelope_t *env) : MilterEvent(env) { }

void MilterAbort::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.abort, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}


/** MilterClose ***************************************************************/

MilterClose::MilterClose (envelope_t *env) : MilterEvent(env) { }

void MilterClose::Fire (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> env = this->RestoreEnvelope(isolate);
  Local<Value> argv[argc] = {
    env,
  };
  Handle<Value> h = this->EventWrap(isolate, local->fcall.close, argc, argv);
  this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
}
