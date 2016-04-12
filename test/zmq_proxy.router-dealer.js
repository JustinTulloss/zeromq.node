var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5504'
  , backendAddr = addr+':5505'
  , captureAddr = addr+':5506';

describe('proxy.router-dealer', function() {

  it('should proxy req-rep connected over router-dealer', function (done){

    var frontend = zmq.socket('router');
    var backend = zmq.socket('dealer');

    var rep = zmq.socket('rep');
    var req = zmq.socket('req');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);

    req.connect(frontendAddr);
    rep.connect(backendAddr);

    req.on('message',function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo bar');
      frontend.close();
      backend.close();
      req.close();
      rep.close();
      done();
    });

    rep.on('message', function (msg) {
      rep.send(msg+' bar');
    });

    setTimeout(function() {
      req.send('foo');
    }, 100.0);

    zmq.proxy(frontend,backend);

  });

  it('should proxy rep-req connections with capture', function (done){

    var frontend = zmq.socket('router');
    var backend = zmq.socket('dealer');

    var rep = zmq.socket('rep');
    var req = zmq.socket('req');

    var capture = zmq.socket('pub');
    var capSub = zmq.socket('sub');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);
    capture.bindSync(captureAddr);

    req.connect(frontendAddr);
    rep.connect(backendAddr);
    capSub.connect(captureAddr);
    capSub.subscribe('');

    req.on('message',function (msg) {
      req.close();
      rep.close();
    });

    rep.on('message', function (msg) {
      rep.send(msg+' bar');
    });

    capSub.on('message',function (msg) {
      backend.close();
      frontend.close();
      capture.close();
      capSub.close();
      setTimeout(function() {
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('foo bar');
        done();
      },100.0)
    });

    setTimeout(function() {
      req.send('foo');
    },200.0)

    zmq.proxy(frontend,backend,capture);

  });
});
