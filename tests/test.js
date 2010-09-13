var vows = require('vows'),
    assert = require('assert'),
    sys = require('sys'),
    zeromq = require('zeromq');

vows.describe('ZeroMQ')
.addBatch({
    'A context': {
        topic: function() { this.callback(null, new zeromq.Context())},
        'has a close method': function(c) {
            assert.isFunction(c.close);
        },
        'can be closed': function(c) {
            assert.doesNotThrow(function() {
                c.close();
            });
        }
    }
})
.addBatch({
    'A socket': {
        topic: function() {
            var c = new zeromq.Context();
            var s = new zeromq.Socket(c, zeromq.Socket.ZMQ_REQ);
            this.c = c;
            this.callback(null, s);
        },
        'after successful open': {
            topic: function(s) {
                this.callback(null, s);
            },
            'can connect': function(s) {
                assert.doesNotThrow(function() {
                    s.connect('tcp://127.0.0.1:5554');
                });
            },
            'dies on bad host': function(s) {
                //TODO: this just hangs, need to fix that behavior
                //assert.throws(function() {s.connect('tcp://127.0.0.1:8398'); });
            },
            'after successful connect': {
                topic: function(s) {
                    this.callback(null, s);
                },
                'can send': function(s) {
                    assert.doesNotThrow(function() {
                        s.send("hey there!");
                    });
                },
                'can set highwater mark': function(s) {
                    assert.doesNotThrow(function() {
                        s.highwaterMark = 10;
                    });
                    assert.equal(s.highwaterMark, 10);
                },
                'after successful `send`': {
                    topic: function(s) {
                        var self = this;
                        s.on('receive', function(data) {
                            self.callback(null, data);
                        });
                    },
                    'does receive data': function(data) {
                        assert.isString(data);
                    },
                    'after receive': {
                        topic: function(data, s) {
                            this.callback(null, s);
                        },
                        'can be closed': function(s) {
                            assert.doesNotThrow(function() {
                                s.close();
                            });
                            this.c.close();
                        }
                    }
                }
            }
        }
    }
}).export(module);
