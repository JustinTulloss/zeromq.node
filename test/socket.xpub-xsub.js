var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.xpub-xsub', function () {
    var pub, sub;
    
    beforeEach(function () {
        pub = zmq.socket('pub');
        sub = zmq.socket('sub');
		xpub = zmq.socket('xpub');
		xsub = zmq.socket('xsub');
    });
    
    it('should support pub-sub', function (done) {
        var n = 0;
		var m = 0;
		
		
	// publisher stuff
		pub.bindSync('tcp://*:5556');
		
	// broker
		xsub.connect('tcp://127.0.0.1:5556');
		xpub.bindSync('tcp://*:5555'); // what about high water mark?
		
	// subscriber		
		sub.on('message', function (msg) {
            msg.should.be.an.instanceof(Buffer);
            switch (n++) {
                case 0:
					console.log('first message');
                    msg.toString().should.equal('js is cool');
                    break;
                case 1:
					console.log('second message');
                    msg.toString().should.equal('luna is cool too');
                    break;
            }
        });
		
		sub.subscribe('js');
		sub.subscribe('luna');
		sub.connect('tcp://127.0.0.1:5555'); // connect to the xpub
		
		setTimeout(function () {
			sub.unsubscribe('luna');
		}, 500);
		
	// back to broker
		// When we receive data on xsub, it means someone is publishing
		xsub.on('message', function (msg) {		
			xpub.send(msg); // We just re send it out on the xpub, so subscribers can receive it
		});
		
		// When xpub receives a message, it's subscribe requests
		xpub.on('message', function (msg, bla) {
			var type = msg[0] === 0 ? 'unsubscribe' : 'subscribe'; // The first byte is the subscribe (1) /unsubscribe flag (0)		
			var channel = msg.slice(1).toString(); // The channel name is the rest of the buffer
			console.log(type + ':' + channel);
			
			switch(type) {
				case 'subscribe':
					switch(m++) {
						case 0:
							channel.should.equal('js');
							break;
						case 1:
							channel.should.equal('luna');
							break;
					}
					break;
				case 'unsubscribe':
					switch(m++)
					{
						case 2:
                            channel.should.equal('luna');
                            sub.close();
                            pub.close();
                            xsub.close();
                            xpub.close();
                            done();
							break;
					}
					break;
			}
			
			xsub.send(msg); // We just re send it out on the xsub so the publisher knows it has a subscriber 
		});
		
	// and now publish
	
	//	setInterval(function () {
	//		console.log("publisher is sending...");
	//		pub.send('A' + n);
	//		pub.send('B' + n);
	//		n++;
	//	}, 100);
		
		setTimeout(function() {
			pub.send('js is cool');
			pub.send('ruby is meh');
			pub.send('py is pretty cool');
			pub.send('luna is cool too');
		}, 100.0);
		
    });

});
