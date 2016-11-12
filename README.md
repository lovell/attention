# attention

This repo is no longer maintained or supported, please use [sharp](https://github.com/lovell/sharp) instead.

```javascript
sharp(input)
  .resize(width, height)
  .crop(sharp.strategy.attention)
  ...
```

This was an experimental Node.js module to help you discover which parts of an image draw the focus of human attention.

You could use this information to modify an image or its surroundings for maximum effect.

## Features

### Dominant Palette

The dominant palette of an image contains the colours that, if you had to reduce the palette to a fixed number of colours, would still represent the image most clearly to human eyes.

This should not be confused with the simpler "average" colour, often a washed-out brown or grey with a low chroma value.

A typical use case for this feature is to find the most appropriate colours for text and design elements that will appear near a given image, enhancing its appearance.

### Salient Region

Human eyes are drawn to edges and areas with a high relative contrast.

This module creates and combines two saliency masks to calculate which region of an image contains edges and stands out more with respect to brightness and colour.

1. Edge detection using the [Sobel operator](http://en.wikipedia.org/wiki/Sobel_operator).
2. Distance from the "average" colour using the [Delta-E 2000](http://en.wikipedia.org/wiki/Color_difference#CIEDE2000) formula.

The [MSRA Salient Object Database](http://research.microsoft.com/en-us/um/people/jiansun/salientobject/salient_object.htm) was used to discover the best thresholds for each mask.

A typical use case for this feature is to automatically remove less relevant parts of an image when cropping to fit fixed dimensions.

### Focal Point

This is the most dominant or "heaviest" point within the Salient Region.

It is calculated by finding the midpoint of the total pixel count in each of the horizontal and vertical directions of the saliency mask.

A typical use case for this feature is to automatically centre the most relevant part of an image when cropping/hiding the top and bottom or left and right.

## Installation

	npm install attention

### Prerequisites

* Node.js v0.10+
* [libvips](https://github.com/jcupitt/libvips) v7.42.2+

## Usage

```javascript
attention('input.jpg')
  .swatches(1)
  .palette(function(err, palette) {
    palette.swatches.forEach(function(swatch) {
      console.dir(swatch);
    });
  });
```
outputs:
```
{r: 96, g: 73, b: 58, css: '#60493a', ...}
```

```javascript
attention('input.jpg')
  .region(function(err, region) {
    console.dir(region);
  });
```
outputs:
```
{ top: 29, left: 17, bottom: 555, right: 485, ... }
```

```javascript
attention('input.jpg')
  .point(function(err, point) {
    console.dir(point);
  });
```
outputs:
```
{ x: 293, y: 117, ... }
```

## API

### attention([input])

Constructor to which further methods are chained. `input`, if present, can be one of:

* Buffer containing JPEG, PNG, WebP or TIFF image data, or
* String containing the filename of an image, with most major formats supported.

### palette(callback)

Calculates the dominant palette for the input image, with the number of colours limited to the value passed to `swatches()`.

`callback` gets the arguments `(err, palette)` where `palette` has the attributes:

* `swatches`: an array of sRGB colours split into integral (0-255) red, green, and blue components as well as a handy `css` String in hex notation.
* `duration`: the length of time taken to find the palette, in milliseconds.

### swatches(count)

Set `count` to the number of distinct colour swatches required, defaulting to a value of 10.

### region(callback)

Calculates the most salient region of the input image.

`callback` gets the arguments `(err, region)` where `region` contains the edges of the salient region in pixels from the top-left corner of the input:

* `top`
* `left`
* `bottom`
* `right`

The `region` Object also contains:

* `width`: the width of the input image.
* `height`: the height of the input image.
* `duration`: the length of time taken to find the salient region, in milliseconds.

### point(callback)

Calculates the focal point of the input image.

`callback` gets the arguments `(err, point)` where `point` contains the coordinates of the focal point from the top-left corner of the input:

* `x`
* `y`

The `point` Object also contains:

* `width`: the width of the input image.
* `height`: the height of the input image.
* `duration`: the length of time taken to find the focal point, in milliseconds.

## Thanks

This module uses John Cupitt's [libvips](https://github.com/jcupitt/libvips) and its marvellous new (2015) C++ API.

It also includes the source of Dennis Ranke's [ExoQuant](https://github.com/exoticorn/exoquant) colour quantisation library as a submodule, used under the terms of the MIT Licence.

## Licence

Copyright 2015 Lovell Fuller

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0.html)

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
