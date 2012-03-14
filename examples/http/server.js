
var http = require('http')
  , zmq = require('../../')
  , sock = zmq.socket('dealer');

http.createServer(function(req, res){
  var id = Date.now() + Math.random();

  var obj = {
    method: req.method,
    url: req.url,
    header: req.headers
  };

  var json = JSON.stringify(obj);

  sock.send([id, 'request', json]);
}).listen(3000);

console.log('HTTP server listening on :3000');