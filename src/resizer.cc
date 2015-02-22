#include <vips/vips8>

#include "resizer.h"

/*
  Resize the longest edge of the image to longestEdge pixels
*/
ImageResizer::ImageResizer(const int longestEdge) {
  this->longestEdge = longestEdge;
  this->originalWidth = 0;
  this->originalHeight = 0;
  this->ratio = 1.0;
};

/*
  Load the image from a file
*/
vips::VImage ImageResizer::FromFile(std::string file) {
  return LoadAndResize(file, NULL, 0);
};

/*
  Load the image from a buffer
*/
vips::VImage ImageResizer::FromBuffer(void *buffer, size_t bufferLength) {
  return LoadAndResize(NULL, buffer, bufferLength);
};

/*
  Delete input char[] buffer and notify V8 of memory deallocation
  Used as the callback function for the "postclose" signal
*/
static void DeleteBuffer(VipsObject *object, char *buffer) {
  if (buffer != NULL) {
    delete[] buffer;
  }
};

/*
  All the resize logic
*/
vips::VImage ImageResizer::LoadAndResize(std::string file, void *buffer, size_t bufferLength) {
  // Input
  vips::VImage input;
  std::string loader;
  if (buffer != NULL && bufferLength > 0) {
    // From buffer
    loader = vips_foreign_find_load_buffer(buffer, bufferLength);
    input = vips::VImage::new_from_buffer(buffer, bufferLength, NULL);
    // Listen for "postclose" signal to delete input buffer
    g_signal_connect(input.get_image(), "postclose", G_CALLBACK(DeleteBuffer), buffer);
  } else {
    // From file
    loader = vips_foreign_find_load(file.c_str());
    input = vips::VImage::new_from_file(file.c_str());
  }

  // Store original image dimensions
  this->originalWidth = input.width();
  this->originalHeight = input.height();

  // Which edge is the longest?
  const int longestEdge = std::max(this->originalWidth, this->originalHeight);

  // Calculate float shrink ratio of target vs actual longest edge
  this->ratio = static_cast<double>(this->longestEdge) / static_cast<double>(longestEdge);
  double affineRatio = ratio;

  // Shrink-on-load JPEG
  if (loader == "VipsForeignLoadJpegFile" && longestEdge >= 2 * this->longestEdge) {
    int shrinkOnLoad = 2;
    if (longestEdge >= 8 * this->longestEdge) {
      shrinkOnLoad = 8;
      affineRatio = affineRatio * 8.0;
    } else if (longestEdge >= 4 * this->longestEdge) {
      shrinkOnLoad = 4;
      affineRatio = affineRatio * 4.0;
    } else {
      affineRatio = affineRatio * 2.0;
    }
    if (buffer != NULL && bufferLength > 0) {
      // From buffer
      input = vips::VImage::new_from_buffer(
        buffer, bufferLength, NULL,
        vips::VImage::option()->set("shrink", shrinkOnLoad)
      );
    } else {
      // From file
      input = vips::VImage::new_from_file(
        file.c_str(),
        vips::VImage::option()->set("shrink", shrinkOnLoad)
      );
    }
  }

  // Import embedded colour profile, if any
  if (input.get_typeof(VIPS_META_ICC_NAME) > 0) {
    input = input.icc_import(vips::VImage::option()->set("embedded", TRUE));
  }

  // Shrink via affine reduction
  return input.resize(affineRatio, vips::VImage::option()->set("interpolate",
    vips::VInterpolate::new_from_name("bilinear")));
};
