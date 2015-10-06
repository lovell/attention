'use strict';
/*jshint loopfunc: true */

var fs = require('fs');
var path = require('path');

var userDataDir = 'UserData';

var images = {};

var median = function(values) {
  values.sort(function(a,b) {
    return a - b;
  });
  var half = Math.floor(values.length / 2);
  if (values.length % 2) {
    return values[half];
  } else {
    return Math.floor((values[half - 1] + values[half]) / 2);
  }
};

// List of files
fs.readdirSync(userDataDir).forEach(function(file) {
  // Contents of file
  var lines = fs.readFileSync(path.join(userDataDir, file), {encoding: 'utf-8'}).split(/\r\n/);
  // First line = number of entries
  var entries = parseInt(lines[0], 10);
  // Verify number of entries
  if (entries !== 500) {
    throw new Error('Expecting 500 images in ' + file + ', found ' + entries);
  }
  // Keep track of which line we're on
  var linePos = 2;
  for (var i = 0; i < entries; i++) {
    // Get data for current image
    var filename = lines[linePos].replace(/\\/, path.sep);
    linePos = linePos + 2;
    var regions = lines[linePos].split('; ');
    linePos = linePos + 2;
    // Parse human-labelled regions for min/max coords
    var lefts = [], tops = [], rights = [], bottoms = [];
    regions.forEach(function(region) {
      if (region.indexOf(' ') !== -1) {
        var coords = region.split(' ');
        lefts.push(parseInt(coords[0], 10));
        tops.push(parseInt(coords[1], 10));
        rights.push(parseInt(coords[2], 10));
        bottoms.push(parseInt(coords[3], 10));
      }
    });
    // Add image
    images[filename] = {
      left: median(lefts),
      top: median(tops),
      right: median(rights),
      bottom: median(bottoms)
    };
  }
});

// Verify number of images found
var imageCount = Object.keys(images).length;
if (imageCount === 5000) {
  // Write output
  fs.writeFileSync('userData.json', JSON.stringify(images, null, 2));
} else {
  throw new Error('Expecting 5000 images, found ' + imageCount);
}
