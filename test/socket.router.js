var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.router', function(){
  it('should handle the unroutable', function(done){
    var complete = 0;

    if (!semver.gte(zmq.version, '3.2.0')) {
      done();
      return console.warn('Test requires libzmq >= 3.2.0');
    }

    if (semver.eq(zmq.version, '3.2.1')) {
      done();
      return console.warn('ZMQ_ROUTER_MANDATORY is broken in libzmq = 3.2.1');
    }

    var envelope = '12384982398293';

    var errMsgs = require('os').platform() === 'win32' ? ['Unknown error'] : [];
    errMsgs.push('No route to host');
    errMsgs.push('Resource temporarily unavailable');

    function assertRouteError(err) {
      if (errMsgs.indexOf(err.message) === -1) {
        throw new Error('Bad error');
      }
    }

    // should emit an error event on unroutable msgs if mandatory = 1 and error handler is set

    (function(){
      var sock = zmq.socket('router');
      sock.on('error', function (err) {
        sock.close();
        assertRouteError(err);
        if (++complete === 2) done();
      });

      sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

      sock.send([envelope, '']);
    })();

    // should throw an error on unroutable msgs if mandatory = 1 and no error handler is set

    (function(){
      var sock = zmq.socket('router');

      sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

      try {
        sock.send([envelope, '']);
      } catch (err) {
        assertRouteError(err);
      }

      try {
        sock.send([envelope, '']);
      } catch (err) {
        assertRouteError(err);
      }

      try {
        sock.send([envelope, '']);
      } catch (err) {
        assertRouteError(err);
      }

      sock.close();
    })();

    // should silently ignore unroutable msgs if mandatory = 0

    (function(){
      var sock = zmq.socket('router');

      (function(){
        sock.send([envelope, '']);
        sock.close();
      }).should.not.throw;
    })();
    if (++complete === 2) done();
  });

  it('should handle router <-> dealer message bursts', function (done) {
    // tests https://github.com/JustinTulloss/zeromq.node/issues/523
    // based on https://gist.github.com/messa/862638ab44ca65f712fe4d6ef79aeb67

    var router = zmq.socket('router');
    var dealer = zmq.socket('dealer');
    var addr = 'tcp://127.0.0.1:12345';
    var expected = 1000;
    var counted = 0;

    router.bindSync(addr);

    router.on('message', function () {
      var msg = [];
      for (var i = 0; i < arguments.length; i += 1) {
        msg.push(arguments[i]);
      }
      router.send(msg);
    });

    dealer.on('message', function (part1, part2, part3, part4, part5) {
      String(part1).should.equal('Hello');
      String(part2).should.equal('world');
      String(part3).should.equal('part3');
      String(part4).should.equal('part4');
      String(part5).should.equal('undefined');

      counted += 1;
      if (counted === expected) {
        router.close();
        dealer.close();
        done();
      }
    });

    dealer.connect(addr);

    for (var i = 0; i < expected; i += 1) {
      dealer.send(['Hello', 'world', 'part3', 'part4']);
    }
  });
});
