 
var zmq = require('../')
  , should = require('should');

var push = zmq.socket('push')
  , pull = zmq.socket('pull');

pull.on('message', function(msg1, msg2, msg3){
  msg1.toString().should.equal('string');
  msg2.toString().should.equal('15.99');
  msg3.toString().should.equal('buffer');
  push.close();
  pull.close();
});

pull.bind('inproc://stuff', function(){
  push.connect('inproc://stuff');
  push.send(['string', 15.99, new Buffer('buffer')]);
});