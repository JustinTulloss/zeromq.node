var vows = require('vows'),
    assert = require('assert'),
    zeromq = require('../');

var suite = vows.describe('ZeroMQ');
suite.options.error = false;
suite.addBatch({
  'A reply socket': {
    topic: function() {
      return zeromq.createSocket('reply');
    },
    'can set the high water mark': function(rep) {
      rep.highWaterMark = 10;
      assert.equal(rep.highWaterMark, 10);
    },
    'after a successful open': {
      topic: function(rep) {
        rep.bind('inproc://1-1', this.callback);
      },
      'can be bound to an inproc address': function(bindErr) {
        assert.ifError(bindErr);
      },
      'and a request socket': {
        topic: function(bindErr, rep) {
          return zeromq.createSocket('request');
        },
        'can connect to the reply socket': function(req) {
          req.connect('inproc://1-1');
        },
        'after a successful connect': {
          topic: function(req, bindErr, rep) {
            rep.on('message', this.callback);
            req.send('foo');
          },
          'can send a request': function(reqMsg) {
            assert.equal(reqMsg, 'foo');
          },
          'after a successful request': {
            topic: function(reqMsg, req, bindErr, rep) {
              req.on('message', this.callback);
              rep.send('bar');
            },
            'can send a reply': function(repMsg) {
              assert.equal(repMsg, 'bar');
            },
            'after a successful reply': {
              topic: function(repMsg, reqMsg, req, bindErr, rep) {
                req.close();
                rep.close();
                this.callback(null);
              },
              'can close both sockets': function() {}
            }
          }
        }
      }
    }
  }
});
suite.export(module);
