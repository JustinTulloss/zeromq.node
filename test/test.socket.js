
var zmq = require('../')
  , should = require('should');

var sock = zmq.createSocket('req');
sock.type.should.equal('req');

sock.backlog.should.be.a('number');

process.exit(0);