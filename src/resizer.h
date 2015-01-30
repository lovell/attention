#ifndef SRC_RESIZER_H_
#define SRC_RESIZER_H_

class ImageResizer {

  int longestEdge;

  vips::VImage LoadAndResize(std::string file, void *buffer, size_t bufferLength);

public:
  /*
    Image dimensions
  */
  int originalWidth = 0;
  int originalHeight = 0;

  /*
    The ratio by which the image was shrunk to achieve longestEdge
  */
  double ratio = 1.0;

  /*
    Resize the longest edge of the image to longestEdge pixels
  */
  ImageResizer(int longestEdge);

  /*
    Load the image from a file
  */
  vips::VImage FromFile(std::string file);

  /*
    Load the image from a buffer
  */
  vips::VImage FromBuffer(void *buffer, size_t bufferLength);

};

#endif  // SRC_RESIZER_H_
