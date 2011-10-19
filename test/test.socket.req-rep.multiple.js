
var zmq = require('../')
  , should = require('should');

var n = 5;

while (n--) {
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
      });
    });
  })(n);
}