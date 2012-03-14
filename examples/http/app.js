
var zmq = require('../../')
  , sock = zmq.socket('dealer');

sock.on('message', function(envelope, id, type, json){
  id = id.toString();
  type = type.toString();
  var req = JSON.parse(json.toString());
  
});

sock.connect('tcp://127.0.0.1:5001');
console.log('app connected to :5001');