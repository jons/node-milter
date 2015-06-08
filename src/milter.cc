/**
 * node bindings for milter
 */

#include <node/uv.h>
#include <node.h>
#include <v8.h>
#include <arpa/inet.h>

#include "libmilter/mfapi.h"

using namespace v8;
using namespace node;

typedef struct privdata privdata_t;
typedef struct bindings bindings_t;

struct privdata
{
  int id;
};


struct bindings
{
  uv_work_t request;
  privdata_t *_priv;
  int retval;
};


static const char *g_name = "node-bindings";

static int g_flags = SMFIF_QUARANTINE;



/**
 */
sfsistat fi_connect (SMFICTX *context, char *host, _SOCK_ADDR *sa)
{
  privdata_t *priv = (privdata_t *)smfi_getpriv(context);
  char s[INET6_ADDRSTRLEN+1];

  inet_ntop(AF_INET, sa, s, sizeof s);
  priv->id = 0;

  fprintf(stderr, "connect \"%s\" \"%s\"\n", host, s);
  return SMFIS_CONTINUE;
}


sfsistat fi_negotiate (SMFICTX *context,
                           unsigned long a,
                           unsigned long b,
                           unsigned long c,
                           unsigned long d,
                           unsigned long *e,
                           unsigned long *f,
                           unsigned long *g,
                           unsigned long *h)
{
  fprintf(stderr, "negotiate %lu %lu %lu %lu\n", a, b, c, d);
  return SMFIS_CONTINUE;
}

sfsistat fi_unknown (SMFICTX *context, const char *command)
{
  fprintf(stderr, "unknown \"%s\"\n", command);
  return SMFIS_CONTINUE;
}

sfsistat fi_helo (SMFICTX *context, char *helo)
{
  fprintf(stderr, "helo \"%s\"\n", helo);
  return SMFIS_CONTINUE;
}

sfsistat fi_envfrom (SMFICTX *context, char **argv)
{
  fprintf(stderr, "envfrom \"%s\"\n", argv[0]);
  return SMFIS_CONTINUE;
}

sfsistat fi_envrcpt (SMFICTX *context, char **argv)
{
  fprintf(stderr, "envrcpt \"%s\"\n", argv[0]);
  return SMFIS_CONTINUE;
}

sfsistat fi_data (SMFICTX *context)
{
  fprintf(stderr, "data\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_header (SMFICTX *context, char *name, char *value)
{
  fprintf(stderr, "header\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_eoh (SMFICTX *context)
{
  fprintf(stderr, "eoh\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_body (SMFICTX *context, unsigned char *body, size_t len)
{
  fprintf(stderr, "body\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_eom (SMFICTX *context)
{
  fprintf(stderr, "eom\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_abort (SMFICTX *context)
{
  fprintf(stderr, "abort\n");
  return SMFIS_CONTINUE;
}

sfsistat fi_close (SMFICTX *context)
{
  fprintf(stderr, "close\n");
  return SMFIS_CONTINUE;
}

/**
 */
void milter_worker (uv_work_t *request)
{
  bindings_t *local = static_cast<bindings_t *>(request->data);
  local->retval = smfi_main(local->_priv);
}


void milter_cleanup (uv_work_t *request, int status)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  bindings_t *local = static_cast<bindings_t *>(request->data);

  // TODO: do something with local->retval ?

  delete local->_priv;
  delete local;
}


/**
 */
void milter_start (const FunctionCallbackInfo<Value> &args)
{
  Isolate *isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  bindings_t *local = new bindings_t();
  struct smfiDesc desc;
  int r;

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

  local->_priv = new privdata_t();
  local->request.data = local;

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

  Local<String> connstr = args[0]->ToString();
  char *c_connstr = new char[connstr->Utf8Length()+1];
  connstr->WriteUtf8(c_connstr);
  bool ok = false;

  r = smfi_register(desc);
  if (MI_SUCCESS == r)
  {
    r = smfi_setconn(c_connstr);
    if (MI_SUCCESS == r)
    {
      uv_queue_work(uv_default_loop(), &local->request, milter_worker, milter_cleanup);
      ok = true;
    }
  }
  delete c_connstr;
  if (ok)
    args.GetReturnValue().Set(Number::New(isolate, ok ? 0 : -1));

  // TODO: if not ok, send error number and description somewhere
  //       or put them in an object and return it, or throw an exception
}


/**
 * module initialization
 */
void init (Handle<Object> target)
{
  NODE_SET_METHOD(target, "start", milter_start);
}

NODE_MODULE(milter, init)
