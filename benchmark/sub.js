
var zmq = require('../')
  , sub = zmq.socket('sub');

var start
  , ops = 100000;

sub.subscribe('');
sub.on('message', function(msg){
  console.error(msg);
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
  start = new Date;
  console.log('sub bound');
});
