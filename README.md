[![Build Status](https://travis-ci.org/JustinTulloss/zeromq.node.png)](https://travis-ci.org/JustinTulloss/zeromq.node)

# node-zeromq

  [ØMQ](http://www.zeromq.org/) bindings for node.js.

## Installation

[Install zmq package](http://www.zeromq.org/intro:get-the-software) first.

    $ npm install zmq

## Example

producer.js:

```js
var zmq = require('zmq')
  , sock = zmq.socket('push');

sock.bindSync('tcp://127.0.0.1:3000');
console.log('Producer bound to port 3000');

setInterval(function(){
  console.log('sending work');
  sock.send('some work');
}, 500);
```

worker.js:

```js

var zmq = require('zmq')
  , sock = zmq.socket('pull');

sock.connect('tcp://127.0.0.1:3000');
console.log('Worker connected to port 3000');

sock.on('message', function(msg){
  console.log('work: %s', msg.toString());
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

