var zmq = require('..')
  , should = require('should');

describe('socket.req-rep', function(){
  it('should support req-rep', function(done){
    var rep = zmq.socket('rep')
      , req = zmq.socket('req');

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.bind('inproc://stuffreqrep', function (error) {
      if (error) throw error;
      req.connect('inproc://stuffreqrep');
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

        rep.bind('inproc://' + n, function (error) {
          if (error) throw error;
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

  it('should support a burst', function (done) {
    var rep = zmq.socket('rep');
    var req = zmq.socket('req');
    var n = 10;

    rep.on('message', function (msg) {
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.bind('inproc://reqrepburst', function (error) {
      if (error) throw error;
      req.connect('inproc://reqrepburst');

      var received = 0;

      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');

        received += 1;

        if (received === n) {
          rep.close();
          req.close();
          done();
        }
      });

      for (var i = 0; i < n; i += 1) {
        req.send('hello');
      }
    });
  });

});
