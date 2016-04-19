
var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var version = semver.gte(zmq.version, '3.2.0');

describe('socket.error-callback', function(){
  var sock;

  if (!version) {
    return console.warn("ZMQ_ROUTER_MANDATORY requires libzmq 3.2.0, skipping test");
  }

  it('should create a socket and set ZMQ_ROUTER_MANDATORY', function () {
    sock = zmq.socket('router');
    sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);
  });

  it('should callback with error when not connected', function (done) {
    sock.send(['foo', 'bar'], null, function (error) {
      error.should.be.an.instanceof(Error);
      done();
    });
  });
});
