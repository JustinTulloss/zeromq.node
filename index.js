var semver = require('semver');

module.exports = require('./lib');
if (semver.gte(process.version, '0.10.0')) {
  module.exports.ReadableStream = require('./lib/stream').ReadableStream;
  module.exports.WritableStream = require('./lib/stream').WritableStream;
  module.exports.DuplexStream = require('./lib/stream').DuplexStream;
  module.exports.createStream = require('./lib/stream').createStream;
}