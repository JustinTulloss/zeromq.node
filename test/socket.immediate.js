var zmq = require('..')
  , should = require('should')
  , semver = require('semver');
  
describe('socket.immediate', function(){
  
  it('should support immediate socket option', function (done){
    
    if (semver.gte(zmq.version, '4.0.0')) {
      
      var push = zmq.socket('push');
      push.setsockopt('immediate', 1);
      
    } else {
      done();
      return console.warn('stream socket type in libzmq v4+');
    }
  });
});