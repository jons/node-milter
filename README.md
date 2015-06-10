# node-milter
node.js bindings for postfix milters

this is an attempt to turn a node.js process into a milter, so that you can
write all of your filtering callbacks in node.js instead of in C.

the milter will register with the name "node-bindings", not "node-milter".

if you know node, and you know libmilter, then you know their programming
models and/or architecture are fundamentally at odds, and that if this module or
anything like it works at all, then it's probably a hideous and disgusting mess.

:)

ANOMALIES

sources implicitly depend on pthreads: you can't use this on any non-POSIX
platforms yet. but then again, you can't use milters there, either. this cannot
be worked around by switching the relevant code to libuv unless libmilter is
simply already done, and with pthreads. it would have to be rewritten.
