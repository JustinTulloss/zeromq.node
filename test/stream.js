var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

if (!semver.gte(process.version, '0.10.0')) {
  return console.warn('Test requires Node >= 0.10.0');
}

describe('ReadableStream', function () {
  it('should expose ReadableStream', function () {
    zmq.ReadableStream.should.exist;
  });

  it('should expose createStream', function () {
    zmq.createStream.should.exist;
  });

  it('should support sub, xsub and pull socket type', function() {
    ['sub', 'xsub', 'pull'].forEach(function(type) {
      (function() {
        // check for supported types, xsub not supported in 0mq 2-x
        if (!zmq.types[type]) {
          return;
        }

        var sock = zmq.createSocket(type, { autoFlush: false }),
          stream = zmq.createStream(sock);
      }).should.not.throw();
    });
  });

  it('should emit data received trough the socket', function (done) {
    var sender = zmq.socket('push')
      , receiver = zmq.createSocket('pull', { autoFlush: false })
      , stream = zmq.createStream(receiver);

    receiver.bind('inproc://ReadableStreamTest');

    receiver.on('bind', function(){
      sender.connect('inproc://ReadableStreamTest');
      sender.send('hello');
    });

    stream.on('data', function (messageArray) {
      messageArray[0].toString().should.equal('hello');
      receiver.close();
      sender.close();
      done();
    });
  });
});