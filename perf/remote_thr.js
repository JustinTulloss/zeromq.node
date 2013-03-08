var zmq = require('../')
var assert = require('assert')

if (process.argv.length != 5) {
  console.log('usage: remote_thr <bind-to> <message-size> <message-count>')
  process.exit(1)
}

var connect_to = process.argv[2]
var message_size = Number(process.argv[3])
var message_count = Number(process.argv[4])
var message = new Buffer(message_size)
message.fill('h')

console.log(message.length)

var counter = 0

var pub = zmq.socket('pub')
pub.connect(connect_to)

function send(){
  process.nextTick(function () {
    pub.send(message)
    if (++counter < message_count) send()
    // else pub.close() // all messages may not be received by local_thr if closed
  })
}
send()
