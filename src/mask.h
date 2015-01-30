#ifndef SRC_MASK_H_
#define SRC_MASK_H_

class Mask {

public:

  /*
    Generate mask using Sobel operators
  */
  static vips::VImage Edges(vips::VImage input);
  
  /*
    Generate mask of prominent colours
  */
  static vips::VImage Colours(vips::VImage input);

  /*
    Generate combined saliency mask
  */
  static vips::VImage Saliency(vips::VImage input);
  
};

#endif  // SRC_MASK_H_
