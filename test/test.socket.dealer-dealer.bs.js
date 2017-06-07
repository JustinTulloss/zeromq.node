
var zmq = require('../')
  , should = require('should');

var a = zmq.socket('dealer')
  , b = zmq.socket('dealer');

a.on('message', function(msg) {
  msg.should.be.an.instanceof(Buffer);
  msg.toString().should.equal('hello');
  a.close();
  b.close();
  clearTimeout(timeout);
});

b.bind('inproc://stuff', function() {
  a.connect('inproc://stuff');
  b.send('hello');
});

// guard against hanging
var timeout = setTimeout(function() {
  throw new Error('Timeout');
}, 1000);
