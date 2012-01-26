 
var zmq = require('../')
  , should = require('should');

var push = zmq.socket('push')
  , pull = zmq.socket('pull');

pull.on('message', function(a, b, c, d, e){
  a.toString().should.equal('tobi');
  b.toString().should.equal('loki');
  c.toString().should.equal('jane');
  d.toString().should.equal('luna');
  e.toString().should.equal('manny');
  push.close();
  pull.close();
});

pull.bind('inproc://stuff', function(){
  push.connect('inproc://stuff');
  push.send(['tobi', 'loki'], zmq.ZMQ_SNDMORE);
  push.send(['jane', 'luna'], zmq.ZMQ_SNDMORE);
  push.send('manny');
});