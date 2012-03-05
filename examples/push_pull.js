/*
 *
 * Pipeline
 *
 */

var cluster = require('cluster'),
    zeromq = require('zmq'),
    port = 'tcp://127.0.0.1:12345';

if (cluster.isMaster) {
    //Fork workers.
    for (var i = 0; i < 2; i++) {
        cluster.fork();
    }

    cluster.on('death', function(worker) {
        console.log('worker ' + worker.pid + ' died');
    });
    
    //push = upstream
    
    var socket = zeromq.socket('push');

    socket.identity = 'upstream' + process.pid;

    socket.bind(port, function(err) {
        if (err) throw err;
        console.log('bound!');

        setInterval(function() {
            var date = new Date();

            console.log(socket.identity + ': sending data ' + date.toString());
            socket.send(date.toString());
        }, 500);
    });
} else {
    //pull = downstream
    
    var socket = zeromq.socket('pull');

    socket.identity = 'downstream' + process.pid;
    
    socket.connect(port);
    console.log('connected!');

    socket.on('message', function(data) {
        console.log(socket.identity + ': received data ' + data.toString());
    });
}