
var zmq = require('../')
  , should = require('should')
  , semver = require('semver')
  , sock = zmq.socket('req');

if (semver.satisfies(zmq.version, '3.x')) {
  // ZMQ_SNDHWM
  sock.getsockopt(zmq.ZMQ_SNDHWM).should.not.equal(1);
  sock.setsockopt(zmq.ZMQ_SNDHWM, 1).should.equal(sock);
  sock.getsockopt(zmq.ZMQ_SNDHWM).should.equal(1);
  // ZMQ_RCVHWM
  sock.getsockopt(zmq.ZMQ_RCVHWM).should.not.equal(1);
  sock.setsockopt(zmq.ZMQ_RCVHWM, 1).should.equal(sock);
  sock.getsockopt(zmq.ZMQ_RCVHWM).should.equal(1);
}

sock.close();
