'use strict';

var path = require('path');
var attention = require('../');

var distance = function(expectedX, expectedY, actualX, actualY) {
  return Math.sqrt(
    Math.pow(Math.abs(actualX - expectedX), 2) + Math.pow(Math.abs(actualY - expectedY), 2)
  );
};

var userData = require('./userData.json');

var count = 0;
var totalDistance = 0;

Object.keys(userData).forEach(function(file) {
  var filename = path.join(__dirname, 'Image', file);
  attention(filename).region(function(err, region) {
    if (err) {
      console.log('Skipped ' + filename + ' because ' + err);
    } else {
      var distTopLeft = distance(userData[file].left, userData[file].top, region.left, region.top);
      var distBottomRight = distance(userData[file].right, userData[file].bottom, region.right, region.bottom);
      totalDistance = totalDistance + distTopLeft + distBottomRight;
      count++;
      console.log('Processed ' + filename + ' in ' + region.duration + 'ms, average distance now ' + (totalDistance / count).toFixed(2));
    }
  });
});
