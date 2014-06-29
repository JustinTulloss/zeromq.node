 
var zmq = require('../')
  , should = require('should');

var push = zmq.socket('push')
  , pull = zmq.socket('pull');

var sentnoflags = false, sentwithflags = false;

pull.on('message', function(a){
  sentnoflags.should.equal(true);
  sentwithflags.should.equal(true);
  push.close();
  pull.close();
});

pull.bind('inproc://stuff', function(){
  push.connect('inproc://stuff');
  push.send('hi', function () {
    sentnoflags = true;
  });
  push.send('hi', 0, function () {
    sentwithflags = true;
  });
});