# node-milter
node.js bindings for postfix milters

    milter.start(
      string smtpd_milter_description,
      function(env,host,addr) connect,
      function(env,command) unknown,
      function(env,identity) helo,
      function(env,address) mailfrom,
      function(env,address) rcptto,
      function(env) data,
      function(env,name,value) header,
      function(env) eoh,
      function(env,buffer,length) segment,
      function(env) eom,
      function(env) abort,
      function(env) close,
    );
    
    milter.SMFIS_CONTINUE
    milter.SMFIS_REJECT
    milter.SMFIS_DISCARD
    milter.SMFIS_ACCEPT
    milter.SMFIS_TEMPFAIL
    milter.SMFIS_NOREPLY
    milter.SMFIS_SKIP


this addon produces libmilter callbacks in node.js so that you don't have to be
a C programmer to use postfix with libmilter.

when its main function is called, libmilter creates a threaded daemon where each
mail session has one unique thread in your program servicing it. this main is
therefore sequestered away in a libuv worker to allow node.js to continue to
function normally.

your libmilter event callbacks are called from these threads with a context
pointer to distinguish them from each other. you must return a code at the end
of the event implementation, which tells postfix what to do next, continue,
reject, tempfail, etc.

this milter blocks itself during each of those events. it wraps the event into
a localized context object and queues that, signals to libuv that it's time
to do some node.js work, and goes to sleep until the pthread condition is met.
thus multiple libmilter sessions may block on a single stage of the event, but,
they are in their own threads so it's mostly ok.

when libuv runs the async worker to service the queue on the other side, we are
now back in node.js-land (a scope) and can make js callbacks, wrap event data in
js objects, and so on. but it is required that the js callbacks return a
decision for postfix immediately, so your js callbacks cannot be async (yet).


ANOMALIES

the milter will register in postfix with the name "node-bindings". the name
"node-milter" is intended to give the project a sensible npm identity.

sources implicitly depend on pthreads yet don't use their #include files by name.
  - libmilter explicitly uses pthreads, and libuv is implicitly using them. the
    code compiles because of this happy coincidence.
  - you can't use this on any non-POSIX platforms yet. but then again, you can't
    use milters there, either.
  - this cannot be worked around by switching the relevant code over to libuv
    without also rewriting all of libmilter. i looked. it's not worth it.

node.js buffers are created during the message data event (the client has already
send command "DATA", and now a chunk of the message data has arrived) using the
i-suspect-is-soon-to-be-deprecated method Buffer::Use(), which should have been
named Buffer::New() like the others, by passing (const unsigned char *) as the
expected (char *) which is probably stupid. it is unclear to me why there is no
Buffer::New() that simply accepts (void *) like all the real POSIX C buffer-
manipulating functions. whatever.
