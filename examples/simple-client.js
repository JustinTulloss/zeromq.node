zmq = require('zeromq');
util = require('util');

s = zmq.createSocket('req');
s1 = zmq.createSocket('req');

msg = process.argv[2];

count = 0;
s.on('message', function(data) {
    count--;
    util.puts('received: ' + data.toString('utf8'));
    if (count == 0) {
        s.close();
    }
});
s.on('error', function(error) {
    count--;
    util.puts(error);
});

s.connect('tcp://127.0.0.1:5554');
s.send(msg);
count++;
s.send("just sent you a message, hope you got it");
count++;
s.send(new Buffer("Buffers are nice cause they can handle arbitrary data"));
count++;

s1.on('message', function(data) {
    util.puts('other socket received: ' + data.toString('utf8'))
    s1.close();
});

s1.connect('tcp://127.0.0.1:5554');
s1.send('I\'ve got a lovely bunch of coconuts');
