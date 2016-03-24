var zmq = require('..')
  , should = require('should');

describe('socket.events', function(){

  it('should support events', function(done){
    var rep = zmq.socket('rep')
      , req = zmq.socket('req');

    rep.on('message', function(msg){
      msg.should.be.an.instanceof(Buffer);
      msg.toString().should.equal('hello');
      rep.send('world');
    });

    rep.bind('inproc://stuffevents', function (error) {
      if (error) throw error;
    });

    rep.on('bind', function(){
      req.connect('inproc://stuffevents');
      req.on('message', function(msg){
        msg.should.be.an.instanceof(Buffer);
        msg.toString().should.equal('world');
        req.close();
        rep.close();
        done();
      });
      req.send('hello');
    });
  });
});
