
var zmq = require('../')
  , should = require('should')
  , semver = require('semver');

// version

semver.valid(zmq.version).should.be.ok;

// socket types and socket opts

// All versions.
var constants = [
  'PUB',
  'SUB',
  'REQ',
  'XREQ',
  'REP',
  'XREP',
  'DEALER',
  'ROUTER',
  'PUSH',
  'PULL',
  'PAIR',
  'AFFINITY',
  'IDENTITY',
  'SUBSCRIBE',
  'UNSUBSCRIBE',
  'RATE',
  'RECOVERY_IVL',
  'SNDBUF',
  'RCVBUF',
  'RCVMORE',
  'FD',
  'EVENTS',
  'TYPE',
  'LINGER',
  'RECONNECT_IVL',
  'BACKLOG',
  'POLLIN',
  'POLLOUT',
  'POLLERR',
  'SNDMORE'
];

// 2.x only.
if (semver.satisfies(zmq.version, '2.x')) {
  constants.concat([
    'HWM',
    'SWAP',
    'MCAST_LOOP',
    'NOBLOCK'
  ]);
}

// 3.x only.
if (semver.satisfies(zmq.version, '3.x')) {
  constants.concat([
    'XPUB',
    'XSUB',
    'SNDHWM',
    'RCVHWM'
  ]);
}

// 3.2 and above.
if (semver.gte('3.2')) {
  constants.concat([
    'LAST_ENDPOINT'
  ]);
}

constants.forEach(function(typeOrProp){
  zmq['ZMQ_' + typeOrProp].should.be.a('number');
});

// states

['STATE_READY', 'STATE_BUSY', 'STATE_CLOSED'].forEach(function(state){
  zmq[state].should.be.a('number');
})

// constructors

zmq.Context.should.be.a('function');
zmq.Socket.should.be.a('function');

// methods

zmq.socket.should.be.a('function');
