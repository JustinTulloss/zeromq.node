/*
 *
 * Queue device
 *
 */

var zmq = require('../../')
    , frontPort = 'tcp://127.0.0.1:12345'
    , backPort = 'tcp://127.0.0.1:12346';

function createClient (port) {
    var socket = zmq.socket('req');

    socket.identity = 'client' + process.pid;

    socket.on('message', function(data) {
        console.log(socket.identity + ': answer data ' + data);
    });

    socket.connect(port);
    console.log('client connected!');

    setInterval(function() {
        var value = Math.floor(Math.random()*100);

        console.log(socket.identity + ': asking ' + value);
        socket.send(value);
    }, 100);
};

function createServer (port) {
    var socket = zmq.socket('rep');

    socket.identity = 'server' + process.pid;

    socket.on('message', function(data) {
        console.log(socket.identity + ': received ' + data.toString());
        socket.send(data * 2);
    });

    socket.connect(port, function(err) {
        if (err) throw err;
        console.log('server connected!');
    });
};

function createQueueDevice(frontPort, backPort) {
    var frontSocket = zmq.socket('router'),
        backSocket = zmq.socket('dealer');

    frontSocket.identity = 'router' + process.pid;
    backSocket.identity = 'dealer' + process.pid;
    
    frontSocket.bind(frontPort, function (err) {
        console.log('bound', frontPort);
    });
    
    frontSocket.on('message', function() {
        //pass to back
        console.log('router: sending to server', arguments[0].toString(), arguments[2].toString());
        backSocket.send(Array.prototype.slice.call(arguments));
    });

    backSocket.bind(backPort, function (err) {
        console.log('bound', backPort);
    });
    
    backSocket.on('message', function() {
        //pass to front
        console.log('dealer: sending to client', arguments[0].toString(), arguments[2].toString());
        frontSocket.send(Array.prototype.slice.call(arguments));
    });
}

createQueueDevice(frontPort, backPort);

createClient(frontPort);
createServer(backPort);   