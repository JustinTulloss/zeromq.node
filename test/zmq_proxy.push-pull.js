var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5501'
  , backendAddr = addr+':5502'
  , captureAddr = addr+':5503';

describe('proxy.push-pull', function() {

  it('should proxy push-pull connected to pull-push',function (done) {

    var frontend = zmq.socket('pull');
    var backend = zmq.socket('push');

    var pull = zmq.socket('pull');
    var push = zmq.socket('push');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);

    push.connect(frontendAddr);
    pull.connect(backendAddr);

    pull.on('message',function (msg) {

      frontend.close();
      backend.close();
      push.close();
      pull.close();

      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');
      done();
    });

    setTimeout(function() {
      push.send('foo');
    }, 100.0);

    zmq.proxy(frontend,backend);

  });

  it('should proxy pull-push connected to push-pull with capture',function (done) {

    var frontend = zmq.socket('push');
    var backend = zmq.socket('pull');

    var capture = zmq.socket('pub');
    var capSub = zmq.socket('sub');

    var pull = zmq.socket('pull');
    var push = zmq.socket('push');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);
    capture.bindSync(captureAddr);

    pull.connect(frontendAddr);
    push.connect(backendAddr);
    capSub.connect(captureAddr);

    pull.on('message',function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');
    });

    capSub.subscribe('');
    capSub.on('message',function (msg) {
      capture.close();
      capSub.close();

      setTimeout(function() {
        frontend.close();
        backend.close();
        push.close();
        pull.close();

        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('foo');
        done();
      },100.0);
    });

    setTimeout(function() {
      push.send('foo');
    }, 100.0);

    zmq.proxy(frontend,backend,capture);

  });
});
