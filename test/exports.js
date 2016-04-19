
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
      rep.setsockopt('curve_server', 0);
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

  it('should export states', function(){
    ['STATE_READY', 'STATE_BUSY', 'STATE_CLOSED'].forEach(function(state){
      zmq[state].should.be.a.Number;
    });
  });

  it('should export constructors', function(){
    zmq.Context.should.be.a.Function;
    zmq.Socket.should.be.a.Function;
  });

  it('should export methods', function(){
    zmq.socket.should.be.a.Function;
  });
});
