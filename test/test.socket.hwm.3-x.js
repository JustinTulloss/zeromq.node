
var zmq = require('../')
  , should = require('should')
  , sock = zmq.socket('req');

if (zmq.version >= '3.0.0') {
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
