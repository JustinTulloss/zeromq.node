zmq = require('zeromq');
util = require('util');

s = zmq.createSocket('rep');

s.bind('tcp://127.0.0.1:5554', function(err) {
    if (err) throw err;
    s.on('message', function(data) {
        util.puts("received data: " + data.toString('utf8'));
        s.send("Your message was received");
    });
    util.puts('Ready to go!');
});
