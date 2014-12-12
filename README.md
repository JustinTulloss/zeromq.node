[![Build Status](https://travis-ci.org/JustinTulloss/zeromq.node.png)](https://travis-ci.org/JustinTulloss/zeromq.node)

# node-zeromq

  [ØMQ](http://www.zeromq.org/) bindings for node.js.

## Installation

First make sure [ZeroMQ is installed](http://www.zeromq.org/intro:get-the-software).
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
against libsodium. Check the Travis configuration for a list of what is tested
and therefore known to work.

With ZeroMQ headers installed, you can install and use this module:

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

## Running tests

  Install dev deps:

     $ npm install

  Build:

     $ make

  Test:

     $ make test

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

