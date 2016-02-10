var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5507'
  , backendAddr = addr+':5508'
  , captureAddr = addr+':5509';

var version = semver.gte(zmq.version, '3.1.0');
var testutil = require('./util');

describe('proxy.xpub-xsub', function() {
  afterEach(testutil.cleanup);
  
  it('should proxy pub-sub connected to xpub-xsub', function (done) {
    if (!version) {
      done();
      return console.warn('Test requires libzmq >= 3.1.0');
    }

    var frontend = zmq.socket('xpub');
    var backend = zmq.socket('xsub');

    var sub = zmq.socket('sub');
    var pub = zmq.socket('pub');
    testutil.push_sockets(frontend, backend, sub, pub);

    sub.subscribe('');
    sub.on('message',function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');
      console.log(msg.toString());
      done();
    });

    frontend.bind(frontendAddr,function() {
      backend.bind(backendAddr,function() {

        sub.connect(frontendAddr);
        pub.connect(backendAddr);

        setTimeout(function() {
          pub.send('foo');
        }, 500);

        setTimeout(function () {
          throw Error("Timeout");
        }, 10000);

        zmq.proxy(frontend,backend);

      });
    });
  });

  it('should proxy connections with capture', function (done) {
    if (!version) {
      done();
      return console.warn('Test requires libzmq >= 3.1.0');
    }

    var frontend = zmq.socket('xpub');
    var backend = zmq.socket('xsub');

    var capture = zmq.socket('pub');
    var capSub = zmq.socket('sub');

    var sub = zmq.socket('sub');
    var pub = zmq.socket('pub');
    testutil.push_sockets(frontend, backend, sub, pub, capture, capSub);

    var countdown = testutil.done_countdown(done, 2);

    sub.subscribe('');
    sub.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');

      console.log(msg.toString());
      countdown();
    });

    capSub.subscribe('');
    capSub.on('message',function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');
      countdown();
    });

    capture.bind(captureAddr,function() {
      frontend.bind(frontendAddr,function() {
        backend.bind(backendAddr,function() {

          pub.connect(backendAddr);
          sub.connect(frontendAddr);
          capSub.connect(captureAddr);

          setTimeout(function () {
            pub.send('foo');
          }, 500);

          setTimeout(function () {
            throw Error("Timeout");
          }, 10000);

          zmq.proxy(frontend,backend,capture);
        });
      });
    });
  });

  it('should throw an error if the order is wrong', function (done) {
    if (!version) {
      done();
      return console.warn('Test requires libzmq >= 3.1.0');
    }

    var frontend = zmq.socket('xpub');
    var backend = zmq.socket('xsub');

    var sub = zmq.socket('sub');
    var pub = zmq.socket('pub');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);

    sub.connect(frontendAddr);
    pub.connect(backendAddr);
    testutil.push_sockets(frontend, backend, sub, pub);

    try{

      zmq.proxy(backend,frontend);

    } catch(e){

      e.message.should.equal('wrong socket order to proxy');

    } finally{

      done();

    }
  })
});