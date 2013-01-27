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
