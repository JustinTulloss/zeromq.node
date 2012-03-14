
var zmq = require('../../')
  , router = zmq.socket('router')
  , dealer = zmq.socket('dealer');

router.on('message', function(envelope, id){
  var args = Array.apply(null, arguments);
  dealer.send(args);
});

dealer.on('message', function(envelope, id){
  var args = Array.apply(null, arguments);
  router.send(args);
});

router.bind('tcp://127.0.0.1:5000');
dealer.bind('tcp://127.0.0.1:5001');
console.log('router bound to :5000');
console.log('dealer bound to :5001');