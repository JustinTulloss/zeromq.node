
var zmq = require('../../')
  , sock = zmq.socket('dealer')
  , http = require('http')
  , Stream = require('stream')

var server = http.createServer(function(req, res){
  // res.writeHead(200, req.headers);
  // req.on('data', function(chunk){
  //   res.write(chunk);
  // }).on('end', function(){
  //   res.end();
  // });

  console.log(req.method, req.url);
  var body = 'Hello';
  res.writeHead(200, { 'Content-Length': body.length });
  res.write(body);
  res.end();
});

var socks = {};

sock.on('message', function(envelope, id, type, data){
  var id = id.toString()
    , type = type.toString()
    , req
    , res;

  console.log('%s %s', id, type);

  switch (type) {
    case 'request':
      req = new Stream;
      res = new Stream;
      res.socket = req;

      res.writeHead = function(status, headers){
        var obj = JSON.stringify({ status: status, header: headers });
        sock.send([envelope, id, 'response', obj]);
      };

      res.write = function(data){
        sock.send([envelope, id, 'data', data]);
      };

      res.end = function(){
        sock.send([envelope, id, 'end']);
      };

      socks[id] = req;
      data = JSON.parse(data.toString());
      req.url = data.url;
      req.method = data.method;
      req.headers = data.header;
      // console.log('%s : %s "%s"', id, req.method, req.url);
      server.emit('request', req, res);
      break;
    case 'data':
      // console.log();
      // console.log('data');
      // console.log(data.toString());
      req = socks[id];
      req.emit('data', data);
      break;
    case 'end':
      // console.log('end');
      req = socks[id];
      req.emit('end');
      delete socks[id];
      break;
  }
});

sock.connect('tcp://127.0.0.1:5001');
console.log('app connected to :5001');