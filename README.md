This library gives you bindings to ØMQ from node.js. This is not terribly
well tested, but there is at least one company successfully using these bindings
in production. Bug reports welcome.

To Install
==========

First, get [ØMQ 2.1], [Homebrew] on Mac will get you what you need.
Debian/Ubuntu users may also need to install the `libev-dev` package.

Then use [npm] to install zeromq.node:

    $ npm install zmq

`npm` will yell at you if you don't have node 0.3.0, as that is required.

API
===

The API contains elements of the [ØMQ API]. You should refer to it
for in depth detail of the expected behaviors of the system. These methods will
never return error codes, but may throw an exception if any of the errors
described in the ØMQ documentation occur.

First, include the module:

```js
var zmq = require('zmq');
```

After that, you can create sockets with:

```js
var sock = zmq.socket('req');
```

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
   it has been transmitted. See the
   [0MQ](http://www.zeromq.org/intro:read-the-manual) documentation for
   [zmq_send](http://api.zeromq.org/2-1:zmq-send) for exact
   transmission semantics. Raises an exception if the return is < 0.

   The message must be a `Buffer` object or a string. It is assumed that
   strings should be transmitted as UTF-8. If you provide more than one
   argument to send, then a multipart ØMQ message will be sent.

 * close() - Closes the socket

#### Socket Options

   To set a socket option on a socket, use socket[property].  For example,

   socket['identity'] = "mainLoop";

   The following properties are available (the ZMQ_XXX constant describes the name in the ZeroMQ documentation available at [ØMQ setsockopt API]):

   * ioThreadAffinity - set affinity for IO thread (integer, ZMQ_AFFINITY);
   * backlog - set connection backlog for listening sockets (integer, ZMQ_BACKLOG);
   * identity - set the socket identity (name) (string, ZMQ_IDENTITY);
   * lingerPeriod - set the linger period in milliseconds (integer, -1 = unlimited, ZMQ_LINGER);
   * receiveBufferSize - set the kernel receive buffer size (integer, 0 = OS default, ZMQ_RCVBUF);
   * sendBufferSize - set the kernel receive buffer size (integer, 0 = OS default, ZMQ_RCVBUF);

   The following apply to message buffering and reconnection:

   * reconnectionInterval - set the time to wait between reconnection attempts in milliseconds (ZeroMQ attempts to reconnect broken connection automatically behind the scenes) (integer, ZMQ_RECONNECT_IVL)
   * highWaterMark - set high water mark (in number of outstanding messages) before buffered messages start being dropped or swapped to disk (integer, zero = unlimited, ZMQ_HWM);
   * diskOffloadSize - set the amount of disk swap space in bytes for buffering messages in case of disconnection (integer, ZMQ_SWAP)

   The following options are applicable to multicast:

   * multicastLoop - set whether multicast can go over loopback or not (boolean, ZMQ_MCAST_LOOP);
   * multicastDataRate - set maximum multicast transmission rate in kbits per second (integer, ZMQ_RATE);
   * multicastRecovery - set maximum multicast recovery interval in seconds (integer, ZMQ_RECOVERY_IVL)

   The following properties are exposed but not normally used by client code (they are used internally by the library):

   * _fd - File descriptor (integer, ZMQ_FD);
   * _ioevents - Event loop used internally (ZMQ_EVENTS);
   * _receiveMore - Message has more parts (boolean, ZMQ_RCVMORE);
   * _subscribe - Subscribe to a channel (see subscribe() method) (string, ZMQ_SUBSCRIBE);
   * _unsubscribe - Unsubscribe to a channel (see unsubscribe() method) (string, ZMQ_UNSUBSCRIBE);


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

```
$ make
```

Testing
=======

```
$ make test
```

Licensing
=========

Licensed under the very permissive [MIT License].

[node.js]: http://github.com/ry/node
[npm]: https://github.com/isaacs/npm
[ØMQ 2.1]: http://www.zeromq.org/intro:get-the-software
[Homebrew]: http://mxcl.github.com/homebrew/
[ØMQ API]: http://api.zeromq.org/
[ØMQ setsockopt API]: http://api.zeromq.org/2-1-3:zmq-setsockopt
[zmq_socket]: http://api.zeromq.org/zmq_socket.html
[zmq_connect]: http://api.zeromq.org/zmq_connect.html
[zmq_bind]: http://api.zeromq.org/zmq_bind.html
[MIT license]: http://www.opensource.org/licenses/mit-license.php
