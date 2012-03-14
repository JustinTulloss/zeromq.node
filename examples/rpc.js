
/**
 * One server two clients
 */

var cluster = require('cluster')
  , zmq = require('../')
  , port = 'tcp://127.0.0.1:12345';

if (cluster.isMaster) {
  for (var i = 0; i < 2; i++) cluster.fork();

  
} else {
  var sock = zmq.socket('dealer');
  sock.connect(port);
}