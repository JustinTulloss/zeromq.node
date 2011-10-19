
var zmq = require('../')
  , should = require('should');

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
    req.close();
    rep.close();
  });
});