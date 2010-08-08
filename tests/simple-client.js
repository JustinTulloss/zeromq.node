zmq = require('zeromq');
sys = require('sys');

ctx = new zmq.Context();
s = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);
msg = process.argv[2];

s.connect('tcp://127.0.0.1:5554');
s.send(msg);
sys.puts(s.recv());
