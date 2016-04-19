var zmq = require('..')
  , should = require('should')
  , semver = require('semver')

describe('context', function() {
  if (!semver.gte(zmq.version, '3.2.0')) {
    return console.warn('Test requires libzmq >= 3.2.0');
  }

  it('should support setting max io threads', function(done) {
    var ctx = zmq.getDefaultContext();

    ctx.set('ZMQ_IO_THREADS', 3);
    ctx.get('ZMQ_IO_THREADS').should.equal(3);
    ctx.set('ZMQ_IO_THREADS', 1);
    done();
  });

  it('should support setting max number of sockets', function(done) {
    var ctx = zmq.getDefaultContext();

    var currMaxSockets = ctx.get('ZMQ_MAX_SOCKETS');
    ctx.set('ZMQ_MAX_SOCKETS', 256);
    ctx.get('ZMQ_MAX_SOCKETS').should.equal(256);
    ctx.set('ZMQ_MAX_SOCKETS', currMaxSockets);
    done();
  });

});
