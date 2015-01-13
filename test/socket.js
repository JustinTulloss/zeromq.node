
var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket', function(){
  var sock;

  it('should alias socket', function(){
    zmq.createSocket.should.equal(zmq.socket);
  });

  it('should include type and close', function(){
    sock = zmq.socket('req');
    sock.type.should.equal('req');
    sock.close.should.be.a.Function;
  });

  it('should use socketopt', function(){
    sock.getsockopt(zmq.ZMQ_BACKLOG).should.not.equal(75);
    sock.setsockopt(zmq.ZMQ_BACKLOG, 75).should.equal(sock);
    sock.getsockopt(zmq.ZMQ_BACKLOG).should.equal(75);
    sock.setsockopt(zmq.ZMQ_BACKLOG, 100);
  });

  it('should use socketopt with sugar', function(){
    sock.getsockopt('backlog').should.not.equal(75);
    sock.setsockopt('backlog', 75).should.equal(sock);
    sock.getsockopt('backlog').should.equal(75);

    sock.backlog.should.be.a.Number;
    sock.backlog.should.not.equal(50);
    sock.backlog = 50;
    sock.backlog.should.equal(50);
  });

  it('should close', function(){
    sock.close();
  });

  it('should support options', function(){
    sock = zmq.socket('req', { backlog: 30 });
    sock.backlog.should.equal(30);
    sock.close();
  });

  it('should throw a javascript error if it hits the system file descriptor limit', function() {
    var i, socks = [], numSocks = 10000;
    function hitlimit() {
      for (i = 0; i < numSocks; i++) {
        socks.push(zmq.socket('router'));
      }
    }
    hitlimit.should['throw'];
    for (i = 0; i < socks.length; i++) {
      socks[i].close();
    }
  });

});
