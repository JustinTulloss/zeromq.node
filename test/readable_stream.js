var zmq = require('..')
  , should = require('should');

describe('ReadableStream', function () {
  it('should expose ReadableStream', function () {
    zmq.ReadableStream.should.exist;
  });

  it('should expose createReadableStream', function () {
    zmq.createReadableStream.should.exist;
  });

  it('should not support plain socket', function () {
    (function () {
      var sock = new zmq.socket('pull'),
        stream = new zmq.createReadableStream(sock);
    }).should.throw('Socket must be a ReadableSocket.');
  });

  it('should emit data received trough the socket', function (done) {
    var sender = zmq.socket('push')
      , receiver = new zmq.createReadableSocket('pull')
      , stream = new zmq.createReadableStream(receiver);

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