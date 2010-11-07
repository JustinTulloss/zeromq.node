/*
 * Sample code for stressing out zeromq.node
 *
 * It just opens up an XREP socket and responds to requests with ASCII strings
 * of incrementing numbers.
 *
 * Clients (in C and Ruby) are provided in stress-test-client.c and
 * stress-test-client.rb.
 *
 * When using the provided clients, you should notice that the thread
 * numbers and reply numbers match on the client and Javascript consoles,
 * but the ordering may of course be different.
 *
 * However, node.js will probably segfault eventually. This is something of a
 * problem. Sometimes, things will lock up without crashing. This may be even
 * more of a problem, because it is harder to detect.
 */

var zmq = require('zeromq'),
    util = require('util');

s = zmq.createSocket('xrep');

var i = 0;

s.bind('tcp://*:23456', function(err) {
  if (err)
    throw err;

  s.on('message', function(envelope, blank, data) {
    var buffer;
    
    ++i;
    util.puts('received [' + data.toString('utf8') + ']; replying with ' + i);
    buffer = new Buffer(i.toString(), 'ascii');

    process.nextTick(function() {
      s.send(envelope, blank, buffer);
    });
  });

  util.puts('socket initialized');
});
