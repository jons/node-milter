/**
 *
 *
 *
 */

#include <assert.h>
#include <node.h>
#include <v8.h>
#include "envelope.h"

using namespace v8;
using namespace node;


Persistent<Function> Envelope::constructor;


/**
 */
Envelope::Envelope ()
{ }

/**
 */
Envelope::~Envelope ()
{ }


/**
 */
void Envelope::Init (Handle<Object> target)
{
  Isolate *isolate = Isolate::GetCurrent();
  Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate, New);
  tmpl->SetClassName(String::NewFromUtf8(isolate, "Envelope"));
  tmpl->InstanceTemplate()->SetInternalFieldCount(0);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "done", Done);

  constructor.Reset(isolate, tmpl->GetFunction());
}


/**
 */
Local<Object> Envelope::NewInstance (Isolate *isolate, HandleScope &scope)
{
  Handle<Value> argv[0] = { };
  Local<Function> cons = Local<Function>::New(isolate, constructor);
  Local<Object> envelope = cons->NewInstance(0, argv);
  //envelope->Ref();
  return envelope;
}


/**
 */
void Envelope::New (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);

  if (args.IsConstructCall())
  {
    Envelope *envelope = new Envelope();
    envelope->Wrap(args.This());
    args.GetReturnValue().Set(args.This());
  }
  else
  {
    const unsigned int argc = 0;
    Local<Value> argv[argc] = { };
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    args.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}


/**
 */
void Envelope::Done (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);

  if (args.Length() < 1)
  {
    // TODO: throw fatal exception
    return;
  }

  if (!args[0]->IsNumber())
  {
    // TODO: throw fatal exception
    return;
  }

  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  assert (envelope != NULL);
  assert (envelope->current_event != NULL);

  envelope->current_event->Done(isolate, args[0]->ToNumber()->IntegerValue());
}


/**
 */
void Envelope::SetCurrentEvent (MilterEvent *event)
{
  assert (event != NULL);
  this->current_event = event;
}
