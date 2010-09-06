zmq = require('zeromq');
sys = require('sys');

ctx = new zmq.Context();
s = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);
s1 = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);

msg = process.argv[2];

count = 0;
s.on('receive', function(data) {
    count--;
    sys.puts('received: ' + data);
    if (count == 0) {
        s.close();
    }
});
s.connect('tcp://127.0.0.1:5554');
s.send(msg);
count++;
s.send("just sent you a message, hope you got it");
count++;
s.send(new Buffer("Buffers are nice cause they can handle arbitrary data"));
count++;

s1.on('receive', function(data) {
    sys.puts('other socket received: ' + data)
    s1.close();
});

s1.connect('tcp://127.0.0.1:5554');
s1.send('I\'ve got a lovely bunch of coconuts');
