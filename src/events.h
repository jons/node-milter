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

#ifndef MILTER_EVENTS_H
#define MILTER_EVENTS_H

#include <node.h>
#include <v8.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>
#include "milter.h"

using namespace v8;
using namespace node;


/**
 * base class. something-something inheritance
 */
class MilterEvent
{
  public:
    // do the event work
    //virtual void Fire (Isolate *isolate, Handle<Object> &context) = 0;
    virtual void Fire (Isolate *isolate, bindings_t *local) = 0;

    // take control of the event
    bool Lock ();
    bool Unlock ();

    /**
     * wait until event is ready
     * this call is used on the libmilter side!
     */
    bool Wait ();

    /**
     * mark the event as complete
     * this call is used on the node.js side!
     */
    bool Done ();

    void Append (MilterEvent *ev);
    void Detatch ();

    //
    MilterEvent *Next () const;
    bool IsDone () const;
    int Result () const;

    virtual ~MilterEvent ();

  protected:
    MilterEvent (envelope_t *env);

    /**
     */
    void SetResult (int retval);

    /**
     */
    Local<Object> CreateEnvelope (Isolate *isolate);
    Local<Object> RestoreEnvelope (Isolate *isolate);
    void DestroyEnvelope (Isolate *isolate);

    /**
     * shared wrapper for Fire() implementors
     * all events are triggered in the same manner
     */
    Handle<Value> EventWrap (Isolate *isolate, Persistent<Function> &pfunc, unsigned int argc, Local<Value> *argv);


  private:
    MilterEvent *ev_next;
    bool is_done;
    envelope_t *fi_envelope;
    int fi_retval;
    pthread_mutex_t pt_lock;
    pthread_cond_t pt_ready;
};


/**
 * new connection established
 *
 * each connection is associated 1:1 with an envelope, created by this event,
 * which lives until either the close or abort event, that allows the node.js
 * script kiddie to associate each individual callback with the correct mail
 * session.
 */
class MilterConnect : public MilterEvent
{
  public:
    MilterConnect (envelope_t *env, const char *host, sockaddr_in *sa);

    /**
     * fire connect event
     */
    //void Fire (Isolate *isolate, Handle<Object> &context);
    void Fire (Isolate *isolate, bindings_t *local);

    const char *Host() const;
    const char *Address() const;

  private:
    const char *sz_host;
    char sz_addr[INET6_ADDRSTRLEN+1];
};


/**
 */
class MilterUnknown : public MilterEvent
{
  public:
    MilterUnknown (envelope_t *env, const char *command);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    const char *sz_command;
};


/**
 */
class MilterHELO : public MilterEvent
{
  public:
    MilterHELO (envelope_t *env, const char *helo);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    const char *sz_helo;
};


/**
 */
class MilterMAILFROM : public MilterEvent
{
  public:
    MilterMAILFROM (envelope_t *env, char **argv);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    char **szpp_argv;
};


/**
 */
class MilterRCPTTO : public MilterEvent
{
  public:
    MilterRCPTTO (envelope_t *env, char **argv);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    char **szpp_argv;
};


/**
 */
class MilterDATA : public MilterEvent
{
  public:
    MilterDATA (envelope_t *env);
    void Fire (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterHeader : public MilterEvent
{
  public:
    MilterHeader (envelope_t *env, const char *name, const char *value);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    const char *sz_name;
    const char *sz_value;
};


/**
 */
class MilterEndHeaders : public MilterEvent
{
  public:
    MilterEndHeaders (envelope_t *env);
    void Fire (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterMessageData : public MilterEvent
{
  public:
    MilterMessageData (envelope_t *env, const unsigned char *buf, const size_t len);
    void Fire (Isolate *isolate, bindings_t *local);

  private:
    const unsigned char *buf;
    const size_t len;
};


/**
 */
class MilterEndMessage : public MilterEvent
{
  public:
    MilterEndMessage (envelope_t *env);
    void Fire (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterAbort : public MilterEvent
{
  public:
    MilterAbort (envelope_t *env);
    void Fire (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterClose : public MilterEvent
{
  public:
    MilterClose (envelope_t *env);
    void Fire (Isolate *isolate, bindings_t *local);
};


#endif
