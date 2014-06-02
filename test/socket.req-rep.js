var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.req-rep', function(){
  it('should support req-rep', function(done){
    var rep = zmq.socket('rep')
      , req = zmq.socket('req');

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.bind('inproc://stuff', function(){
      req.connect('inproc://stuff');
      req.send('hello');
      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        rep.close();
        req.close();
        done();
      });
    });
  });

  it('should support multiple', function(done){
    var n = 5;

    for (var i = 0; i < n; i++) {
      (function(n){
        var rep = zmq.socket('rep')
          , req = zmq.socket('req');

        rep.on('message', function(msg){
          msg.should.be.an.instanceof(Buffer);
          msg.toString().should.equal('hello');
          rep.send('world');
        });

        rep.bind('inproc://' + n, function(){
          req.connect('inproc://' + n);
          req.send('hello');
          req.on('message', function(msg){
            msg.should.be.an.instanceof(Buffer);
            msg.toString().should.equal('world');
            req.close();
            rep.close();
            if (!--n) done();
          });
        });
      })(i);
    }
  });

});
