
var http = require('http')
  , zmq = require('../../')
  , sock = zmq.socket('dealer');

var requests = {};

http.createServer(function(req, res){
  var id = Date.now() + Math.random();
  // console.log('%s : %s "%s"', id, req.method, req.url);

  requests[id] = res;

  var obj = {
    method: req.method,
    url: req.url,
    header: req.headers
  };

  var json = JSON.stringify(obj);

  sock.send([id, 'request', json]);

  req.on('data', function(chunk){
    sock.send([id, 'data', chunk]);
  });

  req.on('end', function(){
    sock.send([id, 'end']);
  });
}).listen(3000);

sock.on('message', function(id, type, data){
  var id = id.toString()
    , type = type.toString()
    , res = requests[id];

  switch (type) {
    case 'response':
      data = JSON.parse(data);
      res.writeHead(data.status, data.header);
      break;
    case 'data':
      res.write(data);
      break;
    case 'end':
      res.end();
      delete requests[id];
      break;
  }
});

sock.connect('tcp://127.0.0.1:5000');

console.log('HTTP server listening on :3000');
console.log('dealer connected to :5000');