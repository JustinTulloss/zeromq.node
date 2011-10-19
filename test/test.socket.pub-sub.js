
var zmq = require('../')
  , should = require('should');

var pub = zmq.socket('pub')
  , sub = zmq.socket('sub');

var n = 0;

sub.subscribe('');
sub.on('message', function(msg){
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
      sub.close();
      pub.close();
      break;
  }
});

sub.bind('inproc://stuff', function(){
  pub.connect('inproc://stuff');
  pub.send('foo');
  pub.send('bar');
  pub.send('baz');
});
