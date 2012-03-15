
var http = require('http');

var server = http.createServer(function(req, res){
  // res.writeHead(200, req.headers);
  // req.on('data', function(chunk){
  //   res.write(chunk);
  // }).on('end', function(){
  //   res.end();
  // });

  var body = 'Hello';
  res.writeHead(200, { 'Content-Length': body.length });
  res.write(body);
  res.end();
});

server.listen(3001);