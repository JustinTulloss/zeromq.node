var NO = 384;

var zmq = require("zmq");

var socket = zmq.createSocket("pub");
socket.bind("tcp://127.0.0.1:9000", function() {});

setInterval(function() {
  var b = new Buffer(4);
  b[0] = NO >>> 24;
  b[1] = NO >>> 16;
  b[2] = NO >>> 8;
  b[3] = NO % 256;
  socket.send(b);
}, 1000);
