
var zmq = require('../')
  , should = require('should');

var rep = zmq.socket('rep')
  , req = zmq.socket('req');
  
var events = [];

rep.on('message', function(msg){
  msg.should.be.an.instanceof(Buffer);
  msg.toString().should.equal('hello');
  rep.send('world');
});

rep.on('listen', function(){
  events.push('listen');
});

rep.on('accept', function(){
  events.push('accept');
});

rep.on('close', function(){
  events.push('close');
  events.length.should.equal(3);
});

rep.bind('tcp://127.0.0.1:5555');

rep.on('bind', function(){
  req.connect('tcp://127.0.0.1:5555');
  req.send('hello');
  req.on('message', function(msg){
    msg.should.be.an.instanceof(Buffer);
    msg.toString().should.equal('world');
    req.close();
    rep.close();
  });
});