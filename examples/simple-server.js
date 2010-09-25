zmq = require('zeromq');
sys = require('sys');

s = zmq.createSocket('rep');

s.bind('tcp://127.0.0.1:5554', function(err) {
    if(err) throw new Error(err);
    s.on('message', function(data) {
        sys.puts("received data: " + data);
        s.send("Your message was received");
    });
    sys.puts('Ready to go!');
});
