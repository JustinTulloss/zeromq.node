
var zmq = require('../../')
  , sock = zmq.socket('dealer');

sock.on('message', function(envelope, id, type, data){
  var id = id.toString()
    , type = type.toString()
    , req;

  switch (type) {
    case 'request':
      req = JSON.parse(data.toString());
      console.log('%s : %s "%s"', id, req.method, req.url);
      break;
    case 'data':
      console.log('data');
      console.log(data.toString());
      break;
    case 'end':
      console.log('end');
      break;
  }
});

sock.connect('tcp://127.0.0.1:5001');
console.log('app connected to :5001');