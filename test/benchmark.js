
var zmq = require('../')
  , should = require('should');

var pub = zmq.socket('pub')
  , sub = zmq.socket('sub');

var start = new Date
  , n = 100000
  , ops = n;

sub.subscribe('');
sub.on('message', function(msg){
  --n || (function(){
    var duration = new Date - start;
    pub.close();
    sub.close();
    console.log();
    console.log('  pub/sub:');
    console.log('    \033[36m%d\033[m msgs', ops);
    console.log('    \033[36m%d\033[m ops/s', ops / (duration / 1000) | 0);
    console.log('    \033[36m%d\033[m ms', duration);
    console.log();
  })();
});

sub.bind('ipc:///tmp/zmq.sock', function(){
  pub.connect('ipc:///tmp/zmq.sock');
  var times = n;
  while (times--) pub.send('foo');
});
