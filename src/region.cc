#include <vips/vips8>

#include "nan.h"
#include "region.h"
#include "resizer.h"
#include "mask.h"

struct RegionBaton {
  // Input
  void *buffer;
  size_t bufferLength;
  std::string file;

  // Output
  std::string err;
  int width, height, top, left, bottom, right, duration;

  RegionBaton():
    buffer(NULL),
    bufferLength(0),
    width(0),
    height(0),
    top(0),
    left(0),
    bottom(0),
    right(0),
    duration(0) {}
};

class RegionWorker : public Nan::AsyncWorker {

public:
  RegionWorker(Nan::Callback *callback, RegionBaton *baton) : Nan::AsyncWorker(callback), baton(baton) {}
  ~RegionWorker() {}

  void Execute() {
    GTimer *timer = g_timer_new();
    try {

      // Input
      ImageResizer resizer = ImageResizer(240);
      vips::VImage input;
      if (baton->buffer != NULL && baton->bufferLength > 0) {
        // From buffer
        input = resizer.FromBuffer(baton->buffer, baton->bufferLength);
      } else {
        // From file
        input = resizer.FromFile(baton->file);
      }
      baton->width = resizer.originalWidth;
      baton->height = resizer.originalHeight;

      // Generate saliency mask
      vips::VImage mask = Mask::Saliency(input);

      // Measure distance to first non-zero pixel along top and left edges
      vips::VImage profileLeft, profileTop = mask.profile(&profileLeft);
      const int top = floor(1.0 / resizer.ratio * profileTop.min());
      const int left = floor(1.0 / resizer.ratio * profileLeft.min());

      // Verify mask is non-empty
      if (top < resizer.originalHeight && left < resizer.originalWidth) {
        // Measure distance to first non-zero pixel along bottom and right edges
        vips::VImage profileRight, profileBottom = mask.rot(VIPS_ANGLE_D180).profile(&profileRight);
        const int bottom = resizer.originalHeight - 1 - floor(1.0 / resizer.ratio * profileBottom.min());
        const int right = resizer.originalWidth - 1 - floor(1.0 / resizer.ratio * profileRight.min());

        // Verify area of region is greater than 1/16 of original image area
        const int regionArea = (bottom - top) * (right - left);
        if (regionArea > resizer.originalWidth * resizer.originalHeight / 16.0) {
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
    } catch (vips::VError err) {
      baton->err = err.what();
    }

    // Store duration
    baton->duration = ceil(g_timer_elapsed(timer, NULL) * 1000.0);
    g_timer_destroy(timer);

    // Clean up libvips' per-request data and threads
    vips_error_clear();
    vips_thread_shutdown();
  }

  void HandleOKCallback () {
    Nan::HandleScope();

    v8::Local<v8::Value> argv[2] = { Nan::Null(), Nan::Null() };
    if (!baton->err.empty()) {
      // Error
      argv[0] = Nan::Error(baton->err.c_str());
    } else {
      // Region Object
      v8::Local<v8::Object> region = Nan::New<v8::Object>();
      Nan::Set(region, Nan::New("top").ToLocalChecked(), Nan::New<v8::Integer>(baton->top));
      Nan::Set(region, Nan::New("left").ToLocalChecked(), Nan::New<v8::Integer>(baton->left));
      Nan::Set(region, Nan::New("bottom").ToLocalChecked(), Nan::New<v8::Integer>(baton->bottom));
      Nan::Set(region, Nan::New("right").ToLocalChecked(), Nan::New<v8::Integer>(baton->right));
      Nan::Set(region, Nan::New("width").ToLocalChecked(), Nan::New<v8::Integer>(baton->width));
      Nan::Set(region, Nan::New("height").ToLocalChecked(), Nan::New<v8::Integer>(baton->height));
      Nan::Set(region, Nan::New("duration").ToLocalChecked(), Nan::New<v8::Integer>(baton->duration));
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
  Nan::HandleScope();
  RegionBaton *baton = new RegionBaton;

  // Parse options
  v8::Local<v8::Object> options = info[0].As<v8::Object>();
  if (Nan::Has(options, Nan::New("buffer").ToLocalChecked()).FromJust()) {
    // Input is a Buffer
    v8::Local<v8::Object> buffer = Nan::Get(options, Nan::New("buffer").ToLocalChecked()).ToLocalChecked().As<v8::Object>();
    // Take a copy to avoid problems with V8 heap compaction
    baton->bufferLength = node::Buffer::Length(buffer);
    baton->buffer = new char[baton->bufferLength];
    memcpy(baton->buffer, node::Buffer::Data(buffer), baton->bufferLength);
  } else {
    // Input is a filename
    baton->file = *Nan::Utf8String(Nan::Get(options, Nan::New("file").ToLocalChecked()).ToLocalChecked());
  }

  // Join queue for worker thread
  Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
  Nan::AsyncQueueWorker(new RegionWorker(callback, baton));
}
