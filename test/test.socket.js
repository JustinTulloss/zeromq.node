
var zmq = require('../')
  , should = require('should');

// alias

zmq.createSocket.should.equal(zmq.socket);

// .socket

var sock = zmq.socket('req');
sock.type.should.equal('req');
sock.close.should.be.type('function');

// setsockopt

sock.getsockopt(zmq.ZMQ_BACKLOG).should.not.equal(75);
sock.setsockopt(zmq.ZMQ_BACKLOG, 75).should.equal(sock);
sock.getsockopt(zmq.ZMQ_BACKLOG).should.equal(75);
sock.setsockopt(zmq.ZMQ_BACKLOG, 100);

// setsockopt + string sugar

sock.getsockopt('backlog').should.not.equal(75);
sock.setsockopt('backlog', 75).should.equal(sock);
sock.getsockopt('backlog').should.equal(75);

// setsockopt sugar

sock.backlog.should.be.type('number');
sock.backlog.should.not.equal(50);
sock.backlog = 50;
sock.backlog.should.equal(50);

sock.close();

// options

sock = zmq.socket('req', { backlog: 30 });
sock.backlog.should.equal(30);
sock.close();
