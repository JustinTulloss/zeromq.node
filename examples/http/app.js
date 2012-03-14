
var zmq = require('../../')
  , sock = zmq.socket('dealer');

sock.on('message', function(){
  console.log(arguments);
});

sock.connect('tcp://127.0.0.1:5001');
console.log('app connected to :5001');