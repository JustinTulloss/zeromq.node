/*
 *
 * One client two servers (round roobin)
 *
 */

var cluster = require('cluster')
  , zmq = require('../')
  , port = 'tcp://127.0.0.1:12345';

if (cluster.isMaster) {
  for (var i = 0; i < 2; i++) cluster.fork();

  cluster.on('death', function(worker) {
    console.log('worker ' + worker.pid + ' died');
  });

  //dealer = client

  var socket = zmq.socket('dealer');

  socket.identity = 'client' + process.pid;

  socket.bind(port, function(err) {
    if (err) throw err;
    console.log('bound!');

    setInterval(function() {
      var value = Math.floor(Math.random()*100);

      console.log(socket.identity + ': asking ' + value);
      socket.send(value);
    }, 100);


    socket.on('message', function(data) {
      console.log(socket.identity + ': answer data ' + data);
    });
  });
} else {
  //router = server

  var socket = zmq.socket('router');

  socket.identity = 'server' + process.pid;

  socket.connect(port);
  console.log('connected!');

  socket.on('message', function(envelope, data) {
    console.log(socket.identity + ': received ' + envelope + ' - ' + data.toString());
    socket.send([envelope, data * 2]);
  });
}