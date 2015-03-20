var semver = require('semver');

module.exports = require('./lib');
if (semver.gte(process.version, '0.10.0')) {
  module.exports.ReadableStream = require('./lib/readable_stream').ReadableStream;
  module.exports.createReadableStream = require('./lib/readable_stream').createReadableStream;
  module.exports.WritableStream = require('./lib/writable_stream').WritableStream;
  module.exports.createWritableStream = require('./lib/writable_stream').createWritableStream;
};