
var zmq = require('../')
  , should = require('should');

// socket types

['PUB', 'SUB', 'REQ', 'XREQ', 'XREP',
 'DEALER', 'ROUTER', 'PUSH', 'PULL', 'PAIR'].forEach(function(type){
  zmq['ZMQ_' + type].should.be.a('number');
});

// socket opts

['HWM', 'SWAP', 'AFFINITY', 'IDENTITY',
 'SUBSCRIBE', 'UNSUBSCRIBE', 'RATE',
 'RECOVERY_IVL', 'RECOVERY_IVL', 'MCAST_LOOP',
 'SNDBUF', 'RCVBUF', 'RCVMORE', 'SNDMORE', 'FD', 'EVENTS',
 'TYPE', 'LINGER', 'RECONNECT_IVL', 'BACKLOG'].forEach(function(prop){
  zmq['ZMQ_' + prop].should.be.a('number');
});

// states

['STATE_READY', 'STATE_BUSY', 'STATE_CLOSED'].forEach(function(state){
  zmq[state].should.be.a('number');
})

// constructors

zmq.Context.should.be.a('function');
zmq.Socket.should.be.a('function');

// methods

zmq.createSocket.should.be.a('function');
