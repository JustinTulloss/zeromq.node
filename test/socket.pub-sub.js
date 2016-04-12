var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.pub-sub', function(){
  var pub, sub;

  beforeEach(function() {
    pub = zmq.socket('pub');
    sub = zmq.socket('sub');
  });

  it('should support pub-sub', function(done){
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
          done();
          break;
      }
    });

    var addr = "inproc://stuff_ssps";

    sub.bind(addr, function (error) {
      if (error) throw error;
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
  });

  it('should support pub-sub filter', function(done){
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
          sub.close();
          pub.close();
          done();
          break;
      }
    });

    sub.bind('inproc://stuff_sspsf', function (error) {
      if (error) throw error;
      pub.connect('inproc://stuff_sspsf');

      // See comments on pub-sub test.

      setTimeout(function() {
        pub.send('js is cool');
        pub.send('ruby is meh');
        pub.send('py is pretty cool');
        pub.send('luna is cool too');
      }, 100.0);
    });
  });

});
