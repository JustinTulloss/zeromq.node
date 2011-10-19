
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

pull.bind('inproc://stuff', function(){
  push.connect('inproc://stuff');
  push.send('string');
  push.send(15.99);
  push.send(new Buffer('buffer'));
});