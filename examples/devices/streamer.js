/*
 *
 * Forwarder device
 *
 */

var zmq = require('../../')
    , frontPort = 'tcp://127.0.0.1:12345'
    , backPort = 'tcp://127.0.0.1:12346';

function createClient (port) {
    var socket = zmq.socket('push');

    socket.identity = 'client' + process.pid;

    socket.connect(port);
    console.log('client connected!');

    setInterval(function() {
        var value = Math.floor(Math.random()*100);

        console.log(socket.identity + ': pushing ' + value);
        socket.send(value);
    }, 100);
};

function createWorker (port) {
    var socket = zmq.socket('pull');

    socket.identity = 'worker' + process.pid;

    socket.on('message', function(data) {
        console.log(socket.identity + ': pulled ' + data.toString());
    });

    socket.connect(port, function(err) {
        if (err) throw err;
        console.log('worker connected!');
    });
};

function createStreamerDevice(frontPort, backPort) {
    var frontSocket = zmq.socket('pull'),
        backSocket = zmq.socket('push');

    frontSocket.identity = 'sub' + process.pid;
    backSocket.identity = 'pub' + process.pid;

    frontSocket.bind(frontPort, function (err) {
        console.log('bound', frontPort);
    });

    frontSocket.on('message', function() {
        //pass to back
        console.log('forwarder: sending downstream', arguments[0].toString());
        backSocket.send(Array.prototype.slice.call(arguments));
    });

    backSocket.bind(backPort, function (err) {
        console.log('bound', backPort);
    });
}

createStreamerDevice(frontPort, backPort);

createClient(frontPort);
createWorker(backPort);   