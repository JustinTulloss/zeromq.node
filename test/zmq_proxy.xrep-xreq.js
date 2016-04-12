var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5510'
  , backendAddr = addr+':5511'
  , captureAddr = addr+':5512';

//since its for libzmq2, we target versions < 3.0.0
var version = semver.lte(zmq.version, '3.0.0');

describe('proxy.xrep-xreq', function() {
  it('should proxy req-rep connected to xrep-xreq', function (done) {
    if (!version) {
      console.warn('Test requires libzmq v2 (skipping)');
      return done();
    }

    var frontend = zmq.socket('xrep');
    var backend = zmq.socket('xreq');

    var req = zmq.socket('req');
    var rep = zmq.socket('rep');

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
});
