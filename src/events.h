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
    virtual void Fire (Isolate *isolate, Handle<Object> &context) = 0;

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
     * must be called in trigger_event on connect (or negotiate. hmm)
     */
    Local<Value> CreateEnvelope (Isolate *isolate);

    /**
     * shared wrapper for Fire() implementors
     * all events are triggered in the same manner
     */
    Handle<Value> EventWrap (Isolate *isolate, Handle<Object> &context, const char *ev_name, unsigned int argc, Local<Value> *argv);


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
    void Fire (Isolate *isolate, Handle<Object> &context);

    const char *Host() const;
    const char *Address() const;

  private:
    const char *sz_host;
    char sz_addr[INET6_ADDRSTRLEN+1];
};


#endif
