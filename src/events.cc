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
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>
#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include "libmilter/mfapi.h"
#include "milter.h"
#include "events.h"
#include "envelope.h"

using namespace v8;
using namespace node;


MilterEvent::MilterEvent (envelope_t *env)
    : fi_envelope(env),
      ev_next(NULL),
      is_done(false),
      fi_retval(SMFIS_TEMPFAIL) // TODO: allow custom default
    {
      pthread_mutex_init(&this->pt_lock, NULL);
      pthread_cond_init(&this->pt_ready, NULL);
    }


bool MilterEvent::Lock () { return 0 == pthread_mutex_lock(&this->pt_lock); }

bool MilterEvent::Unlock () { return 0 == pthread_mutex_unlock(&this->pt_lock); }

bool MilterEvent::Wait () { return 0 == pthread_cond_wait(&this->pt_ready, &this->pt_lock); }


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

void MilterEvent::SetMilterContext (SMFICTX *context)
{
  assert(NULL != context);
  this->smfi_context = context;
}

bool MilterEvent::IsDone () const { return this->is_done; }

int MilterEvent::Result () const { return this->fi_retval; }

/*virtual*/ MilterEvent::~MilterEvent ()
{
  pthread_cond_destroy(&this->pt_ready);
  pthread_mutex_destroy(&this->pt_lock);
}


/**
 * the last call made by the script kiddie from a callback is:
 *     envelope.done(retval)
 * that then returns here, via the envelope's pointer to the current event
 *
 * xxfi_close will always be a MilterClose which will cause Postfire() to
 * Reset() its persistent envelope handle
 */
bool MilterEvent::Done (Isolate *isolate, int retval)
{
  this->Postfire(isolate);

  this->fi_retval = retval;
  this->is_done = (0 == pthread_cond_signal(&this->pt_ready));

  // throw exception if the sleeping libmilter thread is not signaled
  if (!this->is_done)
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Condition signal failed")));

  // throw exception if event is not yielded back to pthread
  if (!this->Unlock())
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Event unlock failed")));

  return this->is_done;
}


/**
 * caling envelope.whatever() outside of certain events is largely forbidden
 */
bool MilterEvent::IsNegotiate () const
{
  return false;
}
bool MilterEvent::IsConnect () const
{
  return false;
}
bool MilterEvent::IsEndMessage () const
{
  return false;
}



/**
 * the default prefire event
 *
 * must be called during trigger_event
 * restore existing object for session
 */
void MilterEvent::Prefire (Isolate *isolate, HandleScope &scope)
{
  this->envelope = Local<Object>::New(isolate, this->fi_envelope->object);
}

/**
 * the default postfire event (do nothing)
 */
void MilterEvent::Postfire (Isolate *isolate)
{
  // naaaah-thing
}


/**
 */
void MilterEvent::Fire (Isolate *isolate, bindings_t *local)
{
  HandleScope scope (isolate);
  this->Prefire(isolate, scope);

  Envelope *env = ObjectWrap::Unwrap<Envelope>(this->envelope);
  env->SetCurrentEvent(this);
  env->SetMilterContext(this->smfi_context);

  this->FireWrapper(isolate, local);
}


/**
 * shared wrapper for each event implementation
 * XXX: maybe inline this for maximum gofast
 */
void MilterEvent::DoFCall (Isolate *isolate, Persistent<Function> &pfunc, unsigned int argc, Local<Value> *argv)
{
  TryCatch tc;
  Local<Function> fcall = Local<Function>::New(isolate, pfunc);
  fcall->Call(isolate->GetCurrentContext()->Global(), argc, argv);
  if (tc.HasCaught())
    FatalException(tc);
}


/** MilterNegotiate ***********************************************************/

MilterNegotiate::MilterNegotiate (envelope_t *env,
    unsigned long f0_,
    unsigned long f1_,
    unsigned long f2_,
    unsigned long f3_,
    unsigned long *pf0_,
    unsigned long *pf1_,
    unsigned long *pf2_,
    unsigned long *pf3_)
  : MilterEvent(env), f0(f0_), f1(f1_), f2(f2_), f3(f3_), pf0(pf0_), pf1(pf1_), pf2(pf2_), pf3(pf3_)
{
  //
}

bool MilterNegotiate::IsNegotiate () const { return true; }

void MilterNegotiate::Prefire (Isolate *isolate, HandleScope &scope)
{
  this->envelope = Envelope::NewInstance(isolate, scope);
  this->fi_envelope->object.Reset(isolate, this->envelope);
}

/**
 */
void MilterNegotiate::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 5;
  Local<Value> argv[argc] = {
    this->envelope,
    Number::New(isolate, this->f0),
    Number::New(isolate, this->f1),
    Number::New(isolate, this->f2),
    Number::New(isolate, this->f3)
  };
  this->DoFCall(isolate, local->fcall.negotiate, argc, argv);
}


/**
 */
