
var zmq = require('../')
  , pub = zmq.socket('pub');

var n = 100000
  , ops = n;

pub.connect('ipc:///tmp/zmq.sock', function(){
  console.log('publishing %d messages', n);
  while (ops--) pub.send('foo');
});
