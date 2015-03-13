var zmq = require('..')
  , should = require('should');

describe('ReadableSocket', function () {
  it('should expose ReadableSocket', function () {
    zmq.ReadableSocket.should.exist;
  });

  it('should expose createReadableSocket', function () {
    zmq.createReadableSocket.should.exist;
  });

  it('should not support pub, xpub, req, xreq, rep, xrep, push, dealer, router, pair, stream socket type', function () {
    ['pub', 'xpub', 'req', 'xreq', 'rep', 'xrep', 'push', 'dealer', 'router', 'pair', 'stream'].forEach(function (type) {
      (function () {
        var sock = new zmq.ReadableSocket(type);
      }).should.throw();
    });
  });

  it('should support sub, xsub and pull socket type', function () {
    ['sub', 'xsub', 'pull'].forEach(function (type) {
      (function () {
        var sock = new zmq.ReadableSocket(type);
      }).should.not.throw();
    });
  });

  it('should not implement send', function(){
    (function () {
      var sock = new zmq.ReadableSocket('pull');
      sock.send()
    }).should.throw('Can not send using ReadableSocket.');
  });

  it('should emit data on message arrival', function (done) {
    var sender = zmq.socket('push')
      , receiver = new zmq.ReadableSocket('pull');

    receiver.bind('inproc://ReadableSocketTest');

    receiver.on('bind', function(){
      sender.connect('inproc://ReadableSocketTest');
      sender.send('hello');
    });

    receiver.on('data', function () {
      receiver.close();
      sender.close();
      done();
    });
  });
});