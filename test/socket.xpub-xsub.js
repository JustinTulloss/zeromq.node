var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.xpub-xsub', function () {
  it('should support pub-sub tracing and filtering', function (done) {	
    if (!semver.gte(zmq.version, '3.1.0')) {
      done();
      return console.warn('Test requires libzmq >= 3.1.0');
    }

    var n = 0;
    var m = 0;
    var pub = zmq.socket('pub');
    var sub = zmq.socket('sub');
    var xpub = zmq.socket('xpub');
    var xsub = zmq.socket('xsub');

    pub.bindSync('tcp://*:5556');	
    xsub.connect('tcp://127.0.0.1:5556');
    xpub.bindSync('tcp://*:5555'); 		
    sub.connect('tcp://127.0.0.1:5555');

    xsub.on('message', function (msg) {
      xpub.send(msg); // Forward message using the xpub so subscribers can receive it
    });
        
    xpub.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);

      var type = msg[0] === 0 ? 'unsubscribe' : 'subscribe';
      var channel = msg.slice(1).toString();
            
      switch (type) {
        case 'subscribe':
          switch (m++) {
            case 0:
              channel.should.equal('js');
              break;
            case 1:
              channel.should.equal('luna');
              break;
          }
          break;
        case 'unsubscribe':
          switch (m++) {
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
});
