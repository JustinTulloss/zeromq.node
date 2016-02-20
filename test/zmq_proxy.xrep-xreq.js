var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

var addr = 'tcp://127.0.0.1'
  , frontendAddr = addr+':5510'
  , backendAddr = addr+':5511'
  , captureAddr = addr+':5512';

//since its for libzmq2, we target versions < 3.0.0
var version = semver.lte(zmq.version, '3.0.0');
var testutil = require('./util');

describe('proxy.xrep-xreq', function() {
  afterEach(testutil.cleanup);
  
  it('should proxy req-rep connected to xrep-xreq', function (done) {
    if (!version) {
      done();
      return console.warn('Test requires libzmq v2');
    }

    var frontend = zmq.socket('xrep');
    var backend = zmq.socket('xreq');

    var req = zmq.socket('req');
    var rep = zmq.socket('rep');

    frontend.bindSync(frontendAddr);
    backend.bindSync(backendAddr);

    req.connect(frontendAddr);
    rep.connect(backendAddr);
    testutil.push_sockets(frontend, backend, req, rep);

    req.on('message',function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('foo bar');
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