
var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('exports', function(){
  it('should export a valid version', function(){
    semver.valid(zmq.version).should.be.ok;
  });

  it('should generate valid curve keypair', function(done) {
    try {
      var rep = zmq.socket('rep');
      rep.set('curve_server', 0);
    } catch(e) {
      console.log("libsodium seems to be missing (skipping curve test)");
      done();
      return;
    }

    var curve = zmq.curveKeypair();
    should.exist(curve);
    should.exist(curve.public);
    should.exist(curve.secret);
    curve.public.length.should.equal(40);
    curve.secret.length.should.equal(40);
    done();
  });

  it('should export methods', function(){
    zmq.socket.should.be.a.Function;
    zmq.createSocket.should.be.a.Function;
    zmq.context.should.be.a.Function;
    zmq.createContext.should.be.a.Function;
  });
});
