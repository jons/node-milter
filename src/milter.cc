/**
 * node bindings for milter
 */

#include <node.h>
#include <v8.h>
#include <arpa/inet.h>

#include "libmilter/mfapi.h"

using namespace v8;


struct bindings
{
  Isolate *isolate;
  Function connect;
};


static const char *g_name = "node-bindings";

static int g_flags = SMFIF_QUARANTINE;



/**
 */
sfsistat fi_connect __P((SMFICTX *context, char *hostname, _SOCK_ADDR *sa))
{
  Isolate *isolate = Isolate::GetCurrent();
  struct bindings *local = (struct bindings *)smfi_getpriv(context);
  int r;

  fprintf(stderr, "connect %s\n", hostname);
  /*
  char s[INET6_ADDRSTRLEN+1];
  inet_ntop(AF_INET, sa, s, sizeof s);
  */

  const unsigned argc = 1;
  Local<Value> argv[argc] = {
    String::NewFromUtf8(isolate, "hello")
  };

  if (!local->connect->IsFunction())
  {
    //isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "milter_connect is not a function")));
    printf("tempfail 1\n");
    return SMFIS_TEMPFAIL;
  }

  Local<Value> rv = local->connect->Call(isolate->GetCurrentContext()->Global(), argc, argv);
  if (!rv->IsNumber())
  {
    //isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "milter_connect must return a number")));
    printf("tempfail 2\n");
    return SMFIS_TEMPFAIL;
  }
  r = rv->IntegerValue();
  printf("returning %d on connect\n", r);

  return SMFIS_CONTINUE;
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
void milter_main (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  struct bindings *local = new struct bindings;
  struct smfiDesc desc;
  int r;

  local->isolate = isolate;

  if (args.Length() < 2)
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "milter: wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "milter: expected string as first argument")));
    return;
  }

  if (!args[1]->IsObject())
  {
    isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "milter: expected object as second argument")));
    return;
  }

  Local<Object> cbmap = Local<Object>::Cast(args[1]);

  if (cbmap->Has(String::NewFromUtf8(isolate, "connect")))
  {
    Local<Function> connect = Local<Function>::Cast(cbmap->GetRealNamedProperty(String::NewFromUtf8(isolate, "connect")));

    Handle<String> name = connect->GetDisplayName()->ToString();

    printf("have function: %s\n", name->GetExternalAsciiStringResource()->data());

    local->connect = Persistent<Function>::New(isolate, connect);
  }

  desc.xxfi_name      =(char *)g_name;
  desc.xxfi_version   = SMFI_VERSION;
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
  r = smfi_register(desc);
  r = smfi_setconn((char *)"inet:12345");
  r = smfi_main(local);
  // this does not return until interrupt
  delete local;
  args.GetReturnValue().Set(Number::New(isolate, r));
}


/**
 * module initialization
 */
void init (Handle<Object> target)
{
  NODE_SET_METHOD(target, "main", milter_main);
}

NODE_MODULE(milter, init)
