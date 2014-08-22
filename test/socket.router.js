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

      (function(){
        sock.send([envelope, '']);
      }).should.throw('No route to host');

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
