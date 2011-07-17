var NO = 384;

var zmq = require("zmq");

var b = new Buffer(4);
b[0] = NO >>> 24;
b[1] = NO >>> 16;
b[2] = NO >>> 8;
b[3] = NO % 256;

var socket = zmq.createSocket("sub");
socket.connect("tcp://127.0.0.1:9000");

socket.subscribe(b);
socket.on("message", function(msg) {
  console.log(msg);
});
