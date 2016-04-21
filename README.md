# zmq &nbsp;&nbsp;[![Build Status](https://travis-ci.org/JustinTulloss/zeromq.node.png)](https://travis-ci.org/JustinTulloss/zeromq.node) &nbsp;[![Build status](https://ci.appveyor.com/api/projects/status/n0h0sjs127eadfuo/branch/windowsbuild?svg=true)](https://ci.appveyor.com/project/reqshark/zeromq-node)

[ØMQ](http://www.zeromq.org/) bindings for node.js.

## Compatibility

This module maintains full compatibility with ZeroMQ 2, 3 and 4. We test rigorously on Linux and OS X. Windows support
is at the moment a best-effort situation, so volunteers, please raise your hands! We attempt to support a wide range
of Node.js versions (see the [Travis configuration](.travis.yml) for a list of supported versions).

## Installation

### on Windows:

First install [Visual Studio](https://www.visualstudio.com/) and [Node.js](https://nodejs.org/download/)

Ensure you're building ZeroMQ from a conservative location on disk, one without unusual characters or spaces, for
example somewhere like: `C:\sources\myproject`.

Installing the ZeroMQ library is optional and not required on Windows.

### on Unix/POSIX and OS X:

First install `pkg-config` and the [ZeroMQ library](http://www.zeromq.org/intro:get-the-software).

The installation process varies by platform, but headers are mandatory. Most Linux distributions provide these headers
with `-devel` packages like `zeromq-devel` or `zeromq3-devel`. Homebrew for OS X provides versions 3 and 4 with packages
`zeromq3` and `zeromq`, respectively. A [Chris Lea PPA](https://launchpad.net/~chris-lea/+archive/ubuntu/zeromq) is
available for Debian-like users who want a version newer than currently provided by their distribution.

Note: For zap support with versions >=4 you need to have libzmq built and linked against libsodium. Check the
[Travis configuration](.travis.yml) for a list of what is tested and therefore known to work.

#### with your platform-specifics taken care of, install and use this module:

    $ npm install zmq --save

## API

Before starting, please make sure you understand the fundamentals of ZeroMQ. The excellent
[ØMQ - The Guide](http://zguide.zeromq.org/page:all) by [Pieter Hintjens](https://github.com/hintjens) is often praised
as a benchmark on how to write great documentation. Please do go through a good part of it before undertaking anything
with ZeroMQ.

### Constants

ZeroMQ is full of constants for various that are [well documented](http://api.zeromq.org/). It would be overkill to
document them all here again, so please refer to the official documentation. Whenever you use a constant in node-zmq,
you may refer to it as a string that is either the full constant name (eg: `'ZMQ_MAX_SOCKETS'`), or by a short lowercase
version without the `ZMQ_` prefix (eg: `'max_sockets'`).

### Importing this module

```js
var zmq = require('zmq');

console.log('Using libzmq version:', zmq.version);
```

### Feature support

To see if a socket type is supported by this version of ZeroMQ:

```js
var type = 'sub';
var isSupported = zmq.socketTypeExists(type); // returns a boolean
```

To see if a socket option is supported by this version of ZeroMQ:

```js
var option = 'ZMQ_ROUTER_MANDATORY';
var isSupported = zmq.socketOptionExists(option); // returns a boolean
```

To see if a context option is supported by this version of ZeroMQ:

```js
var option = 'ZMQ_IO_THREADS';
var isSupported = zmq.contextOptionExists(option); // returns a boolean
```

### Contexts

**Creating a context**

```js
var options = {
  ZMQ_IO_THREADS: 1,
  ZMQ_MAX_SOCKETS: 1024,
  /* etc */
};

var ctx = zmq.context(options);
```

Creating a custom context is optional, and you can ignore this if you want to use default settings. But if you want to
create your sockets on a custom context, you can use this API.

You may pass along an options object as illustrated above. Every option, and the object itself are completely optional.
For more about the options and their availability, check the [zmq_ctx_set documentation](http://api.zeromq.org).

**Getting the default context**

```js
var ctx = zmq.getDefaultContext(); // always returns the same instance
```

**Setting and getting options**

You can set the options one by one, and read their current values:

```js
ctx.set('ZMQ_MAX_SOCKETS', 1024);
var maxSockets = ctx.get('max_sockets'); // 1024
```

**Creating a socket**

You can create a socket from a context:

```js
var type = 'pub'; // or 'ZMQ_PUB'
var options = {
  ZMQ_SNDHWM: 1000
};

var sock = ctx.socket(type, options);
```

You may pass along an options object as illustrated above. Every option, and the object itself are completely optional.
For more about the options and their availability, check the [zmq_setsockopt documentation](http://api.zeromq.org).

### Sockets

**Creating a socket using the default context**

You have now seen that you can create a socket from a context. If you're satisfied with using the default context, you
can create a socket more easily:

```js
var type = 'pub'; // or 'ZMQ_PUB'
var options = {
  ZMQ_SNDHWM: 1000
};

var sock = zmq.socket(type, options);
```

You may pass along an options object as illustrated above. Every option, and the object itself are completely optional.
For more about the options and their availability, check the [zmq_setsockopt documentation](http://api.zeromq.org).

**Setting and getting options**

You can set the options one by one, and read their current values:

```js
sock.set('ZMQ_SNDHWM', 10);
var sndhwm = sock.get('sndhwm'); // returns 10
```

**Binding and unbinding**

You can bind sockets asynchronously (advisable):

```js
var address = 'tcp://0.0.0.0:12345';

sock.bind(address, function (error) {
  // if there was no error, we are now accepting connections
});

sock.unbind(address, function (error) {
  // if there was no error, we are no longer accepting connections
});
```

Or synchronously:

```js
var address = 'tcp://0.0.0.0:12345';

sock.bindSync(address);

// if no error was thrown, we are now accepting connections

sock.unbindSync(address);

// if no error was thrown, we are no longer accepting connections
```

**Connecting and disconnecting**

```js
var address = 'tcp://1.2.3.4:12345';

sock.connect(address);
sock.disconnect(address);
```

**Sending messages**

The `sock.send(message, flags, callback)` method takes 3 arguments:

* message: a Buffer, string, or an array of string/Buffer.
* flags: a string or array of strings (optional).
* callback: called once the message is in the send-queue (optional).

```js
sock.send(['hello', 'world'], function (error) {
  // if no error was returned, the multi-part message is now in the send-queue
});

sock.send('hello', function (error) {
  // if no error was returned, the single-part message is now in the send-queue
});

sock.send('hello', ['ZMQ_SNDMORE']);
sock.send('world');
```

**Receiving messages**

Sockets will emit a `"message"` event when messages come in.

```js
sock.on('message', function (part1, part2, etc) {
  // all parts are individual arguments
});
```

When a socket is paused (see the API below), events will not be emitted, but you can still extract messages from the
queue by calling:

```js
var parts = sock.read(); // returns an array of parts
```

**Pausing a socket**

You can pause a socket's incoming message flow.

```js
sock.pause();
// no "message" events will be emitted until sock.resume() is called
sock.resume();
```

**Detaching from the event loop**

You may temporarily disable polling on a socket and let the Node.js process terminate without closing sockets explicitly
by removing their event loop references. Newly created sockets are already `ref()`-ed.

```js
// Detach the socket from the main event loop of the node.js runtime.
// Calling this on already detached sockets is a no-op.

sock.unref();

// Attach the socket to the main event loop.
// Calling this on already attached sockets is a no-op.

sock.ref();
```

**Monitoring a socket**

You can get socket state changes events by calling the `monitor` method on a socket. The supported events are (see ZMQ
[docs](http://api.zeromq.org/4-2:zmq-socket-monitor) for full description):

* connect - `ZMQ_EVENT_CONNECTED`
* connect_delay - `ZMQ_EVENT_CONNECT_DELAYED`
* connect_retry - `ZMQ_EVENT_CONNECT_RETRIED`
* listen - `ZMQ_EVENT_LISTENING`
* bind_error - `ZMQ_EVENT_BIND_FAILED`
* accept - `ZMQ_EVENT_ACCEPTED`
* accept_error - `ZMQ_EVENT_ACCEPT_FAILED`
* close - `ZMQ_EVENT_CLOSED`
* close_error - `ZMQ_EVENT_CLOSE_FAILED`
* disconnect - `ZMQ_EVENT_DISCONNECTED`

All events get 2 arguments:

* `fd` - The file descriptor of the underlying socket (if available)
* `endpoint` - The underlying socket endpoint

A special `monitor_error` event will be raised when there was an error in the monitoring process, after this event no more
monitoring events will be sent, you can try and call `monitor` again to restart the monitoring process.

```js
// Create an inproc PAIR socket where ZeroMQ will publish socket state changes events, the events from this socket will
// be read every `interval` (defaults to 10ms). By default only 1 message will be read every interval. This can be
// configured by using the `numOfEvents` parameter, where passing 0 will read all available messages per interval.

var interval = 10;
var numOfEvents = 0;

sock.monitor(interval, numOfEvents);

// Stop monitoring

sock.unmonitor();
```

A full example:

```js
// Create a socket
var zmq = require('zmq');
var socket = zmq.socket('req');

// Register to monitoring events
socket.on('connect', function (fd, ep) { console.log('connect, endpoint:', ep); });
socket.on('connect_delay', function (fd, ep) { console.log('connect_delay, endpoint:', ep); });
socket.on('connect_retry', function (fd, ep) { console.log('connect_retry, endpoint:', ep); });
socket.on('listen', function (fd, ep) { console.log('listen, endpoint:', ep); });
socket.on('bind_error', function (fd, ep) { console.log('bind_error, endpoint:', ep); });
socket.on('accept', function (fd, ep) { console.log('accept, endpoint:', ep); });
socket.on('accept_error', function (fd, ep) { console.log('accept_error, endpoint:', ep); });
socket.on('close', function (fd, ep) { console.log('close, endpoint:', ep); });
socket.on('close_error', function (fd, ep) { console.log('close_error, endpoint:', ep); });
socket.on('disconnect', function (fd, ep) { console.log('disconnect, endpoint:', ep); });

// Handle monitor errors
socket.on('monitor_error', function(err) {
	console.log('Error in monitoring: %s, will restart monitoring in 5 seconds', err);
	setTimeout(function() { socket.monitor(500, 0); }, 5000);
});

// Call monitor, check for events every 500ms and get all available events.
console.log('Start monitoring...');
socket.monitor(500, 0);
socket.connect('tcp://127.0.0.1:1234');

setTimeout(function() {
	socket.unmonitor();
}, 20000);
```

**Helpers for "sub" sockets**

The way to subscribe to messages from a publisher is a bit archaic by having to go through socket options instead of
being able to call a function, which would feel more natural. For that reason there are the following helper functions:

```js
sock.subscribe('foo');
sock.unsubscribe('foo');
```

Please note that this is equal to:

```js
sock.set('ZMQ_SUBSCRIBE', 'foo');
sock.set('ZMQ_UNSUBSCRIBE', 'foo');
```

**Destroying a socket**

You can destroy a socket by calling its `close()` method.

```js
sock.close();
```

## Usage examples

### Push/Pull

```js
// producer.js
var zmq = require('zmq');
var sock = zmq.socket('push');

sock.bindSync('tcp://127.0.0.1:3000');
console.log('Producer bound to port 3000');

setInterval(function(){
  console.log('sending work');
  sock.send('some work');
}, 500);
```

```js
// worker.js
var zmq = require('zmq');
var sock = zmq.socket('pull');

sock.connect('tcp://127.0.0.1:3000');
console.log('Worker connected to port 3000');

sock.on('message', function (msg) {
  console.log('work: %s', msg.toString());
});
```

### Pub/Sub

```js
// publisher.js
var zmq = require('zmq');
var sock = zmq.socket('pub');

sock.bindSync('tcp://127.0.0.1:3000');
console.log('Publisher bound to port 3000');

setInterval(function () {
  console.log('sending a multipart message envelope');
  sock.send(['kitty cats', 'meow!']);
}, 500);
```

```js
// subscriber.js
var zmq = require('zmq');
var sock = zmq.socket('sub');

sock.connect('tcp://127.0.0.1:3000');
sock.subscribe('kitty cats');
console.log('Subscriber connected to port 3000');

sock.on('message', function (topic, message) {
  console.log('received a message related to:', topic, 'containing message:', message);
});
```

## Running benchmarks

Benchmarks are available in the `perf` directory, and have been implemented according to the ZeroMQ documentation:
[How to run performance tests](http://www.zeromq.org/results:perf-howto)

In the following examples, the arguments are respectively:
* the host to connect to/bind on
* message size (in bytes)
* message count

You can run a latency benchmark by running these two commands in two separate
shells:

```sh
node ./perf/local_lat.js tcp://127.0.0.1:5555 1 100000
```

```sh
node ./perf/remote_lat.js tcp://127.0.0.1:5555 1 100000
```

And you can run throughput tests by running these two commands in two
separate shells:

```sh
node ./perf/local_thr.js tcp://127.0.0.1:5555 1 100000
```

```sh
node ./perf/remote_thr.js tcp://127.0.0.1:5555 1 100000
```

Running `make perf` will run the commands listed above.
