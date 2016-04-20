var zmq = require('..');
var should = require('should');

describe('socket.zap', function () {
  if (!zmq.socketOptionExists('ZMQ_CURVE_SERVER')) {
    return console.log('ZMQ_CURVE_SERVER does not exist in this version of ZMQ (skipping curve test)');
  }

  var zap = require('./zap');
  var zapSocket, rep, req, count = 0;

  beforeEach(function () {
    count++;
    zapSocket = zap.start(count);
    rep = zmq.socket('rep');
    req = zmq.socket('req');
  });

  afterEach(function () {
    req.close();
    rep.close();
    zapSocket.close();
  });

  it('should support curve', function (done) {
    var port = 'tcp://127.0.0.1:12347';

    try {
      rep.set('ZMQ_CURVE_SERVER', 0);
    } catch(e) {
      console.log('libsodium seems to be missing (skipping curve test)');
      return done();
    }

    var serverPublicKey = new Buffer('7f188e5244b02bf497b86de417515cf4d4053ce4eb977aee91a55354655ec33a', 'hex');
    var serverPrivateKey = new Buffer('1f5d3873472f95e11f4723d858aaf0919ab1fb402cb3097742c606e61dd0d7d8', 'hex');
    var clientPublicKey = new Buffer('ea1cc8bd7c8af65497d43fc21dbec6560c5e7b61bcfdcbd2b0dfacf0b4c38d45', 'hex');
    var clientPrivateKey = new Buffer('83f99afacfab052406e5f421612568034e85f4c8182a1c92671e83dca669d31d', 'hex');

    rep.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.zap_domain = 'test';
    rep.set('curve_server', 1);
    rep.set('curve_secretkey', serverPrivateKey);
    rep.get('mechanism').should.eql(2);

    rep.bind(port, function (error) {
      if (error) throw error;
      req.set('curve_serverkey', serverPublicKey);
      req.set('curve_publickey', clientPublicKey);
      req.set('curve_secretkey', clientPrivateKey);
      req.get('mechanism').should.eql(2);

      req.connect(port);
      req.send('hello');
      req.on('message', function (msg) {
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        done();
      });
    });

  });

  it('should support null', function (done) {
    var port = 'tcp://127.0.0.1:12345';

    rep.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.set('zap_domain', 'test');
    rep.get('mechanism').should.eql(0);

    rep.bind(port, function (error) {
      if (error) throw error;
      req.get('mechanism').should.eql(0);
      req.connect(port);
      req.send('hello');
      req.on('message', function (msg) {
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        done();
      });
    });
  });

  it('should support plain', function (done) {
    var port = 'tcp://127.0.0.1:12346';

    rep.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.set('zap_domain', 'test');
    rep.set('plain_server', 1);
    rep.get('mechanism').should.eql(1);

    rep.bind(port, function (error) {
      if (error) throw error;
      req.set('plain_username', 'user');
      req.set('plain_password', 'pass');
      req.get('mechanism').should.eql(1);

      req.connect(port);
      req.send('hello');
      req.on('message', function (msg) {
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        done();
      });
    });
  });
});
