
var zmq = require('../')
  , should = require('should');

var pub = zmq.socket('pub')
  , sub = zmq.socket('sub');

var n = 0;

sub.subscribe('js');
sub.subscribe('luna');

sub.on('message', function(msg){
  msg.should.be.an.instanceof(Buffer);
  switch (n++) {
    case 0:
      msg.toString().should.equal('js is cool');
      break;
    case 1:
      msg.toString().should.equal('luna is cool too');
      pub.close();
      sub.close();
      break;
  }
});

sub.bind('inproc://stuff', function(){
  pub.connect('inproc://stuff');
  pub.send('js is cool');
  pub.send('ruby is meh');
  pub.send('py is pretty cool');
  pub.send('luna is cool too');
});
