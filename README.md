
# node-zeromq

  ØMQ bindings for node.js.

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

## Contributors

 Authored by Justin Tulloss, maintained by Shripad K and TJ Holowaychuk.


 Output of `git summary`:

      project: zeromq.node
      commits: 260
      files  : 38
      authors: 
        114	Justin Tulloss          43.8%
         53	Tj Holowaychuk          20.4%
         48	Stéphan Kochen         18.5%
         12	jeremybarnes            4.6%
         10	TJ Holowaychuk          3.8%
          9	mike castleman          3.5%
          3	Yaroslav Shirokov       1.2%
          2	Corey Jewett            0.8%
          2	mgc                     0.8%
          1	rick                    0.4%
          1	Matt Crocker            0.4%
          1	Joshua Gourneau         0.4%
          1	Micheil Smith           0.4%
          1	Jeremy Barnes           0.4%
          1	nponeccop               0.4%
          1	Paul Bergeron           0.4%


