
var zmq = require('../')
  , should = require('should');

var sock = zmq.createSocket('req');
sock.type.should.equal('req');

sock.close.should.be.a('function');

// setsockopt

sock.getsockopt(zmq.ZMQ_BACKLOG).should.not.equal(75);
sock.setsockopt(zmq.ZMQ_BACKLOG, 75).should.equal(sock);
sock.getsockopt(zmq.ZMQ_BACKLOG).should.equal(75);

// setsockopt sugar

sock.backlog.should.be.a('number');
sock.backlog.should.not.equal(50);
sock.backlog = 50;
sock.backlog.should.equal(50);

process.exit(0);