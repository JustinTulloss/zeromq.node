var zmq = require('..')
  , should = require('should');

describe('proxy', function() {
  it('should be a function off the module namespace', function() {
    zmq.proxy.should.be.a.Function;
  });
});