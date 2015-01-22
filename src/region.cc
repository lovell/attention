#include <vips/vips8>

#include "nan.h"
#include "region.h"

struct RegionBaton {
  // Input
  std::string file;
  void* buffer;
  size_t bufferLength;

  // Output
  std::string err;
  int top;
  int left;
  int bottom;
  int right;
  int duration;

  RegionBaton():
    bufferLength(0),
    top(-1),
    left(-1),
    bottom(-1),
    right(-1),
    duration(-1) {}
};

class RegionWorker : public NanAsyncWorker {

 public:
  RegionWorker(NanCallback *callback, RegionBaton *baton) : NanAsyncWorker(callback), baton(baton) {}
  ~RegionWorker() {}

  void Execute() {
    using vips::VImage;
    using vips::VError;

    try {
      GTimer *timer = g_timer_new();

      // Input
      VImage image;
      std::string loader;
      if (baton->bufferLength > 0) {
        // From buffer
        loader = vips_foreign_find_load_buffer(baton->buffer, baton->bufferLength);
        image = VImage::new_from_buffer(baton->buffer, baton->bufferLength, NULL);
      } else {
        // From file
        loader = vips_foreign_find_load(baton->file.c_str());
        image = VImage::new_from_file(baton->file.c_str());
      }

      // Store original image dimensions
      const int width = image.width();
      const int height = image.height();

      // Reduce the longest edge
      const int longestEdge = std::max(width, height);
      const int longestEdgeTargetLength = 320;

      // Calculate float shrink ratio
      double shrink = static_cast<double>(longestEdgeTargetLength) / static_cast<double>(longestEdge);

      // Shrink-on-load JPEG
      if (loader == "VipsForeignLoadJpegFile" && longestEdge >= 2 * longestEdgeTargetLength) {
        int shrinkOnLoad = 2;
        if (longestEdge >= 8 * longestEdgeTargetLength) {
          shrinkOnLoad = 8;
          shrink = shrink * 8.0;
        } else if (longestEdge >= 4 * longestEdgeTargetLength) {
          shrinkOnLoad = 4;
          shrink = shrink * 4.0;
        } else {
          shrink = shrink * 2.0;
        }
        if (baton->bufferLength > 0) {
          // From buffer
          image = VImage::new_from_buffer(baton->buffer, baton->bufferLength, NULL, VImage::option()->set("shrink", shrinkOnLoad));
        } else {
          // From file
          image = VImage::new_from_file(baton->file.c_str(), VImage::option()->set("shrink", shrinkOnLoad));
        }
      }

      // Import embedded colour profile, if any
      if (image.get_typeof(VIPS_META_ICC_NAME) > 0) {
        image = image.icc_import(VImage::option()->set("embedded", TRUE));
      }

      // Shrink via affine reduction
      image = image.resize(shrink);
      const int shrunkWidth = image.width();
      const int shrunkHeight = image.height();

      // Mask 1: edge detection using Sobel operators

      // Convert image to greyscale and apply small blur
      VImage grey = image.colourspace(VIPS_INTERPRETATION_B_W).gaussblur(2.0);
      // Create horizontal operator
      VImage sobelX = VImage::new_matrixv(3, 3,
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0);
      // Create vertical operator
      VImage sobelY = VImage::new_matrixv(3, 3,
        1.0, 2.0, 1.0,
        0.0, 0.0, 0.0,
        -1.0, -2.0, -1.0);
      // Apply Sobel operators
      VImage sobelFiltered = grey.conv(sobelX) + grey.conv(sobelY);
      // Halve range to stay within 0-255
      sobelFiltered = (sobelFiltered / 2).cast(VIPS_FORMAT_UCHAR);
      // Calculate 85th percentile threshold
      double sobelThreshold = static_cast<double>(sobelFiltered.percent(85));
      // Remove pixels below threshold and remove noise with 5x5 median filter
      VImage edgeMask = sobelFiltered.relational_const({sobelThreshold}, VIPS_OPERATION_RELATIONAL_MOREEQ).rank(5, 5, 12);

      // Mask 2: prominent colours using Delta-E 2000

      // Generate image containing the average colour
      image = image.gaussblur(1.0);
      VImage averageColour = image.shrink(shrunkWidth, shrunkHeight).colourspace(VIPS_INTERPRETATION_LAB).zoom(shrunkWidth, shrunkHeight);
      // Calculate Delta-E 2000 distance to the average in the LAB colour space
      VImage averageDelta = image.colourspace(VIPS_INTERPRETATION_LAB).dE00(averageColour);
      // Calculate percentile threshold
      double colourThreshold = static_cast<double>(averageDelta.percent(88));
      // Remove pixels below threshold
      VImage colourMask = averageDelta.relational_const({colourThreshold}, VIPS_OPERATION_RELATIONAL_MOREEQ);

      // Mask 1+2: Keep pixels that appear in both masks and remove noise with 5x5 median filter
      VImage mask = edgeMask.boolean(colourMask, VIPS_OPERATION_BOOLEAN_AND).rank(5, 5, 12);

      // Measure distance to first non-zero pixel along top and left edges
      VImage profileLeft, profileTop = mask.profile(&profileLeft);
      const int top = floor(shrink * profileTop.min());
      const int left = floor(shrink * profileLeft.min());

      // Verify mask is non-empty
      if (top < height && left < width) {
        // Measure distance to first non-zero pixel along bottom and right edges
        VImage profileRight, profileBottom = mask.rot(VIPS_ANGLE_D180).profile(&profileRight);
        const int bottom = height - 1 - floor(shrink * profileBottom.min());
        const int right = width - 1 - floor(shrink * profileRight.min());

        // Verify area of region is greater than 1/16 of original image area
        const int regionArea = (bottom - top) * (right - left);
        if (regionArea > width * height / 16.0) {
          // Store results
          baton->top = top;
          baton->left = left;
          baton->bottom = bottom;
          baton->right = right;
        } else {
          baton->err = "Salient region was too small";
        }
      } else {
        baton->err = "Could not determine salient region";
      }

      // Store duration
      baton->duration = ceil(g_timer_elapsed(timer, NULL) * 1000.0);
      g_timer_destroy(timer);

    } catch (VError err) {
      baton->err = err.what();
    }

    // Clean up libvips' per-request data and threads
    vips_error_clear();
    vips_thread_shutdown();
  }

