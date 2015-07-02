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
    /**
     * called by node worker to begin the main event work
     */
    void Fire (Isolate *isolate, bindings_t *local);

    /**
     * just a mutex; control of the event
     */
    bool Lock ();
    bool Unlock ();

    /**
     * called by libmilter worker to wait until event is ready
     */
    bool Wait ();

    /**
     * allow an event callback to set a milter result flag
     * returns true if libmilter thread is signaled
     */
    bool Done (Isolate *isolate, int retval);

    /**
     * allow an envelope function to ensure it is not being called at the wrong
     * milter stage.
     * most of them are allowed during EOM only.
     */
    virtual bool IsNegotiate () const;
    virtual bool IsConnect() const;
    virtual bool IsEndMessage () const;

    // linklist junk
    void Append (MilterEvent *ev);
    void Detatch ();
    MilterEvent *Next () const;

    /**
     * 
     */
    void SetMilterContext (SMFICTX *context);

    // not sure if needed
    bool IsDone () const;
    int Result () const;

    virtual ~MilterEvent ();

  protected:
    explicit MilterEvent (envelope_t *env);

    /**
     * each event type must select its associated event callback from
     *
     */
    virtual void FireWrapper (Isolate *isolate, bindings_t *local) = 0;
    virtual void Prefire (Isolate *isolate, HandleScope &scope);
    virtual void Postfire (Isolate *isolate);

    /**
     * each event type must implement
     */
    void DoFCall (Isolate *isolate, Persistent<Function> &pfunc, unsigned int argc, Local<Value> *argv);

    Local<Object> envelope;

    envelope_t *fi_envelope;
    SMFICTX *smfi_context;

  private:
    MilterEvent *ev_next;
    bool is_done;
    int fi_retval;

    pthread_mutex_t pt_lock;
    pthread_cond_t pt_ready;
};


/**
 */
class MilterNegotiate : public MilterEvent
{
  public:
    MilterNegotiate (envelope_t *env,
      unsigned long f0_, unsigned long f1_, unsigned long f2_, unsigned long f3_,
      unsigned long *pf0_, unsigned long *pf1_, unsigned long *pf2_, unsigned long *pf3_);
    void FireWrapper (Isolate *isolate, bindings_t *local);

    bool IsNegotiate () const;

    /**
     * change negotiation settings
     */
    void Negotiate (unsigned long f0, unsigned long f1, unsigned long f2, unsigned long f3);

  protected:
    void Prefire (Isolate *isolate, HandleScope &scope);

  private:
    unsigned long f0;
    unsigned long f1;
    unsigned long f2;
    unsigned long f3;
    unsigned long *pf0;
    unsigned long *pf1;
    unsigned long *pf2;
    unsigned long *pf3;
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
    void FireWrapper (Isolate *isolate, bindings_t *local);

    bool IsConnect() const;

    const char *Host() const;
    const char *Address() const;

  protected:

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
    void FireWrapper (Isolate *isolate, bindings_t *local);

  private:
    const char *sz_command;
};


/**
 */
class MilterHELO : public MilterEvent
{
  public:
    MilterHELO (envelope_t *env, const char *helo);
    void FireWrapper (Isolate *isolate, bindings_t *local);

  private:
    const char *sz_helo;
};


/**
 */
class MilterMAILFROM : public MilterEvent
{
  public:
    MilterMAILFROM (envelope_t *env, char **argv);
    void FireWrapper (Isolate *isolate, bindings_t *local);

  private:
    char **szpp_argv;
};


/**
 */
class MilterRCPTTO : public MilterEvent
{
  public:
    MilterRCPTTO (envelope_t *env, char **argv);
    void FireWrapper (Isolate *isolate, bindings_t *local);

  private:
    char **szpp_argv;
};


/**
 */
class MilterDATA : public MilterEvent
{
  public:
    MilterDATA (envelope_t *env);
    void FireWrapper (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterHeader : public MilterEvent
{
  public:
    MilterHeader (envelope_t *env, const char *name, const char *value);
    void FireWrapper (Isolate *isolate, bindings_t *local);

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
    void FireWrapper (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterMessageData : public MilterEvent
{
  public:
    MilterMessageData (envelope_t *env, const unsigned char *buf, const size_t len);
    void FireWrapper (Isolate *isolate, bindings_t *local);

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
    void FireWrapper (Isolate *isolate, bindings_t *local);

    bool IsEndMessage () const;
};


/**
 */
class MilterAbort : public MilterEvent
{
  public:
    MilterAbort (envelope_t *env);
    void FireWrapper (Isolate *isolate, bindings_t *local);
};


/**
 */
class MilterClose : public MilterEvent
{
  public:
    MilterClose (envelope_t *env);
    void FireWrapper (Isolate *isolate, bindings_t *local);
  protected:
    void Postfire (Isolate *isolate);
};


#endif
