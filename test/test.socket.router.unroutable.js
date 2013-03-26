
var zmq = require('../')
  , should = require('should')
  , semver = require('semver');

if (!semver.gte(zmq.version, '3.2.0')) {
  console.warn('Test requires libzmq >= 3.2.0');
  return;
}

if (semver.eq(zmq.version, '3.2.1')) {
  console.warn('ZMQ_ROUTER_MANDATORY is broken in libzmq = 3.2.1');
  return;
}

// should emit an error event on unroutable msgs if mandatory = 1 and error handler is set

var sock = zmq.socket('router');
sock.on('error', function(err) {
  err.message.should.equal('No route to host');
  sock.close();
});

sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

var envelope = '12384982398293';
sock.send([envelope, '']);

// should throw an error on unroutable msgs if mandatory = 1 and no error handler is set

var sock = zmq.socket('router');

sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);

(function(){
  var envelope = '12384982398293';
  sock.send([envelope, '']);
}).should.throw('No route to host');
sock.close();

// should silently ignore unroutable msgs if mandatory = 0

var sock = zmq.socket('router');

(function(){
  var envelope = '12384982398293';
  sock.send([envelope, '']);
  sock.close();
}).should.not.throw('No route to host');
