/**
 *
 */
#include <stdlib.h>
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
  tmpl->InstanceTemplate()->SetInternalFieldCount(2);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "done",      Done);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "negotiate", Negotiate);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "getsymval",   SMFI_GetSymbol);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "setsymlist",  SMFI_SetSymbolList);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "setreply",    SMFI_SetReply);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "setmlreply",  SMFI_SetMultilineReply);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "progress",    SMFI_Progress);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "quarantine",  SMFI_Quarantine);

  NODE_SET_PROTOTYPE_METHOD(tmpl, "addheader",   SMFI_AddHeader);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "chgheader",   SMFI_ChangeHeader);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "insheader",   SMFI_InsertHeader);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "replacebody", SMFI_ReplaceBody);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "addrcpt",     SMFI_AddRecipient);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "addrcpt_par", SMFI_AddRecipientExtended);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "delrcpt",     SMFI_DelRecipient);
  NODE_SET_PROTOTYPE_METHOD(tmpl, "chgfrom",     SMFI_ChangeFrom);

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
    Envelope *envelope = new Envelope;
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
void Envelope::SetCurrentEvent (MilterEvent *event)
{
  assert (event != NULL);
  this->current_event = event;
}

/**
 */
void Envelope::SetMilterContext (SMFICTX *context)
{
  assert (context != NULL);
  this->smfi_context = context;
}



/**
 */
void Envelope::Done (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Done unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope done method called outside event context")));
    return;
  }
  
  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsNumber())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected number")));
    return;
  }

  event->Done(isolate, args[0]->ToNumber()->IntegerValue());
}


/**
 */
void Envelope::Negotiate (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Negotiate unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;

  if (NULL == event || !event->IsNegotiate())
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope negotiate method called outside event context")));
    return;
  }


  if (args.Length() < 4)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  for (int i = 0; i < 4; i++)
    if (!args[i]->IsNumber())
    {
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid argument: expected number")));
      return;
    }

  unsigned long f0 = args[0]->ToNumber()->Uint32Value();
  unsigned long f1 = args[1]->ToNumber()->Uint32Value();
  unsigned long f2 = args[2]->ToNumber()->Uint32Value();
  unsigned long f3 = args[3]->ToNumber()->Uint32Value();

  MilterNegotiate *ev = (MilterNegotiate *)event;
  ev->Negotiate(f0, f1, f2, f3);
}


/**
 */
void Envelope::SMFI_GetSymbol (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_GetSymbol unwrap failed")));
    return;
  }

  Local<String> symname = args[0]->ToString();
  char *c_symname  = new char[symname->Utf8Length()+1];
  symname->WriteUtf8(c_symname);

  char *c_symval = smfi_getsymval(envelope->smfi_context, c_symname);
  delete [] c_symname;

  args.GetReturnValue().Set(String::NewFromUtf8(isolate, c_symval ? c_symval : ""));

  // TODO: either segfault or memory leak here, probably
  if (c_symval)
    free(c_symval);
}


/**
 */
void Envelope::SMFI_SetSymbolList (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Not yet implemented")));

  // TODO: smfi_setsymlist
}


/**
 */
void Envelope::SMFI_SetReply (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_SetReply unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called out of event context")));
    return;
  }
  if (event->IsNegotiate() || event->IsConnect())
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called from prohibited event context")));
    return;
  }


  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString() && !args[0]->IsNumber())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected number or string")));
    return;
  }

  if (args.Length() > 1)
  {
    if (!args[1]->IsString() && !args[1]->IsNull() && !args[1]->IsUndefined())
    {
      isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Second argument: expected string, null, or undefined")));
      return;
    }
    if (args.Length() > 2)
    {
      if (!args[2]->IsString() && !args[2]->IsNull() && !args[1]->IsUndefined())
      {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Third argument: expected string, null, or undefined")));
        return;
      }
    }
  }

  Local<String> rcode = args[0]->ToString();
  char *c_rcode = new char[rcode->Utf8Length()+1];
  rcode->WriteUtf8(c_rcode);

  char *c_xcode = NULL;
  char *c_message = NULL;

  if (args.Length() > 1 && args[1]->IsString())
  {
    Local<String> xcode   = args[1]->ToString();
    c_xcode = new char[xcode->Utf8Length()+1];
    xcode->WriteUtf8(c_xcode);
  }

  if (args.Length() > 2 && args[2]->IsString())
  {
    Local<String> message = args[2]->ToString();
    c_message = new char[message->Utf8Length()+1];
    message->WriteUtf8(c_message);
  }

  int r = smfi_setreply(envelope->smfi_context, c_rcode, c_xcode, c_message);

  delete [] c_rcode;
  if (c_xcode)
    delete [] c_xcode;
  if (c_message)
    delete [] c_message;

  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void Envelope::SMFI_SetMultilineReply (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Not yet implemented")));

  // TODO: implement with variable arguments somehow. smfi_setmlreply(rcode,xcode,vaargs)
}


