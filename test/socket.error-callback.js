
var zmq = require('..');
var should = require('should');
var semver = require('semver');

describe('socket.error-callback', function () {
  if (!semver.gte(zmq.version, '3.2.0')) {
    return console.warn("ZMQ_ROUTER_MANDATORY requires libzmq 3.2.0, skipping test");
  }

  var sock;

  it('should create a socket and set ZMQ_ROUTER_MANDATORY', function () {
    sock = zmq.socket('router');
    sock.set('ZMQ_ROUTER_MANDATORY', 1);
  });

  it('should callback with error when not connected', function (done) {
    sock.send(['foo', 'bar'], null, function (error) {
      error.should.be.an.instanceof(Error);
      done();
    });
  });
});
