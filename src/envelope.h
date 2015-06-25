/**
 * mail session envelope
 *
 */


#ifndef MILTER_ENVELOPE_H
#define MILTER_ENVELOPE_H

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include "forward.h"
#include "events.h"

using namespace v8;
using namespace node;


/**
 */
class Envelope : public ObjectWrap
{
  public:
    static void Init (Handle<Object> target);

    static Local<Object> NewInstance (Isolate *isolate, HandleScope &scope);

    void SetCurrentEvent (MilterEvent *event);

  private:
    explicit Envelope ();
    ~Envelope ();

    static void New (const FunctionCallbackInfo<Value> &args);
    static void Done (const FunctionCallbackInfo<Value> &args);

    static Persistent<Function> constructor;

    bindings_t *local;

    MilterEvent *current_event;
};

#endif
