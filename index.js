module.exports = process.versions.nsolid
  ? require('_zmq')
  : require('./lib');
