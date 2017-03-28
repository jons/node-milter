# node-milter
node.js bindings for postfix milters

this addon produces libmilter callbacks in node.js v7.7.4 so that you don't have
to be a C programmer to use postfix with libmilter.

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


QUICKSTART

creating a milter daemon.

    milter.start(
      string smtpd_milter_description,
      number flags,
      function(env, f0, f1, f2, f3) negotiate,
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


other controls are available when creating a milter.

completing a callback.

    connect = function (env, host, addr) {
      /* ... */
      env.done(decision);
    }


the allowed filter decisions for all callbacks except negotiate.
    
    milter.SMFIS_CONTINUE
    milter.SMFIS_REJECT
    milter.SMFIS_DISCARD
    milter.SMFIS_ACCEPT
    milter.SMFIS_TEMPFAIL
    milter.SMFIS_NOREPLY
    milter.SMFIS_SKIP


negotiate shall return one of these decisions instead. see env.negotiate() for
use of the CONTINUE return code.

    milter.SMFIS_ALL_OPTS
    milter.SMFIS_CONTINUE
    milter.SMFIS_REJECT


add a smtpd_milters line to your main.cf that matches your description in milter.start
and go to town. the test cases included in the project should show you how.


MILTER STUFF

other envelope methods.
access to message modifiers is allowed during the EOM event with these methods.

    env.addheader(name, value)
    env.chgheader(name, refcount, value)
    env.insheader(index, name, value)
    env.replacebody(newbody)
    env.addrcpt(recipient)
    env.addrcpt_par(recipient, args)
    env.delrcpt(recipient)
    env.chgfrom(envfrom, args)
    env.quarantine(reason)
    env.progress()


changing the symbol list is allowed during negotiate.

    env.setsymlist(stage, macrolist)


because pointers cannot be wrapped in node.js addons, an additional method in
the addon implementation that has no analog in libmilter exists to facilitate
changing these settings so that the pointers made available to the xxfi_negotiate
callback can be changed after node.js returns control to libmilter. call this
method BEFORE calling env.done(), also, the values changed by this function are
ignored unless env.done() is called with SMFIS_CONTINUE.

    env.negotiate(f0, f1, f2, f3)


retrieving a macro is allowed during any event. will return an empty string if
the macro doesn't have a value.

    env.getsymval(symname)


changing the server's smtp reply is allowed during any event other than connect.
i believe this should also exclude negotiate, so i enforce that as well.

    env.setreply(rcode, xcode, message)
    env.setmlreply(rcode, xcode, lines)


ANOMALIES

libmilter uses globals and is not thread-safe. you cannot use multiprocessing
features in node with this addon.


the milter will register in postfix with the name "node-bindings", you already
know it's a milter. the name "node-milter" is intended to give the project a
sensible npm identity and some github presence, too.

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

some calls probably don't work unless wrapped in setImmediate(), but i don't
know why yet.
