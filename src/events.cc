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
 * must be called in trigger_event on connect (or negotiate. hmm)
 */
Local<Value> MilterEvent::CreateEnvelope (Isolate *isolate)
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
 * shared wrapper for Fire() implementors
 * all events are triggered in the same manner
 */
Handle<Value> MilterEvent::EventWrap (Isolate *isolate, Handle<Object> &context, const char *ev_name, unsigned int argc, Local<Value> *argv)
{
      TryCatch tc;
      Handle<Value> h = node::MakeCallback(isolate, context, ev_name, argc, argv);
      if (tc.HasCaught())
      {
        FatalException(tc);
        // XXX: this value isn't special; it just coaxes the caller into telling libmilter to TEMPFAIL
        return Undefined(isolate);
      }
      return h;
    }


MilterConnect::MilterConnect (envelope_t *env, const char *host, sockaddr_in *sa)
    : MilterEvent(env), sz_host(host)
    {
      inet_ntop(AF_INET, &((sockaddr_in *)sa)->sin_addr, sz_addr, sizeof sz_addr);
    }

/**
 * fire connect event
 */
void MilterConnect::Fire (Isolate *isolate, Handle<Object> &context)
    {
      const unsigned argc = 3;
      Local<Value> env = this->CreateEnvelope(isolate);
      //Local<Value> env = Local<Value>::New(isolate, this->envelope);
      Local<Value> argv[argc] = {
        env,
        String::NewFromUtf8(isolate, this->sz_host),
        String::NewFromUtf8(isolate, this->sz_addr)
      };
      Handle<Value> h = this->EventWrap(isolate, context, "connect", argc, argv);
      this->SetResult(h->IsNumber() ? h->ToNumber()->IntegerValue() : SMFIS_TEMPFAIL);
    }

const char *MilterConnect::Host() const { return this->sz_host; }

const char *MilterConnect::Address() const { return this->sz_addr; }

