var should = require('should')
  , semver = require('semver')
  , zmq = require('../');
  
if (semver.gte(zmq.version, '4.0.0')) {
  var zap = require('./zap')
  	, port = 'tcp://127.0.0.1:12347'
    , zapSocket = zap.start()
    , rep = zmq.socket('rep')
    , req = zmq.socket('req');
	
  try {
    rep.curve_server = 0;
  } catch(e) {
    console.log("libsoduim seems to be missing; skipping curve test");
    process.exit(0);
  }

  var serverPublicKey = new Buffer('7f188e5244b02bf497b86de417515cf4d4053ce4eb977aee91a55354655ec33a', 'hex')
    , serverPrivateKey = new Buffer('1f5d3873472f95e11f4723d858aaf0919ab1fb402cb3097742c606e61dd0d7d8', 'hex')
    , clientPublicKey = new Buffer('ea1cc8bd7c8af65497d43fc21dbec6560c5e7b61bcfdcbd2b0dfacf0b4c38d45', 'hex')
    , clientPrivateKey = new Buffer('83f99afacfab052406e5f421612568034e85f4c8182a1c92671e83dca669d31d', 'hex');

  rep.on('message', function(msg){
    msg.should.be.an.instanceof(Buffer);
    msg.toString().should.equal('hello');
    rep.send('world');
  });

  rep.zap_domain = "test";
  rep.curve_server = 1;
  rep.curve_secretkey = serverPrivateKey;
  rep.mechanism.should.eql(2);

  var timeout = setTimeout(function() {
    req.close();
    rep.close();
    zapSocket.close();
    throw new Error("Request timed out");
  }, 1000);

  rep.bind(port, function(){
    req.curve_serverkey = serverPublicKey;
    req.curve_publickey = clientPublicKey;
    req.curve_secretkey = clientPrivateKey;
    req.mechanism.should.eql(2);

    req.connect(port);
    req.send('hello');
    req.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('world');
      clearTimeout(timeout);
      req.close();
      rep.close();
      zapSocket.close();
    });
  });
}
