var zmq = require('..')
  , should = require('should')
  , semver = require('semver');

describe('socket.events', function(){

  it('should support events', function(done){
    var rep = zmq.socket('rep')
      , req = zmq.socket('req');

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.bind('inproc://stuff');

    rep.on('bind', function(){
      req.connect('inproc://stuff');
      req.send('hello');
      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        req.close();
        rep.close();
        done();
      });
    });
  });
});
