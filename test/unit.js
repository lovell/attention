'use strict';

var fs = require('fs');
var path = require('path');
var assert = require('assert');

var attention = require('../');

var assertSwatch = function(swatch) {
  assert.strictEqual('number', typeof swatch.r);
  assert.strictEqual('number', typeof swatch.g);
  assert.strictEqual('number', typeof swatch.b);
  assert.strictEqual('string', typeof swatch.css);
  assert.strictEqual(true, swatch.r >= 0 && swatch.r <= 255);
  assert.strictEqual(true, swatch.g >= 0 && swatch.g <= 255);
  assert.strictEqual(true, swatch.b >= 0 && swatch.b <= 255);
  assert.strictEqual(7, swatch.css.length);
  assert.strictEqual(0, swatch.css.indexOf('#'));
};

// "Divided attention" by William Hemsley, public domain
// http://commons.wikimedia.org/wiki/File:William_Hemsley_Divided_attention.jpg
var fixtureFile = path.join(__dirname, 'divided-attention.jpg');
var fixtureData = fs.readFileSync(fixtureFile);

[fixtureFile, fixtureData].forEach(function(fixture) {

  attention(fixture).palette(function(err, palette) {
    if (err) throw err;
    assert.strictEqual('object', typeof palette);
    assert.strictEqual('object', typeof palette.swatches);
    assert.strictEqual(10, palette.swatches.length);
    palette.swatches.forEach(assertSwatch);
  });

  attention(fixture).swatches(1).palette(function(err, palette) {
    if (err) throw err;
    assert.strictEqual('object', typeof palette);
    assert.strictEqual('object', typeof palette.swatches);
    assert.strictEqual(1, palette.swatches.length);
    palette.swatches.forEach(assertSwatch);
    assert.strictEqual(96, palette.swatches[0].r);
    assert.strictEqual(73, palette.swatches[0].g);
    assert.strictEqual(true, ([58, 59].indexOf(palette.swatches[0].b) > -1), palette.swatches[0].b);
    assert.strictEqual(true, (['#60493a', '#60493b'].indexOf(palette.swatches[0].css) > -1), palette.swatches[0].css);
  });

  attention(fixture).swatches(1000).palette(function(err, palette) {
    if (err) throw err;
    assert.strictEqual('object', typeof palette);
    assert.strictEqual('object', typeof palette.swatches);
    assert.strictEqual('number', typeof palette.duration);
    assert.strictEqual(1000, palette.swatches.length);
    palette.swatches.forEach(assertSwatch);
  });

  attention(fixture).region(function(err, region) {
    if (err) throw err;
    assert.strictEqual('object', typeof region);
    assert.strictEqual('number', typeof region.top);
    assert.strictEqual('number', typeof region.left);
    assert.strictEqual('number', typeof region.bottom);
    assert.strictEqual('number', typeof region.right);
    assert.strictEqual('number', typeof region.duration);
    assert.strictEqual(495, region.width);
    assert.strictEqual(599, region.height);
  });

  attention(fixture).point(function(err, point) {
    if (err) throw err;
    assert.strictEqual('object', typeof point);
    assert.strictEqual('number', typeof point.x);
    assert.strictEqual('number', typeof point.y);
    assert.strictEqual('number', typeof point.duration);
    assert.strictEqual(495, point.width);
    assert.strictEqual(599, point.height);
  });

});
