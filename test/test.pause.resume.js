
var fs = require('fs');
var zmq = require('../')
  , should = require('should');

var pub = zmq.socket('pub')
  , sub = zmq.socket('sub');

sub.subscribe('');

var workDone;
sub.on('message', function(msg){
  if (msg.toString() === 'trigger work') {
    workDone = false;
    sub.pause();
    fs.appendFileSync('/tmp/pause.resume', 'paused\n');
    setTimeout(function(){
      fs.appendFileSync('/tmp/pause.resume', 'resumed\n');
      workDone = true;
      sub.resume();
    }, 200);
  } else {
    workDone.should.be.true;
    pub.close();
    sub.close();
  }
});
fs.appendFileSync('/tmp/pause.resume', 'come on\n');
sub.bind('inproc://stuff', function(){
  fs.appendFileSync('/tmp/pause.resume', 'we are alive\n');
  pub.connect('inproc://stuff');
  pub.send('trigger work');
  pub.send('received after work is done');
});
