/*
 *
 * Forwarder device
 *
 */

var zmq = require('../../')
    , frontPort = 'tcp://127.0.0.1:12345'
    , backPort = 'tcp://127.0.0.1:12346';

function createClient (port) {
    var socket = zmq.socket('pub');

    socket.identity = 'client' + process.pid;

    socket.connect(port);
    console.log('client connected!');

    setInterval(function() {
        var value = Math.floor(Math.random()*100);

        console.log(socket.identity + ': broadcasting ' + value);
        socket.send(value);
    }, 100);
};

function createWorker (port) {
    var socket = zmq.socket('sub');

    socket.identity = 'worker' + process.pid;

    socket.subscribe('');
    socket.on('message', function(data) {
        console.log(socket.identity + ': got ' + data.toString());
    });

    socket.connect(port, function(err) {
        if (err) throw err;
        console.log('worker connected!');
    });
};

function createForwarderDevice(frontPort, backPort) {
    var frontSocket = zmq.socket('sub'),
        backSocket = zmq.socket('pub');

    frontSocket.identity = 'sub' + process.pid;
    backSocket.identity = 'pub' + process.pid;

    frontSocket.subscribe('');
    frontSocket.bind(frontPort, function (err) {
        console.log('bound', frontPort);
    });
    
    frontSocket.on('message', function() {
        //pass to back
        console.log('forwarder: recasting', arguments[0].toString());
        backSocket.send(Array.prototype.slice.call(arguments));
    });

    backSocket.bind(backPort, function (err) {
        console.log('bound', backPort);
    });
}

createForwarderDevice(frontPort, backPort);

createClient(frontPort);
createWorker(backPort);   