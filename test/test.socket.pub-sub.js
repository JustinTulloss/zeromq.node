
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

var addr = "inproc://stuff";

sub.bind(addr, function(){
  pub.connect(addr);

  // The connect is asynchronous, and messages published to a non-
  // connected socket are silently dropped.  That means that there is
  // a race between connecting and sending the first message which
  // causes this test to hang, especially when running on Linux. Even an
  // inproc:// socket seems to be asynchronous.  So instead of
  // sending straight away, we wait 100ms for the connection to be
  // established before we start the send.  This fixes the observed
  // hang.

  setTimeout(function() {
    pub.send('foo');
    pub.send('bar');
    pub.send('baz');
  }, 100.0);
});
