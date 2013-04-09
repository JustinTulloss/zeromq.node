
var fs = require('fs')
  , files = process.argv.slice(2)
  , exec = require('child_process').exec
  , Batch = require('batch')
  , batch = new Batch
  , start = new Date;

console.log();

files.forEach(function(file){
  batch.push(function(done){
    exec('node --expose-gc ' + file, function(err){
      if (err) return done(err);
      console.log('  \033[32m✓\033[0m \033[90m%s\033[0m', file);
      done();
    });
  });
});

batch.end(function(err){
  if (err) throw err;
  var duration = new Date - start;
  console.log('\n  \033[32m✓\033[0m \033[90m%s\033[0m', 'completed in ' + duration + 'ms\n');
});