  void HandleOKCallback () {
    NanScope();

    v8::Handle<v8::Value> argv[2] = { NanNull(), NanNull() };
    if (!baton->err.empty()) {
      // Error
      argv[0] = v8::Exception::Error(NanNew<v8::String>(baton->err.data(), baton->err.size()));
    } else {
      // Region Object
      v8::Local<v8::Object> region = NanNew<v8::Object>();
      region->Set(NanNew<v8::String>("top"), NanNew<v8::Integer>(baton->top));
      region->Set(NanNew<v8::String>("left"), NanNew<v8::Integer>(baton->left));
      region->Set(NanNew<v8::String>("bottom"), NanNew<v8::Integer>(baton->bottom));
      region->Set(NanNew<v8::String>("right"), NanNew<v8::Integer>(baton->right));
      region->Set(NanNew<v8::String>("duration"), NanNew<v8::Integer>(baton->duration));
      argv[1] = region;
    }
    delete baton;

    // Return to JavaScript
    callback->Call(2, argv);
  }

 private:
  RegionBaton *baton;
};

NAN_METHOD(region) {
  NanScope();
  RegionBaton *baton = new RegionBaton;

  // Parse options
  v8::Local<v8::Object> options = args[0]->ToObject();
  if (options->Get(NanNew<v8::String>("buffer"))->IsObject()) {
    // Input is a Buffer
    v8::Local<v8::Object> buffer = options->Get(NanNew<v8::String>("buffer"))->ToObject();
    // Take a copy to avoid problems with V8 heap compaction
    baton->bufferLength = node::Buffer::Length(buffer);
    baton->buffer = g_malloc(baton->bufferLength);
    memcpy(baton->buffer, node::Buffer::Data(buffer), baton->bufferLength);
    options->Set(NanNew<v8::String>("buffer"), NanNull());
  } else {
    // Input is a filename
    baton->file = *v8::String::Utf8Value(options->Get(NanNew<v8::String>("file"))->ToString());
  }

  // Join queue for worker thread
  NanCallback *callback = new NanCallback(args[1].As<v8::Function>());
  NanAsyncQueueWorker(new RegionWorker(callback, baton));

  NanReturnUndefined();
}
