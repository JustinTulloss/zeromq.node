**This is alpha quality. Proceed with caution**
=======================

This library allows you to access zeromq from node.js.

Dependencies
============

 * [node.js]. Should work with 2.0 or newer.
 * [zeromq]. Build and install from the master branch. (2.0.9 is inadequate.)

To Build
========

     $ node-waf configure build

API
===

The API contains elements of the [zeromq API][zmq-api]. You should refer to it
for in depth detail of the expected behaviors of the system. These methods will
never return error codes, but may throw an exception if any of the errors
described in the zeromq documentation occur.

First, include the module:

    zeromq = require('zeromq');

After that, you can create sockets with:

    socket = zeromq.createSocket('req');

zeromq.Socket
-------------
A socket is where the action happens. You can send and receive things and it is
oh such fun.

#### Constructor - `function(type)`
 * type - A string describing the type of socket. You can read about the
   different socket types [here][zmq-socket]. The name you use here matches the
   names of the `ZMQ_*` constants, sans the `ZMQ_` prefix.

#### Methods
 * connect(address) - Connect to another socket. `address` should be a string
   as described in the [zeromq api docs][zmq-connect]. This method is not
   asynchronous because it is non-blocking. ZeroMQ will use the provided
   address when it's necessary and will not block here.
 * bind(address, callback) - Bind to a socket to wait for incoming data.
   `address` should be a string as described in the [zeromq api docs][zmq-bind].
   `callback` will be called when binding is complete and takes one argument, 
   a string describing any errors.
 * send(message) - `message` is a string to send across the wire. The message is
   not sent immediately, but there is no callback indicating when it has been
   transmitted. Have your server ack or something if you care that much.

   The message must be either a String or a Buffer object, or bad things will
   happen.

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
    $ vows tests/*

Licensing
=========

Licensed under the very permissive [MIT License][mit-license]

[node.js]: http://nodejs.org/
[zeromq]: http://github.com/zeromq/zeromq2
[zmq-api]: http://api.zeromq.org/
[zmq-socket]: http://api.zeromq.org/zmq_socket.html
[zmq-connect]: http://api.zeromq.org/zmq_connect.html
[zmq-bind]: http://api.zeromq.org/zmq_bind.html
[mit-license]: http://www.opensource.org/licenses/mit-license.php
