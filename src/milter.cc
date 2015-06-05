/**
 * node bindings for milter
 */

#include <libmilter/mfapi.h>
#include <node.h>
#include <v8.h>
#include <arpa/inet.h>

using namespace v8;


struct bindings
{
  Local<String> fi_connect;
};

static char *g_name = "node-milter";
static int g_version = 1;
static int g_flags = 0;



/**
 */
sfsistat fi_connect __P((SMFICTX *context, char *hostname, _SOCK_ADDR *sa))
{
  bindings *local = (bindings *)smfi_getpriv(context);
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  char s[INET6_ADDRSTRLEN+1];
  inet_ntop(AF_INET, sa, s, sizeof s);

  Local<Function> cb = Local<Function>::Cast(local->fi_connect);
  const unsigned argc = 2;
  Local<Value> argv[argc] = {
    String::NewFromUtf8(isolate, hostname),
    String::NewFromUtf8(isolate, s)
  };
  Local<Value> r = cb->Call(isolate->GetCurrentContext()->Global(), argc, argv);
  if (!r->IsNumber())
  {
    // TODO: throw an exception in node.js
    return SMFIS_TEMPFAIL;
  }
  return r->IntegerValue();
}


sfsistat fi_negotiate __P((SMFICTX *context,
                           unsigned long a,
                           unsigned long b,
                           unsigned long c,
                           unsigned long d,
                           unsigned long *e,
                           unsigned long *f,
                           unsigned long *g,
                           unsigned long *h))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_unknown __P((SMFICTX *context, const char *command))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_helo __P((SMFICTX *context, char *helo))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_envfrom __P((SMFICTX *context, char **argv))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_envrcpt __P((SMFICTX *context, char **argv))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_data __P((SMFICTX *context))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_header __P((SMFICTX *context, char *name, char *value))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_eoh __P((SMFICTX *context))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_body __P((SMFICTX *context, unsigned char *body, size_t len))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_eom __P((SMFICTX *context))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_abort __P((SMFICTX *context))
{
  return SMFIS_CONTINUE;
}

sfsistat fi_close __P((SMFICTX *context))
{
  return SMFIS_CONTINUE;
}


/**
 */
void go (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  int r;
  struct smfiDesc desc;

  /*if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expected string argument")));
    return;
  }*/

  desc.xxfi_name      = g_name;
  desc.xxfi_version   = g_version;
  desc.xxfi_flags     = g_flags;

  desc.xxfi_connect   = fi_connect;
  desc.xxfi_unknown   = fi_unknown;
  desc.xxfi_helo      = fi_helo;
  desc.xxfi_envfrom   = fi_envfrom;
  desc.xxfi_envrcpt   = fi_envrcpt;
  desc.xxfi_data      = fi_data;
  desc.xxfi_header    = fi_header;
  desc.xxfi_eoh       = fi_eoh;
  desc.xxfi_body      = fi_body;
  desc.xxfi_eom       = fi_eom;
  desc.xxfi_abort     = fi_abort;
  desc.xxfi_close     = fi_close;
  desc.xxfi_negotiate = fi_negotiate;
#if 0
  // signal handler is not implemented yet
  desc.xxfi_signal    = fi_signal;
#endif

  if (MI_SUCCESS != (r = smfi_register(desc)))
  {
    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Callback Registration Failed")));
    return;
  }

  r = smfi_main();
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 * module initialization
 */
void init(Handle<Object> target)
{
  NODE_SET_METHOD(target, "main", go);
}

NODE_MODULE(milter, init)
