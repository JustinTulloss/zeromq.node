var zmq = require('..')
  , should = require('should')
  , semver = require('semver');


describe('socket.immediate', function() {
	
	it('should test ZMQ_IMMEDIATE socket option', function(done) {
		
		var msgsToSend = 5
		  , receivedMsgs = 0;
		
		var push = zmq.socket('push');
		push.setsockopt('immediate', 1);
		
		push.connect('tcp://127.0.0.1:5423');
		
		for(var i=0; i<msgsToSend; i++) {
			push.send('message #'+i);
		}
		
		var pull = zmq.socket('pull');
		
		var pull = zmq.socket('pull');
		
		pull.on('message', function(data) {
			receivedMsgs += 1;
		});
		
		pull.bind('tcp://127.0.0.1:5423', function(err){
			if (err) throw err;
		});
		
		setTimeout(function(){
			receivedMsgs.should.equal(0);
			done();
		}, 1000);
	});
});
