var zmq = require('../')
var assert = require('assert')

if (process.argv.length != 5) {
  console.log('usage: local_thr <bind-to> <message-size> <message-count>')
  process.exit(1)
}

var bind_to = process.argv[2]
var message_size = Number(process.argv[3])
var message_count = Number(process.argv[4])
var counter = 0

var sub = zmq.socket('sub')
sub.bindSync(bind_to)
sub.subscribe('')

var timer = process.hrtime()

sub.on('message', function (data) {
  assert.equal(data.length, message_size, 'message-size did not match')
  if (++counter === message_count) finish()
})

function finish(){
  var endtime = process.hrtime(timer)
  var millis = (endtime[0]*1000) + (endtime[1]/1000000)
  var throughput = message_count / (millis / 1000)
  var megabits = (throughput * message_size * 8) / 1000000

  console.log('message size: %d [B]', message_size)
  console.log('message count: %d', message_count)
  console.log('mean throughput: %d [msg/s]', throughput.toFixed(0))
  console.log('mean throughput: %d [Mbit/s]', megabits.toFixed(0))
  console.log('overall time: %d secs and %d nanoseconds', endtime[0], endtime[1])
  sub.close()
}
