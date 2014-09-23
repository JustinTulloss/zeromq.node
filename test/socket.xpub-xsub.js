var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.xpub-xsub', function () {
    var pub, sub, xpub, xsub;
    
    beforeEach(function () {
        pub = zmq.socket('pub');
        sub = zmq.socket('sub');
    });
    
    it('should support pub-sub tracing and filtering', function (done) {    	
		if (!semver.gte(zmq.version, '3.1.0')) {
			done();
			return console.warn('Test requires libzmq >= 3.1.0');
		}
		
		var n = 0;
		xpub = zmq.socket('xpub');
		xsub = zmq.socket('xsub');

		pub.bindSync('tcp://*:5556');	
		xsub.connect('tcp://127.0.0.1:5556');
		xpub.bindSync('tcp://*:5555'); 		
        sub.connect('tcp://127.0.0.1:5555');
        
        sub.on('message', function (msg) {
            msg.should.be.an.instanceof(Buffer);
            switch (n++) {
                case 0:
                    msg.toString().should.equal('js is cool');
                    break;
                case 1:
                    msg.toString().should.equal('luna is cool too');
                    break;
            }
        });
        
        sub.subscribe('js');
        sub.subscribe('luna');
		
        XSubXPubProxy(xsub, xpub, done);
        
        setTimeout(function () {
            pub.send('js is cool');
            pub.send('ruby is meh');
            pub.send('py is pretty cool');
            pub.send('luna is cool too');
        }, 100.0);
        
        setTimeout(function () {
            sub.unsubscribe('luna');
        }, 300);
    });

    XSubXPubProxy = function (xsub, xpub, done) {
        var n = 0;

        xsub.on('message', function (msg) {
            xpub.send(msg); // Forward message using the xpub so subscribers can receive it
        });
        
        xpub.on('message', function (msg) {
            msg.should.be.an.instanceof(Buffer);

            var type = msg[0] === 0 ? 'unsubscribe' : 'subscribe';
            var channel = msg.slice(1).toString();
            
            switch (type) {
                case 'subscribe':
                    switch (n++) {
                        case 0:
                            channel.should.equal('js');
                            break;
                        case 1:
                            channel.should.equal('luna');
                            break;
                    }
                    break;
                case 'unsubscribe':
                    switch (n++) {
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
            
            xsub.send(msg); // Forward message using the xsub so the publisher knows it has a subscriber 
        });
    }
});
