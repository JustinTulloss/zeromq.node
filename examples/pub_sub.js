/*
 *
 * Publisher subscriber pattern
 *
 */

var cluster = require('cluster'),
    zeromq = require('zmq'),
    port = 'tcp://127.0.0.1:12345';

if (cluster.isMaster) {
    //Fork subscribers.
    for (var i = 0; i < 2; i++) {
        cluster.fork();
    }

    cluster.on('death', function(worker) {
        console.log('worker ' + worker.pid + ' died');
    });
    
    //publisher = send only
    
    var socket = zeromq.socket('pub');

    socket.identity = 'publisher' + process.pid;
    
    var stocks = ['AAPL', 'GOOG', 'YHOO', 'MSFT', 'INTC'];

    socket.bind(port, function(err) {
        if (err) throw err;
        console.log('bound!');
        
        setInterval(function() {
            var symbol = stocks[Math.floor(Math.random()*stocks.length)],
                value = Math.random()*1000;

            console.log(socket.identity + ': sent ' + symbol + ' ' + value);
            socket.send(symbol + ' ' + value);
        }, 100);
    });
} else {
    //subscriber = receive only
    
    var socket = zeromq.socket('sub');

    socket.identity = 'subscriber' + process.pid;
    
    socket.connect(port);
    
    socket.subscribe('AAPL');
    socket.subscribe('GOOG');

    console.log('connected!');

    socket.on('message', function(data) {
        console.log(socket.identity + ': received data ' + data.toString());
    });
}