/**
 */
void Envelope::SMFI_Progress (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_Progress unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called outside event context")));
    return;
  }

  if (!event->IsEndMessage())
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called from prohibited event context")));
    return;
  }
  int r = smfi_progress(envelope->smfi_context);
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void Envelope::SMFI_AddHeader (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_AddHeader unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called outside event context")));
    return;
  }
  if (!event->IsEndMessage())
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called from prohibited event context")));
    return;
  }

  if (args.Length() < 2)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected string")));
    return;
  }

  if (!args[1]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Second argument: expected string")));
    return;
  }

  Local<String> headerf = args[0]->ToString();
  Local<String> headerv = args[1]->ToString();
  char *c_name  = new char[headerf->Utf8Length()+1];
  char *c_value = new char[headerv->Utf8Length()+1];

  headerf->WriteUtf8(c_name);
  headerv->WriteUtf8(c_value);
  
  int r = smfi_addheader(envelope->smfi_context, c_name, c_value);

  delete [] c_name;
  delete [] c_value;

  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void Envelope::SMFI_ChangeHeader (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_chgheader(envelope->smfi_context, name, index, value)
}


/**
 */
void Envelope::SMFI_InsertHeader (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_insheader(envelope->smfi_context, index, name, value)
}


/**
 * who would ever do this? implement later when actually needed
 */
void Envelope::SMFI_ReplaceBody (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_replacebody
}


/**
 */
void Envelope::SMFI_AddRecipient (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_addrcpt
}


/**
 */
void Envelope::SMFI_AddRecipientExtended (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_addrcpt_par
}


/**
 */
void Envelope::SMFI_DelRecipient (const FunctionCallbackInfo<Value> &args)
{
  // TODO: smfi_delrcpt
}


/**
 */
void Envelope::SMFI_Quarantine (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_Quarantine unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called outside event context")));
    return;
  }
  if (!event->IsEndMessage())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Envelope method called from prohibited event context")));
    return;
  }


  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected string")));
    return;
  }

  Local<String> reason = args[0]->ToString();
  size_t len = reason->Utf8Length();
  if (len < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected non-empty string")));
    return;
  }

  char *c_reason = new char[len+1];
  reason->WriteUtf8(c_reason);
  int r = smfi_quarantine(envelope->smfi_context, c_reason);
  delete [] c_reason;

  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 */
void Envelope::SMFI_ChangeFrom (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope (isolate);
  Envelope *envelope = ObjectWrap::Unwrap<Envelope>(args.Holder());

  if (NULL == envelope)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "SMFI_ChangeFrom unwrap failed")));
    return;
  }
  MilterEvent *event = envelope->current_event;
  if (NULL == event)
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called outside event context")));
    return;
  }
  if (!event->IsEndMessage())
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Envelope method called from prohibited event context")));
    return;
  }

  if (args.Length() < 1)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "First argument: expected string")));
    return;
  }
  if (args.Length() > 1 && !args[1]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Second argument: expected string")));
    return;
  }


  Local<String> address = args[0]->ToString();
  char *c_address = new char[address->Utf8Length()+1];
  char *c_esmtp_args = NULL;
  address->WriteUtf8(c_address);
  if (args.Length() > 1)
  {
    Local<String> esmtp_args = args[1]->ToString();
    c_esmtp_args = new char[esmtp_args->Utf8Length()+1];
    esmtp_args->WriteUtf8(c_esmtp_args);
  }

  int r = smfi_chgfrom(envelope->smfi_context, c_address, c_esmtp_args);

  delete [] c_address;
  if (c_esmtp_args)
    delete [] c_esmtp_args;

  args.GetReturnValue().Set(Number::New(isolate, r));
}
