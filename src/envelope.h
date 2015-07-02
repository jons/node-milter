/**
 * mail session envelope
 *
 */

#ifndef MILTER_ENVELOPE_H
#define MILTER_ENVELOPE_H

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include "libmilter/mfapi.h"
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
    void SetMilterContext (SMFICTX *context);

  private:
    explicit Envelope ();
    ~Envelope ();

    static Persistent<Function> constructor;

    static void New (const FunctionCallbackInfo<Value> &args);

    /**
     * the completion callback provided to each event callback, via the latter's
     * first argument. all js funcs have the signature:
     *   function eventname (envelope, ...)
     *
     * and all of them continue a libmilter thread by calling
     *   envelope.done(retval);
     *
     * where retval is an SMFIS_* filter status code.
     */
    static void Done (const FunctionCallbackInfo<Value> &args);

    /**
     * during negotiate, just before continuation, the implementor needs to let
     * us know what to store in the pointers to config values f0-f3.
     */
    static void Negotiate (const FunctionCallbackInfo<Value> &args);

    /**
     * allowed whenever.
     */
    static void SMFI_GetSymbol (const FunctionCallbackInfo<Value> &args);

    /**
     * allowed during negotiate.
     */
    static void SMFI_SetSymbolList (const FunctionCallbackInfo<Value> &args);

    /**
     * allowed in events other than connect (and negotiate, i suspect.)
     */
    static void SMFI_SetReply (const FunctionCallbackInfo<Value> &args);
    static void SMFI_SetMultilineReply (const FunctionCallbackInfo<Value> &args);

    /**
     * these modifiers are allowed during EOM.
     */
    static void SMFI_Progress (const FunctionCallbackInfo<Value> &args);
    static void SMFI_Quarantine (const FunctionCallbackInfo<Value> &args);

    static void SMFI_AddHeader (const FunctionCallbackInfo<Value> &args);
    static void SMFI_ChangeHeader (const FunctionCallbackInfo<Value> &args);
    static void SMFI_InsertHeader (const FunctionCallbackInfo<Value> &args);
    static void SMFI_ReplaceBody (const FunctionCallbackInfo<Value> &args);
    static void SMFI_AddRecipient (const FunctionCallbackInfo<Value> &args);
    static void SMFI_AddRecipientExtended (const FunctionCallbackInfo<Value> &args);
    static void SMFI_DelRecipient (const FunctionCallbackInfo<Value> &args);
    static void SMFI_ChangeFrom (const FunctionCallbackInfo<Value> &args);

    MilterEvent *current_event;
    SMFICTX *smfi_context;
};

#endif
