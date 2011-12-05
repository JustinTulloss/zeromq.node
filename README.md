
# node-zeromq

  ØMQ bindings for node.js

## Installation

    $ npm install zmq

### Note on version 2.0.0
Version 2.0.0 is a large refactoring that is cleaner, simpler, and better
tested. It is not currently well documented and there are very few examples.
Patches are welcome, but also note that version 1.0.4 is stable and has
more getting started materials. In general version 2.0.0 works the same
as 1.0.4, but there are some small differences.

You can install version 1.0.4 by running

    $ npm install zmq@1.0.4

Many thanks to @visionmedia for doing all the work for version 2!

## Example

producer.js:

```js
var zmq = require('zeromq')
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

