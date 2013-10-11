
var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('exports', function(){
  it('should export a valid version', function(){
    semver.valid(zmq.version).should.be.ok;
  });

  it('should export socket types and options', function(){
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
      'RCVTIMEO',
      'SNDTIMEO',
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
      'RECONNECT_IVL_MAX',
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
        'ZMQ_RECOVERY_IVL_MSEC',
        'NOBLOCK'
      ]);
    }

    // 3.0 and above.
    if (semver.gte(zmq.version, '3.0.0')) {
      constants.concat([
        'XPUB',
        'XSUB',
        'SNDHWM',
        'RCVHWM',
        'MAXMSGSIZE',
        'ZMQ_MULTICAST_HOPS',
        'TCP_KEEPALIVE',
        'TCP_KEEPALIVE_CNT',
        'TCP_KEEPALIVE_IDLE',
        'TCP_KEEPALIVE_INTVL'
      ]);
    }

    // 3.2 and above.
    if (semver.gte(zmq.version, '3.2.0')) {
      constants.concat([
        'IPV4ONLY',
        'DELAY_ATTACH_ON_CONNECT',
        'ROUTER_MANDATORY',
        'XPUB_VERBOSE',
        'TCP_KEEPALIVE',
        'TCP_KEEPALIVE_IDLE',
        'TCP_KEEPALIVE_CNT',
        'TCP_KEEPALIVE_INTVL',
        'TCP_ACCEPT_FILTER',
        'LAST_ENDPOINT'
      ]);
    }

    // 3.3 and above.
    if (semver.gte(zmq.version, '3.3.0')) {
      constants.concat([
        'ROUTER_RAW'
      ]);
    }

    constants.forEach(function(typeOrProp){
      zmq['ZMQ_' + typeOrProp].should.be.a.Number;
    });
  });

  it('should export states', function(){
    ['STATE_READY', 'STATE_BUSY', 'STATE_CLOSED'].forEach(function(state){
      zmq[state].should.be.a.Number;
    });
  });

  it('should export constructors', function(){
    zmq.Context.should.be.a.Function;
    zmq.Socket.should.be.a.Function;
  });

  it('should export methods', function(){
    zmq.socket.should.be.a.Function;
  });
});
