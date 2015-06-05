# node-milter
node.js bindings for postfix milters

this is an attempt to turn a node.js process into a milter, so that you can
write all of your filtering callbacks in node.js instead of in C.

the milter will register with the name "node-bindings", not "node-milter".

if you know node, and you know libmilter, then you know their programming
models and/or architecture are fundamentally at odds, and that if this module or
anything like it works at all, then it's probably a hideous and disgusting mess.

:)
