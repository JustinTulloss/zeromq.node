
This library gives you bindings to ØMQ from node.js. This is not terribly
well tested, but there is at least one company successfully using these bindings
in production. Bug reports welcome.

To Install
==========

First, get [ØMQ 2.1], >2.1.3 is required. [Homebrew] on Mac will get you what
you need.

Then use [npm] to install zeromq.node:

    $ npm install zeromq

`npm` will yell at you if you don't have node 0.3.0, as that is required.

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
-----------------
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
   which may be an `Error`, or simply `undefined` if everything's peachy.

 * send(message, ...) - `message` is a string to send across the wire. The
   message is not sent immediately, but there is no callback indicating when
   it has been transmitted. Have your server ack or something if you care that
   much.

   The message must be a `Buffer` object or a string. It is assumed that
   strings should be transmitted as UTF-8. If you provide more than one
   argument to send, then a multipart ØMQ message will be sent.

 * close() - Closes the socket

#### Events

 * message - A message was received. The arguments are the parts of the
   message. So, for example, if you have an `xrep` socket with plain `req`
   sockets on the other end, you can do something like:

       socket.on('message', function(envelope, blank, data) {
         socket.send(envelope, blank, compute_reply_for(data));
       });

 * error - There was some error. The only argument is an `Error` object
   explaining what the error was.


To Build
========

     $ node-waf configure build

Testing
=======

Tests are pretty incomplete right now, but to run what's there:

    $ npm install vows
    $ vows

Licensing
=========

Licensed under the very permissive [MIT License].

[node.js]: http://github.com/ry/node
[npm]: https://github.com/isaacs/npm
[ØMQ 2.1]: http://www.zeromq.org/intro:get-the-software
[Homebrew]: http://mxcl.github.com/homebrew/
[ØMQ API]: http://api.zeromq.org/
[zmq_socket]: http://api.zeromq.org/zmq_socket.html
[zmq_connect]: http://api.zeromq.org/zmq_connect.html
[zmq_bind]: http://api.zeromq.org/zmq_bind.html
[MIT license]: http://www.opensource.org/licenses/mit-license.php
