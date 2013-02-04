
var zmq = require('../')
  , should = require('should');

var push = zmq.socket('push')
  , pull = zmq.socket('pull');

var n = 0;

pull.on('message', function(msg){
  msg = msg.toString();
  switch (n++) {
    case 0:
      msg.should.equal('string');
      break;
    case 1:
      msg.should.equal('15.99');
      break;
    case 2:
      msg.should.equal('buffer');
      push.close();
      pull.close();
      break;
  }
});

setTimeout(function () {
  n.should.equal(3);
}, 1*1000);

if (zmq.version >= '3.0.0') {
  push.setsockopt(zmq.ZMQ_SNDHWM, 1);
  pull.setsockopt(zmq.ZMQ_RCVHWM, 1);
} else {
  push.setsockopt(zmq.ZMQ_HWM, 1);
  pull.setsockopt(zmq.ZMQ_HWM, 1);
}

push.bind('tcp://127.0.0.1:12345', function () {
  push.send('string');
  push.send(15.99);
  push.send(new Buffer('buffer'));
  pull.connect('tcp://127.0.0.1:12345');
});
