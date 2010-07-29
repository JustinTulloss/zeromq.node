zmq = require('zeromq');
sys = require('sys');

ctx = new zmq.Context();
s = new zmq.Socket(ctx, zmq.Socket.ZMQ_REQ);
msg = new zmq.Message(process.argv[1]);

s.connect('tcp://127.0.0.1:5554');
s.send(msg);
sys.print(s.recv());
