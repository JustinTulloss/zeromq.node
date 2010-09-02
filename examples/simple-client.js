zmq = require('zeromq');
sys = require('sys');

ctx = new zmq.Context();
s = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);
s1 = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);

msg = process.argv[2];

s.on('receive', function(data) {
    sys.puts('received: ' + data);
    s.close();
});
s.connect('tcp://127.0.0.1:5554');
s.send(msg);

s1.on('receive', function(data) {
    sys.puts('other socket received: ' + data);
    s1.close();
});

s1.connect('tcp://127.0.0.1:5554');
s1.send('I\'ve got a lovely bunch of coconuts');
