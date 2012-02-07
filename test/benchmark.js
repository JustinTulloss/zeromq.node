
var zmq = require('../')
  , should = require('should');

var pub = zmq.socket('pub')
  , sub = zmq.socket('sub');

var start = new Date
  , n = 100000
  , total = n;

sub.subscribe('');
sub.on('message', function(msg){
  --n || (function(){
    var duration = new Date - start;
    pub.close();
    sub.close();
    console.log();
    console.log('  pub/sub:');
    console.log('    %d msgs', total);
    console.log('    %dms', duration);
    console.log();
  })();
});

sub.bind('ipc:///tmp/zmq.sock', function(){
  pub.connect('ipc:///tmp/zmq.sock');
  var times = n;
  while (times--) pub.send('foo');
});
