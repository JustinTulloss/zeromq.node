
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

rep.on('listen', function(addr,iarg){
  console.log("listen %s,%d",addr,iarg);
  addr.toString().should.equal('tcp://127.0.0.1:5423');
  events.push('listen');
});

rep.on('accept', function(addr,iarg){
  console.log("accept %s,%d",addr,iarg);
  addr.toString().should.equal('tcp://127.0.0.1:5423');
  events.push('accept');
});

rep.on('disconnect', function(addr,iarg){
  console.log("disconnect %s,%d",addr, iarg);
  addr.toString().should.equal('tcp://127.0.0.1:5423');
  events.push('disconnect');
  events.length.should.equal(3);
});

/* enable monitoring for this socket */
rep.monitor();

rep.bind('tcp://127.0.0.1:5423');

rep.on('bind', function(){
  req.connect('tcp://127.0.0.1:5423');
  req.send('hello');
  req.on('message', function(msg){
    msg.should.be.an.instanceof(Buffer);
    msg.toString().should.equal('world');
    req.close();
    rep.close();
    /* wait a few for the close to reach us then
     * unmonitor to release the handle
     */
    setTimeout((function() {
      rep.unmonitor();
    }), 500);
  });
});

