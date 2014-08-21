
var zmq = require('../')
  , should = require('should');

var rep = zmq.socket('rep')
  , req = zmq.socket('req');

req.transform = function (buf, idx, arr) {
  buf.toString().should.equal('original');
  return new Buffer('transformed');
}

rep.restore = function (buf, idx, arr) {
  buf.toString().should.equal('transformed');
  return new Buffer('restored');
}

rep.on('message', function(msg){
  msg.should.be.an.instanceof(Buffer);
  msg.toString().should.equal('restored');
  req.close();
  rep.close();
});

rep.bind('inproc://stuff', function(){
  req.connect('inproc://stuff');
  req.send('original');
});