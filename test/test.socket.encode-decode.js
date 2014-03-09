
var zmq = require('../')
  , should = require('should');

var rep = zmq.socket('rep')
  , req = zmq.socket('req');

req.encode = function (buf) {
  buf.toString().should.equal('unencoded');
  return new Buffer('encoded');
}

rep.decode = function (buf) {
  buf.toString().should.equal('encoded');
  return new Buffer('decoded');
}

rep.on('message', function(msg){
  msg.should.be.an.instanceof(Buffer);
  msg.toString().should.equal('decoded');
  req.close();
  rep.close();
});

rep.bind('inproc://stuff', function(){
  req.connect('inproc://stuff');
  req.send('unencoded');
});