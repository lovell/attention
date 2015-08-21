#include <numeric>
#include <vips/vips8>

#include "nan.h"
#include "resizer.h"
#include "mask.h"
#include "point.h"

struct PointBaton {
  // Input
  void *buffer;
  size_t bufferLength;
  std::string file;

  // Output
  std::string err;
  int width, height, x, y, duration;

  PointBaton():
    buffer(NULL),
    bufferLength(0),
    width(0),
    height(0),
    x(0),
    y(0),
    duration(0) {}
};

class PointWorker : public Nan::AsyncWorker {

public:
  PointWorker(Nan::Callback *callback, PointBaton *baton) : Nan::AsyncWorker(callback), baton(baton) {}
  ~PointWorker() {}

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

      // Approximate focal point using the centre of gravity of pixels in the mask
      vips::VImage projectRows, projectCols = mask.project(&projectRows);
      size_t colBytes, rowBytes;
      uint32_t *colData = reinterpret_cast<uint32_t*>(projectCols.write_to_memory(&colBytes));
      baton->x = floor(1.0 / resizer.ratio * ElementAtMidpoint(colData, colBytes / 4));
      g_free(colData);
      uint32_t *rowData = reinterpret_cast<uint32_t*>(projectRows.write_to_memory(&rowBytes));
      baton->y = floor(1.0 / resizer.ratio * ElementAtMidpoint(rowData, rowBytes / 4));
      g_free(rowData);

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
      // Point Object
      v8::Local<v8::Object> point = Nan::New<v8::Object>();
      Nan::Set(point, Nan::New("x").ToLocalChecked(), Nan::New<v8::Integer>(baton->x));
      Nan::Set(point, Nan::New("y").ToLocalChecked(), Nan::New<v8::Integer>(baton->y));
      Nan::Set(point, Nan::New("width").ToLocalChecked(), Nan::New<v8::Integer>(baton->width));
      Nan::Set(point, Nan::New("height").ToLocalChecked(), Nan::New<v8::Integer>(baton->height));
      Nan::Set(point, Nan::New("duration").ToLocalChecked(), Nan::New<v8::Integer>(baton->duration));
      argv[1] = point;
    }
    delete baton;

    // Return to JavaScript
    callback->Call(2, argv);
  }

private:
  PointBaton *baton;

  /*
    Find which element in a histogram contains the mid-point of the cumulative total
  */
  int ElementAtMidpoint(uint32_t* data, int size) {
    // Convert to vector
    std::vector<uint32_t> elements;
    elements.insert(elements.end(), data, data + size);
    // Calculate running total at each element
    int partialSum[size];
    std::partial_sum(elements.begin(), elements.end(), partialSum);
    // Find element at mid point of running total
    const int midPoint = floor(partialSum[size - 1] / 2.0);
    int elementAtMidPoint = 0;
    while (partialSum[elementAtMidPoint++] < midPoint);
    return elementAtMidPoint;
  }

};

NAN_METHOD(point) {
  Nan::HandleScope();
  PointBaton *baton = new PointBaton;

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
  Nan::AsyncQueueWorker(new PointWorker(callback, baton));
}
