var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.messages', function(){
  var push, pull;

  beforeEach(function(){
    push = zmq.socket('push');
    pull = zmq.socket('pull');
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
          push.close();
          pull.close();
          done();
          break;
      }
    });

    pull.bind('inproc://stuff_ssm', function (error) {
      if (error) throw error;
      push.connect('inproc://stuff_ssm');
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
      push.close();
      pull.close();
      done();
    });

    pull.bind('inproc://stuff_ssmm', function (error) {
      if (error) throw error;
      push.connect('inproc://stuff_ssmm');
      push.send(['string', 15.99, new Buffer('buffer')]);
    });
  });

  it('should support sndmore', function(done){
    pull.on('message', function(a, b, c, d, e){
      a.toString().should.equal('tobi');
      b.toString().should.equal('loki');
      c.toString().should.equal('jane');
      d.toString().should.equal('luna');
      e.toString().should.equal('manny');
      push.close();
      pull.close();
      done();
    });

    pull.bind('inproc://stuff_sss', function (error) {
      if (error) throw error;
      push.connect('inproc://stuff_sss');
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
          push.close();
          pull.close();
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

    push.bind('tcp://127.0.0.1:12345', function (error) {
      if (error) throw error;
      push.send('string');
      push.send(15.99);
      push.send(new Buffer('buffer'));
      pull.connect('tcp://127.0.0.1:12345');
    });
  });

  it('should call send() callbacks', function(done){
    var received = 0;
    var callbacks = 0;

    function cb() {
      callbacks += 1;
    }

    pull.on('message', function () {
      received += 1;

      if (received === 4) {
        callbacks.should.equal(received);
        done();
      }
    });

    pull.bind('inproc://stuff_ssmm', function (error) {
      if (error) throw error;
      push.connect('inproc://stuff_ssmm');

      push.send('hello', null, cb);
      push.send('hello', null, cb);
      push.send('hello', null, cb);
      push.send(['hello', 'world'], null, cb);
    });
  });
});
