'use strict';

var attention = require('./build/Release/attention');

var Attention = function(input) {
  if (!(this instanceof Attention)) {
    return new Attention(input);
  }
  this.options = {
    swatches: 10
  };
  if (typeof input === 'string') {
    this.options.file = input;
  } else if (typeof input === 'object' && input instanceof Buffer) {
    this.options.buffer = input;
  } else {
    throw new Error('Unsupported input');
  }
  return this;
};
module.exports = Attention;

/*
  Number of colour swatches to return
*/
Attention.prototype.swatches = function(swatches) {
  if (typeof swatches === 'number' && !Number.isNaN(swatches) && swatches % 1 === 0 && swatches > 0 && swatches <= 4096) {
    this.options.swatches = swatches;
  } else {
    throw new Error('Invalid number of swatches (1 - 4096)' + swatches);
  }
  return this;
};

/*
  Find the most salient region in an image
*/
Attention.prototype.region = function(cropOptions, callback) {
  if (typeof callback === 'function') {
    this.options.cropOptions = cropOptions;
    attention.region(this.options, callback);
  } else if (typeof cropOptions === 'function' && typeof callback === 'undefined' ) {
    callback = cropOptions;
    attention.region(this.options, callback);
  } else {
    throw new Error('Missing a callback function');
  }
  return this;
};

/*
  Find the focal point of an image
*/
Attention.prototype.point = function(callback) {
  if (typeof callback === 'function') {
    attention.point(this.options, callback);
  } else {
    throw new Error('Missing a callback function');
  }
  return this;
};

/*
  Find the most dominant colours in an image
*/
Attention.prototype.palette = function(callback) {
  if (typeof callback === 'function') {
    attention.palette(this.options, callback);
  } else {
    throw new Error('Missing a callback function');
  }
  return this;
};
