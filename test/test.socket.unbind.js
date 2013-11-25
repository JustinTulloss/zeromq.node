var zmq = require('../')
  , should = require('should');

if (!zmq.ZMQ_CAN_UNBIND) {
  return;
}

var a = zmq.socket('dealer')
  , b = zmq.socket('dealer')
  , c = zmq.socket('dealer');

var message_count = 0;

a.bind('tcp://127.0.0.1:5420', function (err) {
  if (err) throw err;
  a.bind('tcp://127.0.0.1:5421', function (err) {
    if (err) throw err;
    b.connect('tcp://127.0.0.1:5420');
    b.send('Hello from b.');
    c.connect('tcp://127.0.0.1:5421');
    c.send('Hello from c.');
  });
});

a.on('unbind', function(addr) {
  if (addr === 'tcp://127.0.0.1:5420') {
    b.send('Error from b.');
    c.send('Messsage from c.');
    setTimeout(function () {
      c.send('Final message from c.');
    }, 100);
  }
});

a.on('message', function(msg) {
  message_count++;
  if (msg.toString() === 'Hello from b.') {
    a.unbind('tcp://127.0.0.1:5420');
  } else if (msg.toString() === 'Final message from c.') {
    message_count.should.equal(4);
    a.close();
    b.close();
    c.close();
  } else if (msg.toString() === 'Error from b.') {
    throw Error('b should have been unbound');
  }
});
