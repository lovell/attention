#include <vips/vips8>

#include "mask.h"

/*
  Generate mask using Sobel operators
*/
vips::VImage Mask::Edges(vips::VImage input) {

  // Convert image to greyscale and apply small blur
  vips::VImage grey = input.colourspace(VIPS_INTERPRETATION_B_W).gaussblur(2.0);

  // Create horizontal operator
  vips::VImage sobelX = vips::VImage::new_matrixv(3, 3,
    -1.0, 0.0, 1.0,
    -2.0, 0.0, 2.0,
    -1.0, 0.0, 1.0);
  // Create vertical operator
  vips::VImage sobelY = vips::VImage::new_matrixv(3, 3,
    1.0, 2.0, 1.0,
    0.0, 0.0, 0.0,
    -1.0, -2.0, -1.0);

  // Apply Sobel operators
  vips::VImage sobelFiltered = grey.conv(sobelX) + grey.conv(sobelY);

  // Halve range to stay within 0-255
  sobelFiltered = (sobelFiltered / 2).cast(VIPS_FORMAT_UCHAR);

  // Calculate value threshold at which we can discard 85% of pixels
  const double sobelThreshold = static_cast<double>(sobelFiltered.percent(85.0));

  // Remove pixels below threshold
  return sobelFiltered >= sobelThreshold;
};

/*
  Generate mask of prominent colours
*/
vips::VImage Mask::Colours(vips::VImage input) {

  // Apply Gaussian blur
  vips::VImage blurred = input.gaussblur(1.0);

  // Generate image containing the average colour
  const int shrunkWidth = input.width();
  const int shrunkHeight = input.height();
  vips::VImage averageColour = blurred.shrink(shrunkWidth, shrunkHeight).colourspace(VIPS_INTERPRETATION_LAB).zoom(shrunkWidth, shrunkHeight);

  // Calculate Delta-E 2000 distance to the average in the LAB colour space
  vips::VImage averageDelta = blurred.colourspace(VIPS_INTERPRETATION_LAB).dE00(averageColour);

  // Calculate value threshold at which we can discard 85% of pixels
  const double colourThreshold = static_cast<double>(averageDelta.percent(85.0));

  // Remove pixels below threshold
  return averageDelta >= colourThreshold;
};

/*
  Generate combined saliency mask
*/
vips::VImage Mask::Saliency(vips::VImage input) {
  // Keep pixels that appear in both masks and remove noise with 5x5 median filter
  return (Edges(input) & Colours(input)).rank(5, 5, 12);
};
