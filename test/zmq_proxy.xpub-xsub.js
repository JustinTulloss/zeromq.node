var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5507'
  , backendAddr = addr+':5508'
  , captureAddr = addr+':5509';

var version = semver.gte(zmq.version, '3.1.0');

describe('proxy.xpub-xsub', function() {

  it('should proxy pub-sub connected to xpub-xsub', function (done) {
    if (!version) {
      done();
      return console.warn('Test requires libzmq >= 3.1.0');
    }

    var frontend = zmq.socket('xpub');
    var backend = zmq.socket('xsub');

    var sub = zmq.socket('sub');
    var pub = zmq.socket('pub');

    sub.subscribe('');
    sub.on('message',function (msg) {

      frontend.close();
      backend.close();
      sub.close();
      pub.close();

      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');

      done();
    });

    frontend.bind(frontendAddr, function (error) {
      if (error) throw error;
      backend.bind(backendAddr, function (error) {
        if (error) throw error;

        sub.connect(frontendAddr);
        pub.connect(backendAddr);

        setTimeout(function() {
          pub.send('foo');
        }, 200.0);

        zmq.proxy(frontend, backend);

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

    sub.subscribe('');
    sub.on('message', function (msg) {

      sub.close();
      pub.close();
      backend.close();
      frontend.close();

      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo');
    });

    capSub.subscribe('');
    capSub.on('message',function (msg) {

      capture.close();
      capSub.close();

      setTimeout(function(){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('foo');
        done();
      },100.0);
    });

    capture.bind(captureAddr, function (error) {
      if (error) throw error;
      frontend.bind(frontendAddr, function (error) {
        if (error) throw error;
        backend.bind(backendAddr, function (error) {
          if (error) throw error;

          pub.connect(backendAddr);
          sub.connect(frontendAddr);
          capSub.connect(captureAddr);

          setTimeout(function () {
            pub.send('foo');
          }, 200.0);

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

    try{

      zmq.proxy(backend,frontend);

    } catch(e){

      e.message.should.equal('wrong socket order to proxy');

    } finally{
      frontend.close();
      backend.close();
      pub.close();
      sub.close();

      //allow time for TCP sockets to close
      setTimeout(function(){
        done();
      },200)
    }
  })
});
