// linked list for message batches

var Batch = require('./Batch');


function BatchList() {
  this.firstBatch = null;
  this.lastBatch = null;
  this.length = 0;
}

module.exports = BatchList;


BatchList.prototype.canSend = function () {
  return this.firstBatch ? this.firstBatch.isClosed : false;
};

BatchList.prototype.append = function (buf, flags, cb) {
  var batch = this.lastBatch;

  if (!batch || batch.isClosed) {
    batch = new Batch();

    if (this.lastBatch) {
      this.lastBatch.next = batch;
    }

    this.lastBatch = batch;

    if (!this.firstBatch) {
      this.firstBatch = batch;
    }

    this.length += 1;
  }

  batch.append(buf, flags, cb);
};

BatchList.prototype.fetch = function () {
  var batch = this.firstBatch;
  if (batch && batch.isClosed) {
    this.firstBatch = batch.next;
    this.length -= 1;
    return batch;
  }
  return undefined;
};

BatchList.prototype.restore = function (batch) {
  this.firstBatch = batch;
  this.length += 1;
};
