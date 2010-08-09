**This isn't done yet**

This library allows you to access zeromq from node.js. It is meant
to be a pretty low level wrapping.

Dependencies
============

 * node.js. Should work with trunk [on github][node.js]
 * zeromq. Download and build [here][zmq]

To Build
========

     $ node-waf configure
     $ node-waf

API
===

The API contains elements of the [zeromq API][zmq-api]. You should refer to it
for in depth detail of the expected behaviors of the system.

First, include the module.

    zeromq = require('zeromq');

After that, the following objects are available

zeromq.Context
--------------
A context must be created before you can create a socket. Contexts are basically
logical groupings of sockets. Contexts are thread safe, but who cares? This is
node. You can read more about contexts [in the zeromq api docs][zmq-api].

### Constructor - `function()`
The constructor takes no arguments.

### Methods
 * close() - Closes the context.

zeromq.Socket
-------------
A socket is where the action happens. You can send and receive things and it is
oh such fun.

#### Constructor - `function(context, type)`
 * context - A `zeromq.Context` object that this socket is to be associated with.
 * type - The type of socket. See the ZMQ_* constants hung off of `zeromq.Socket`
   for options. You can read about the different socket types [here][zmq-socket].

#### Methods
 * connect(address) - `address` should be a string as described in the [zeromq
   api docs][zmq-connect].
 * send(message) - `message` is a string to send across the wire.
 * close() - Closes the socket

#### Events
 * receive - Data was received. The only argument is the received data.
 * error - There was some error. The only argument is an exception explaining
   what the error was.

[node.js]: http://github.com/ry/node
[zmq]: http://www.zeromq.org/local--files/area:download/zeromq-2.0.7.tar.gz
[zmq-api]: http://api.zeromq.org/
[zmq-socket]: http://api.zeromq.org/zmq_socket.html
[zmq-connect]: http://api.zeromq.org/zmq_connect.html
