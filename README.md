**This is alpha quality. Proceed with caution**
=======================

This library gives you experimental bindings to ØMQ from node.js.

Dependencies
============

Some bleeding edge dependencies are required:

 * [node.js]. Should work with at least 0.3.
 * [ØMQ]. Should work with at least 2.1.

At the time of writing, ØMQ 2.1 has not been released, so you'll have
to build it from its github repositories, while node.js 0.3 has been
released but is still officially "unstable."

To Build
========

     $ node-waf configure build

API
===

The API contains elements of the [ØMQ API]. You should refer to it
for in depth detail of the expected behaviors of the system. These methods will
never return error codes, but may throw an exception if any of the errors
described in the ØMQ documentation occur.

First, include the module:

    zeromq = require('zeromq');

After that, you can create sockets with:

    socket = zeromq.createSocket('req');

Using ØMQ sockets
-------------
A socket is where the action happens. You can send and receive things and it is
oh such fun.

#### Constructor - `function(type)`
 * type - A string describing the type of socket. You can read about the
   different socket types [here][zmq_socket]. The name you use here matches the
   names of the `ZMQ_*` constants, sans the `ZMQ_` prefix.

#### Methods
 * connect(address) - Connect to another socket. `address` should be a string
   as described in the [ØMQ API docs][zmq_connect]. This method is not
   asynchronous because it is non-blocking. ØMQ will use the provided address
   when it's necessary and will not block here.
 * bind(address, callback) - Bind to a socket to wait for incoming data.
   `address` should be a string as described in the [ØMQ API docs][zmq_bind].
   `callback` will be called when binding is complete and takes one argument, 
   a string describing any errors.
 * send(message, ...) - `message` is a string to send across the wire. The
   message is not sent immediately, but there is no callback indicating when
   it has been transmitted. Have your server ack or something if you care that
   much.

   The message must be either a String or a Buffer object, or bad things will
   happen. If you provide more than one argument to send, then a multipart
   ØMQ message will be sent.

 * close() - Closes the socket

#### Events
 * message - A message was received. The only argument is the message.
 * error - There was some error. The only argument is an exception explaining
   what the error was.

Testing
=======

Tests are pretty incomplete right now, but to run what's there:

    $ node examples/simple-server.js
    # in another terminal:
    $ npm install vows
    $ vows

Licensing
=========

Licensed under the very permissive [MIT License].

[node.js]: http://github.com/ry/node
[ØMQ]: http://github.com/zeromq/zeromq2
[ØMQ API]: http://api.zeromq.org/
[zmq_socket]: http://api.zeromq.org/zmq_socket.html
[zmq_connect]: http://api.zeromq.org/zmq_connect.html
[zmq_bind]: http://api.zeromq.org/zmq_bind.html
[MIT license]: http://www.opensource.org/licenses/mit-license.php
