
# node-zeromq

  ØMQ bindings for node.js, a fork of [zeromq.node](https://github.com/JustinTulloss/zeromq.node).

## Installation

    $ npm install zeromq

## Example

producer.js:

```js
var zmq = require('zeromq')
  , sock = zmq.socket('push');

sock.bind('tcp://127.0.0.1:3000');
console.log('Producer bound to port 3000');

setInterval(function(){
  console.log('sending work');
  sock.send('some work');
}, 500);
```

worker.js:

```js

var zmq = require('zeromq')
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

