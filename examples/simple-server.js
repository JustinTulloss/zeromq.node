zmq = require('zeromq');
sys = require('sys');

ctx = new zmq.Context();
s = new zmq.Socket(ctx, zmq.Socket.ZMQ_REP);

s.on('receive', function(data) {
    sys.puts("received data: " + data);
    s.send("Your message was received");
});
s.bind('tcp://127.0.0.1:5554');
