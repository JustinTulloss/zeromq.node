
var zmq = require('../')
  , should = require('should');

var push = zmq.socket('push')
  , pull = zmq.socket('pull');

var n = 0;

pull.on('message', function(msg){
  msg.should.be.an.instanceof(Buffer);
  switch (n++) {
    case 0:
      msg.toString().should.equal('foo');
      break;
    case 1:
      msg.toString().should.equal('bar');
      break;
    case 2:
      msg.toString().should.equal('baz');
      pull.close();
      push.close();
      break;
  }
});

var addr = "inproc://stuff";

pull.bind(addr, function(){
  push.connect(addr);

  push.send('foo');
  push.send('bar');
  push.send('baz');
});
