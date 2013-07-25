var zmq = require('../');
var assert = require('assert');

if (process.argv.length != 5) {
  console.log('usage: remote_lat <connect-to> <message-size> <roundtrip-count>');
  process.exit(1);
}

var connect_to = process.argv[2];
var message_size = Number(process.argv[3]);
var roundtrip_count = Number(process.argv[4]);
var message = new Buffer(message_size);
message.fill('h');

var recvCounter = 0;

var req = zmq.socket('req');
req.connect(connect_to);

var timer;

req.on('message', function (data) {
  if (!timer) {
    console.log('started receiving');
    timer = process.hrtime();
  }

  assert.equal(data.length, message_size, 'message-size did not match');

  if (++recvCounter === roundtrip_count) {
    finish();
  } else {
    send();
  }
});

function finish() {
  var duration = process.hrtime(timer);
  var millis = duration[0] * 1000 + duration[1] / 1000000;

  console.log('message size: %d [B]', message_size);
  console.log('roundtrip count: %d', roundtrip_count);
  console.log('mean latency: %d [msecs]', millis / (roundtrip_count * 2));
  console.log('overall time: %d secs and %d nanoseconds', duration[0], duration[1]);
  req.close()
}

function send() {
  req.send(message);
}

send()
