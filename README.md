# zmq &nbsp;&nbsp;[![Build Status](https://travis-ci.org/JustinTulloss/zeromq.node.png)](https://travis-ci.org/JustinTulloss/zeromq.node) &nbsp;[![Build status](https://ci.appveyor.com/api/projects/status/n0h0sjs127eadfuo/branch/windowsbuild?svg=true)](https://ci.appveyor.com/project/reqshark/zeromq-node)

[ØMQ](http://www.zeromq.org/) bindings for node.js.

## Installation

### on Windows:
First install [Visual Studio](https://www.visualstudio.com/) and either
[Node.js](https://nodejs.org/download/) or [io.js](https://iojs.org/dist/latest/).

Ensure you're building zmq from a conservative location on disk, one without
unusual characters or spaces, for example somewhere like: `C:\sources\myproject`.

Installing the ZeroMQ library is optional and not required on Windows. We
recommend running `npm install` and node executable commands from a
[github for windows](https://windows.github.com/) shell or similar environment.

### installing on Unix/POSIX (and osx):

First install `pkg-config` and the [ZeroMQ library](http://www.zeromq.org/intro:get-the-software).

This module is compatible with ZeroMQ versions 2, 3 and 4. The installation
process varies by platform, but headers are mandatory. Most Linux distributions
provide these headers with `-devel` packages like `zeromq-devel` or
`zeromq3-devel`. Homebrew for OS X provides versions 4 and 3 with packages
`zeromq` and `zeromq3`, respectively. A
[Chris Lea PPA](https://launchpad.net/~chris-lea/+archive/ubuntu/zeromq)
is available for Debian-like users who want a version newer than currently
provided by their distribution. Windows is supported but not actively
maintained.

Note: For zap support with versions >=4 you need to have libzmq built and linked
against libsodium. Check the [Travis configuration](.travis.yml) for a list of what is tested
and therefore known to work.

#### with your platform-specifics taken care of, install and use this module:

    $ npm install zmq

## Examples

### Push/Pull

```js
// producer.js
var zmq = require('zmq')
  , sock = zmq.socket('push');

sock.bindSync('tcp://127.0.0.1:3000');
console.log('Producer bound to port 3000');

setInterval(function(){
  console.log('sending work');
  sock.send('some work');
}, 500);
```

```js
// worker.js
var zmq = require('zmq')
  , sock = zmq.socket('pull');

sock.connect('tcp://127.0.0.1:3000');
console.log('Worker connected to port 3000');

sock.on('message', function(msg){
  console.log('work: %s', msg.toString());
});
```

### Pub/Sub

```js
// pubber.js
var zmq = require('zmq')
  , sock = zmq.socket('pub');

sock.bindSync('tcp://127.0.0.1:3000');
console.log('Publisher bound to port 3000');

setInterval(function(){
  console.log('sending a multipart message envelope');
  sock.send(['kitty cats', 'meow!']);
}, 500);
```

```js
// subber.js
var zmq = require('zmq')
  , sock = zmq.socket('sub');

sock.connect('tcp://127.0.0.1:3000');
sock.subscribe('kitty cats');
console.log('Subscriber connected to port 3000');

sock.on('message', function(topic, message) {
  console.log('received a message related to:', topic, 'containing message:', message);
});
```
## Monitoring

You can get socket state changes events by calling to the `monitor` function.
The supported events are (see ZMQ [docs](http://api.zeromq.org/4-2:zmq-socket-monitor) for full description):

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

### monitor(interval, numOfEvents)
Will create an inproc PAIR socket where zmq will publish socket state changes events, the events from this socket will
be read every `interval` (defaults to 10ms).
By default only 1 message will be read every interval, this can be configured by using the `numOfEvents` parameter,
where passing 0 will read all available messages per interval.

### unmonitor()
Stop the monitoring process

### example

```js
// Create a socket
var zmq = require('zmq');
socket = zmq.socket('req');

// Register to monitoring events
socket.on('connect', function(fd, ep) {console.log('connect, endpoint:', ep);});
socket.on('connect_delay', function(fd, ep) {console.log('connect_delay, endpoint:', ep);});
socket.on('connect_retry', function(fd, ep) {console.log('connect_retry, endpoint:', ep);});
socket.on('listen', function(fd, ep) {console.log('listen, endpoint:', ep);});
socket.on('bind_error', function(fd, ep) {console.log('bind_error, endpoint:', ep);});
socket.on('accept', function(fd, ep) {console.log('accept, endpoint:', ep);});
socket.on('accept_error', function(fd, ep) {console.log('accept_error, endpoint:', ep);});
socket.on('close', function(fd, ep) {console.log('close, endpoint:', ep);});
socket.on('close_error', function(fd, ep) {console.log('close_error, endpoint:', ep);});
socket.on('disconnect', function(fd, ep) {console.log('disconnect, endpoint:', ep);});

// Handle monitor error
socket.on('monitor_error', function(err) {
	console.log('Error in monitoring: %s, will restart monitoring in 5 seconds', err);
	setTimeout(function() { socket.monitor(500, 0); }, 5000);
});

// Call monitor, check for events every 500ms and get all available events.
console.log('Start monitoring...');
socket.monitor(500, 0);
socket.connect('tcp://127.0.0.1:1234');

setTimeout(function() {
	console.log('Stop the monitoring...');
	socket.unmonitor();
}, 20000);

```

## Detaching from the event loop
You may temporarily disable polling on a specific ZMQ socket and let the node.js
process to terminate without closing sockets explicitly by removing their event loop
references.  Newly created sockets are already `ref()`-ed.

### unref()
Detach the socket from the main event loop of the node.js runtime.
Calling this on already detached sockets is a no-op.

### ref()
Attach the socket to the main event loop.
Calling this on already attached sockets is a no-op.

### Example
```js
var zmq = require('zmq');
socket = zmq.socket('sub');
socket.bindSync('tcp://127.0.0.1:1234');
socket.subscribe('');
socket.on('message', function(msg) { console.log(msg.toString(); });
// Here blocks indefinitely unless interrupted.
// Let it terminate after 1 second.
setTimeout(function() { socket.unref(); }, 1000);
```

## Running tests

#### Install dev deps:
```sh
$ git clone https://github.com/JustinTulloss/zeromq.node.git zmq && cd zmq
$ npm i
```
#### Build:
```sh
# on unix:
$ make

# building on windows:
> npm i
```
#### Test:
```sh
# on unix:
$ make test

# testing on windows:
> npm t
```
## Running benchmarks

Benchmarks are available in the `perf` directory, and have been implemented
according to the zmq documentation:
[How to run performance tests](http://www.zeromq.org/results:perf-howto)

In the following examples, the arguments are respectively:
- the host to connect to/bind on
- message size (in bytes)
- message count

You can run a latency benchmark by running these two commands in two separate
shells:

```sh
node ./local_lat.js tcp://127.0.0.1:5555 1 100000
```

```sh
node ./remote_lat.js tcp://127.0.0.1:5555 1 100000
```

And you can run throughput tests by running these two commands in two
separate shells:

```sh
node ./local_thr.js tcp://127.0.0.1:5555 1 100000
```

```sh
node ./remote_thr.js tcp://127.0.0.1:5555 1 100000
```

Running `make perf` will run the commands listed above.
