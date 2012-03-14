
var zmq = require('../../')
  , sock = zmq.socket('dealer');

sock.on('message', function(envelope, id, type, json){
  var id = id.toString()
    , type = type.toString()
    , req;

  switch (type) {
    case 'request':
      req = JSON.parse(json.toString());
      break;
    case 'data':
      break;
    case 'end':
      break;
  }
});

sock.connect('tcp://127.0.0.1:5001');
console.log('app connected to :5001');