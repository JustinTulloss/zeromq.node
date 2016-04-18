
var zmq = require('..')
  , should = require('should');

describe('socket.error-callback', function(){
  var sock;

  if (!zmq.ZMQ_ROUTER_MANDATORY) {
    console.log("ZMQ_ROUTER_MANDATORY not available, skipping test");
    return;
  }

  it('should create a socket and set ZMQ_ROUTER_MANDATORY', function () {
    sock = zmq.socket('router');
    sock.setsockopt(zmq.ZMQ_ROUTER_MANDATORY, 1);
  });

  it('should callback with error when not connected', function (done) {
    sock.send(['foo', 'bar'], null, function (error) {
      error.should.be.an.instanceof(Error);
      done();
    });
  });
});
