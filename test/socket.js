
var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket', function(){
  var sock;

  it('should alias socket', function(){
    zmq.createSocket.should.equal(zmq.socket);
  });

  it('should include type and close', function(){
    sock = zmq.socket('req');
    sock.type.should.equal('req');
    sock.close.should.be.a.Function;
  });

  it('should use socketopt', function(){
    sock.getsockopt(zmq.ZMQ_BACKLOG).should.not.equal(75);
    sock.setsockopt(zmq.ZMQ_BACKLOG, 75).should.equal(sock);
    sock.getsockopt(zmq.ZMQ_BACKLOG).should.equal(75);
    sock.setsockopt(zmq.ZMQ_BACKLOG, 100);
  });

  it('should use socketopt with sugar', function(){
    sock.getsockopt('backlog').should.not.equal(75);
    sock.setsockopt('backlog', 75).should.equal(sock);
    sock.getsockopt('backlog').should.equal(75);

    sock.backlog.should.be.a.Number;
    sock.backlog.should.not.equal(50);
    sock.backlog = 50;
    sock.backlog.should.equal(50);
  });

  it('should close', function(){
    sock.close();
  });

  it('should support options', function(){
    sock = zmq.socket('req', { backlog: 30 });
    sock.backlog.should.equal(30);
    sock.close();
  });

  describe('events', function(){
    it('should support events', function(done){
      var rep = zmq.socket('rep')
        , req = zmq.socket('req');

      rep.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('hello');
        rep.send('world');
      });

      rep.bind('inproc://stuff');

      rep.on('bind', function(){
        req.connect('inproc://stuff');
        req.send('hello');
        req.on('message', function(msg){
          msg.should.be.an.instanceof(Buffer);
          msg.toString().should.equal('world');
          req.close();
          rep.close();
          done();
        });
      });
    });
  });

  describe('messages', function(){
    var push, pull;

    beforeEach(function(){
      push = zmq.socket('push');
      pull = zmq.socket('pull');
    });

    afterEach(function(){
      push.close();
      pull.close();
    });

    it('should support messages', function(done){
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
            done();
            break;
        }
      });

      pull.bind('inproc://stuff', function(){
        push.connect('inproc://stuff');
        push.send('string');
        push.send(15.99);
        push.send(new Buffer('buffer'));
      });
    });

    it('should support multipart messages', function(done){
      pull.on('message', function(msg1, msg2, msg3){
        msg1.toString().should.equal('string');
        msg2.toString().should.equal('15.99');
        msg3.toString().should.equal('buffer');
        done();
      });

      pull.bind('inproc://stuff', function(){
        push.connect('inproc://stuff');
        push.send(['string', 15.99, new Buffer('buffer')]);
      });
    });

    it('should support sndmore', function() {
      pull.on('message', function(a, b, c, d, e){
        a.toString().should.equal('tobi');
        b.toString().should.equal('loki');
        c.toString().should.equal('jane');
        d.toString().should.equal('luna');
        e.toString().should.equal('manny');
        done();
      });

      pull.bind('inproc://stuff', function(){
        push.connect('inproc://stuff');
        push.send(['tobi', 'loki'], zmq.ZMQ_SNDMORE);
        push.send(['jane', 'luna'], zmq.ZMQ_SNDMORE);
        push.send('manny');
      });
    });

    it('should handle late connect', function(done){
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
            done();
            break;
        }
      });

      if (semver.satisfies(zmq.version, '>=3.x')) {
        push.setsockopt(zmq.ZMQ_SNDHWM, 1);
        pull.setsockopt(zmq.ZMQ_RCVHWM, 1);
      } else if (semver.satisfies(zmq.version, '2.x')) {
        push.setsockopt(zmq.ZMQ_HWM, 1);
        pull.setsockopt(zmq.ZMQ_HWM, 1);
      }

      push.bind('tcp://127.0.0.1:12345', function () {
        push.send('string');
        push.send(15.99);
        push.send(new Buffer('buffer'));
        pull.connect('tcp://127.0.0.1:12345');
      });
    });
  });

  describe('pub-sub', function(){
    var pub, sub;

    beforeEach(function() {
      pub = zmq.socket('pub');
      sub = zmq.socket('sub');
    });

    afterEach(function(){
      sub.close();
      pub.close();
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
            done();
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
            done();
            break;
        }
      });

      sub.bind('inproc://stuff', function(){
        pub.connect('inproc://stuff');

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

  describe('req-rep', function(){
    it('should support req-rep', function(done){
      var rep = zmq.socket('rep')
        , req = zmq.socket('req');

      rep.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('hello');
        rep.send('world');
      });

      rep.bind('inproc://stuff', function(){
        req.connect('inproc://stuff');
        req.send('hello');
        req.on('message', function(msg){
          msg.should.be.an.instanceof(Buffer);
          msg.toString().should.equal('world');
          rep.close();
          req.close();
          done();
        });
      });
    });

    it('should support multiple', function(done){
      var n = 5;

      for (var i = 0; i < n; i++) {
        (function(n){
          var rep = zmq.socket('rep')
            , req = zmq.socket('req');

          rep.on('message', function(msg){
            msg.should.be.an.instanceof(Buffer);
            msg.toString().should.equal('hello');
            rep.send('world');
          });

          rep.bind('inproc://' + n, function(){
            req.connect('inproc://' + n);
            req.send('hello');
            req.on('message', function(msg){
              msg.should.be.an.instanceof(Buffer);
              msg.toString().should.equal('world');
              req.close();
              rep.close();
              if (!--n) done();
            });
          });
        })(i);
      }
    });
  });

  describe('router', function(){
    it('should handle the unroutable', function(done){
      var complete = 0;

      if (!semver.gte(zmq.version, '3.2.0'))
        return console.warn('Test requires libzmq >= 3.2.0');

      if (semver.eq(zmq.version, '3.2.1'))
        return console.warn('ZMQ_ROUTER_MANDATORY is broken in libzmq = 3.2.1');

      var envelope = '12384982398293';

      // should emit an error event on unroutable msgs if mandatory = 1 and error handler is set

      (function(){
        var sock = zmq.socket('router');
        sock.on('error', function(err){
          err.message.should.equal('No route to host');
          sock.close();
          if (++complete === 2) done();
        });

        sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

        sock.send([envelope, '']);
      })();

      // should throw an error on unroutable msgs if mandatory = 1 and no error handler is set

      (function(){
        var sock = zmq.socket('router');

        sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

        (function(){
          sock.send([envelope, '']);
        }).should.throw('No route to host');
        sock.close();
      })();

      // should silently ignore unroutable msgs if mandatory = 0

      (function(){
        var sock = zmq.socket('router');

        (function(){
          sock.send([envelope, '']);
          sock.close();
        }).should.not.throw('No route to host');
      })();
      if (++complete === 2) done();
    });
  });
});
