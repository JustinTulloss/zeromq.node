
var zmq = require('../../')
  , sock = zmq.socket('push');

sock.bind('tcp://127.0.0.1:3000');
console.log('Producer bound to port 3000');

setInterval(function(){
  console.log('sending work');
  sock.send('some work');
}, 500);