# ZeroMQ bindings for versions 2.x, 3.x

  [ØMQ](http://www.zeromq.org/) bindings for node.js.

## Installation

    $ npm install zeromq-port

## Example

server.js:

```js
var zmq = require('zeromq-port');

// publisher = send only
var socket = zmq.createSocket('pub');

var stocks = ["AAPL", "GOOG", "YHOO", "MSFT", "INTC"];

socket.bind("tcp://127.0.0.1:12345", function(err) {
  if (err) console.log(err);
  console.log("bound!");
  setInterval(function() {
    var symbol = stocks[Math.floor(Math.random()*stocks.length)];
    var value = Math.random()*1000;
    socket.send(symbol+" "+value);
    console.log("Sent: "+symbol+" "+value);
  }, 100);
});
```

client.js:

```js

var zmq = require('zeromq-port');

// subscriber = receive only
var socket = zmq.createSocket('sub');
socket.connect("tcp://127.0.0.1:12345");
socket.subscribe("AAPL");

console.log("connected!");

socket.on('message', function(data) {
  console.log("received data: " + data.toString('utf8'));
});
```