void MilterNegotiate::Negotiate(unsigned long f0, unsigned long f1, unsigned long f2, unsigned long f3)
{
  *pf0 = f0;
  *pf1 = f1;
  *pf2 = f2;
  *pf3 = f3;
}


/** MilterConnect *************************************************************/

MilterConnect::MilterConnect (envelope_t *env, const char *host, sockaddr_in *sa)
  : MilterEvent(env), sz_host(host)
{
  inet_ntop(AF_INET, &((sockaddr_in *)sa)->sin_addr, sz_addr, sizeof sz_addr);
}

bool MilterConnect::IsConnect () const { return true; }

/**
 */
void MilterConnect::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 3;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, this->sz_host),
    String::NewFromUtf8(isolate, this->sz_addr)
  };
  this->DoFCall(isolate, local->fcall.connect, argc, argv);
}

const char *MilterConnect::Host() const { return this->sz_host; }

const char *MilterConnect::Address() const { return this->sz_addr; }


/** MilterUnknown *************************************************************/

MilterUnknown::MilterUnknown (envelope_t *env, const char *command)
  : MilterEvent(env), sz_command(command) { }

/**
 */
void MilterUnknown::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, this->sz_command)
  };
  this->DoFCall(isolate, local->fcall.unknown, argc, argv);
}


/** MilterHELO ***************************************************************/

MilterHELO::MilterHELO (envelope_t *env, const char *helo)
  : MilterEvent(env), sz_helo(helo) { }


void MilterHELO::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, sz_helo)
  };
  this->DoFCall(isolate, local->fcall.helo, argc, argv);
}


/** MilterMAILFROM ***************************************************************/

MilterMAILFROM::MilterMAILFROM (envelope_t *env, char **argv)
  : MilterEvent(env), szpp_argv(argv) { }

void MilterMAILFROM::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, szpp_argv[0])
  };
  this->DoFCall(isolate, local->fcall.envfrom, argc, argv);
}


/** MilterRCPTTO ***************************************************************/

MilterRCPTTO::MilterRCPTTO (envelope_t *env, char **argv)
  : MilterEvent(env), szpp_argv(argv) { }

void MilterRCPTTO::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 2;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, szpp_argv[0])
  };
  this->DoFCall(isolate, local->fcall.envrcpt, argc, argv);
}


/** MilterDATA ***************************************************************/

MilterDATA::MilterDATA (envelope_t *env) : MilterEvent(env) { }

void MilterDATA::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    this->envelope,
  };
  this->DoFCall(isolate, local->fcall.data, argc, argv);
}


/** MilterHeader ***************************************************************/

MilterHeader::MilterHeader (envelope_t *env, const char *name, const char *value)
  : MilterEvent(env), sz_name(name), sz_value(value) { }

void MilterHeader::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 3;
  Local<Value> argv[argc] = {
    this->envelope,
    String::NewFromUtf8(isolate, sz_name),
    String::NewFromUtf8(isolate, sz_value)
  };
  this->DoFCall(isolate, local->fcall.header, argc, argv);
}


/** MilterEndHeaders ***************************************************************/

MilterEndHeaders::MilterEndHeaders (envelope_t *env) : MilterEvent(env) { }

void MilterEndHeaders::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    this->envelope,
  };
  this->DoFCall(isolate, local->fcall.eoh, argc, argv);
}


/** MilterMessageData ***************************************************************/

MilterMessageData::MilterMessageData (envelope_t *env, const unsigned char *buf, const size_t len)
  : MilterEvent(env), buf(buf), len(len) { }

void MilterMessageData::FireWrapper (Isolate *isolate, bindings_t *local)
{
  char *nodebuf = new char[len];
  memcpy(nodebuf, buf, len);
  const unsigned argc = 3;
  Local<Value> argv[argc] = {
    this->envelope,
    Buffer::Use(isolate, nodebuf, len),
    Number::New(isolate, len)
  };
  this->DoFCall(isolate, local->fcall.body, argc, argv);
}


/** MilterEndMessage ***************************************************************/

MilterEndMessage::MilterEndMessage (envelope_t *env) : MilterEvent(env) { }

bool MilterEndMessage::IsEndMessage () const { return true; }

void MilterEndMessage::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    this->envelope,
  };
  this->DoFCall(isolate, local->fcall.eom, argc, argv);
}


/** MilterAbort ***************************************************************/

MilterAbort::MilterAbort (envelope_t *env) : MilterEvent(env) { }

void MilterAbort::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    this->envelope,
  };
  this->DoFCall(isolate, local->fcall.abort, argc, argv);
}


/** MilterClose ***************************************************************/

MilterClose::MilterClose (envelope_t *env) : MilterEvent(env) { }

void MilterClose::FireWrapper (Isolate *isolate, bindings_t *local)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    this->envelope,
  };
  this->DoFCall(isolate, local->fcall.close, argc, argv);
}


/**
 */
void MilterClose::Postfire (Isolate *isolate)
{
  this->fi_envelope->object.Reset();
}
