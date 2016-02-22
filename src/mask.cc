#include <vips/vips8>

#include "mask.h"

/*
  Generate mask using Sobel operators
*/
vips::VImage Mask::Edges(vips::VImage input) {

  // Convert image to greyscale and apply small blur
  vips::VImage grey = input.colourspace(VIPS_INTERPRETATION_B_W).gaussblur(2.0);

  // Create Sobel operator
  vips::VImage sobel = vips::VImage::new_matrixv(3, 3,
    -1.0, 0.0, 1.0,
    -2.0, 0.0, 2.0,
    -1.0, 0.0, 1.0);

  // Apply horizontal and vertical Sobel operators
  vips::VImage sobelFiltered =
    (grey.conv(sobel) - 128).abs() +
    (grey.conv(sobel.rot90()) - 128).abs();

  // Calculate value threshold at which we can keep 15% of pixels
  const double sobelThreshold = static_cast<double>(sobelFiltered.percent(15.0));

  // Keep pixels below threshold
  return sobelFiltered < sobelThreshold;
};

/*
  Generate mask of prominent colours
*/
vips::VImage Mask::Colours(vips::VImage input) {

  // Apply Gaussian blur
  vips::VImage lab = input.colourspace(VIPS_INTERPRETATION_LAB);

  // Generate image containing the average colour
  const int shrunkWidth = lab.width();
  const int shrunkHeight = lab.height();
  vips::VImage averageColour = lab.shrink(shrunkWidth, shrunkHeight).zoom(shrunkWidth, shrunkHeight);

  // Calculate Delta-E 2000 distance to the average in the LAB colour space
  vips::VImage averageDelta = lab.dE00(averageColour);

  // Calculate value threshold at which we can discard 85% of pixels
  const double colourThreshold = static_cast<double>(averageDelta.percent(85.0));

  // Remove pixels below threshold
  return averageDelta >= colourThreshold;
};

/*
  Generate combined saliency mask
*/
vips::VImage Mask::Saliency(vips::VImage input) {
  // Keep pixels that appear in both masks
  return (Edges(input) & Colours(input));
};
