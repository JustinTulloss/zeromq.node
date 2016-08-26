var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var address = 'tcp://127.0.0.1:5556'
  , receivedMsgs = 0;

// we target versions > 3.3.0
var version = semver.gte(zmq.version, '3.3.0');

describe('socket.immediate', function() {
	
	it('should test ZMQ_IMMEDIATE socket option', function(done) {
		
		if (!version) {
			done();
			return console.warn('Test requires libzmq v3.3');
		}
		
		var push = zmq.socket('push');
		push.setsockopt('immediate', 1);
		
		push.connect(address);
		
		push.send('Hello');
		
		var pull = zmq.socket('pull');
		
		pull.bind(address);
		
		pull.on('message', function(msg) {
			receivedMsgs += 1;
		});
		
		setTimeout(function(){
			receivedMsgs.should.equal(0);
			push.close();
			pull.close();
			done();
		}, 1000);
	});
});
