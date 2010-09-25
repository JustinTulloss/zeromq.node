var binding = exports.capi = require('./binding');

var context_ = null;
var defaultContext = function() {
  if (context_ !== null)
    return context_;

  context_ = new binding.Context();
  process.on('exit', function() {
    context_.close();
    context_ = null;
  });

  return context_;
};

exports.createSocket = function(type) {
  var ctx = defaultContext();
  return new binding.Socket(ctx, type);
};
