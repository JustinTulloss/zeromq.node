var zmq = require('../');
var assert = require('assert');

if (process.argv.length != 5) {
  console.log('usage: local_lat <bind-to> <message-size> <roundtrip-count>');
  process.exit(1);
}

var bind_to = process.argv[2];
var message_size = Number(process.argv[3]);
var roundtrip_count = Number(process.argv[4]);
var counter = 0;

var rep = zmq.socket('rep');
rep.bindSync(bind_to);

rep.on('message', function (data) {
  assert.equal(data.length, message_size, 'message-size did not match');
  rep.send(data);
  if (++counter === roundtrip_count){ 
    setTimeout( function(){ 
      rep.close();
    }, 1000); 
  }
})
