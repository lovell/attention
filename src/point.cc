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

class PointWorker : public NanAsyncWorker {

public:
  PointWorker(NanCallback *callback, PointBaton *baton) : NanAsyncWorker(callback), baton(baton) {}
  ~PointWorker() {}

  void Execute() {
    GTimer *timer = g_timer_new();
    try {

      // Input
      ImageResizer resizer = ImageResizer(320);
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
    NanScope();

    v8::Handle<v8::Value> argv[2] = { NanNull(), NanNull() };
    if (!baton->err.empty()) {
      // Error
      argv[0] = v8::Exception::Error(NanNew<v8::String>(baton->err.data(), baton->err.size()));
    } else {
      // Point Object
      v8::Local<v8::Object> region = NanNew<v8::Object>();
      region->Set(NanNew<v8::String>("x"), NanNew<v8::Integer>(baton->x));
      region->Set(NanNew<v8::String>("y"), NanNew<v8::Integer>(baton->y));
      region->Set(NanNew<v8::String>("width"), NanNew<v8::Integer>(baton->width));
      region->Set(NanNew<v8::String>("height"), NanNew<v8::Integer>(baton->height));
      region->Set(NanNew<v8::String>("duration"), NanNew<v8::Integer>(baton->duration));
      argv[1] = region;
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
  NanScope();
  PointBaton *baton = new PointBaton;

  // Parse options
  v8::Local<v8::Object> options = args[0]->ToObject();
  if (options->Get(NanNew<v8::String>("buffer"))->IsObject()) {
    // Input is a Buffer
    v8::Local<v8::Object> buffer = options->Get(NanNew<v8::String>("buffer"))->ToObject();
    // Take a copy to avoid problems with V8 heap compaction
    baton->bufferLength = node::Buffer::Length(buffer);
    baton->buffer = new char[baton->bufferLength];
    memcpy(baton->buffer, node::Buffer::Data(buffer), baton->bufferLength);
    options->Set(NanNew<v8::String>("buffer"), NanNull());
  } else {
    // Input is a filename
    baton->file = *v8::String::Utf8Value(options->Get(NanNew<v8::String>("file"))->ToString());
  }

  // Join queue for worker thread
  NanCallback *callback = new NanCallback(args[1].As<v8::Function>());
  NanAsyncQueueWorker(new PointWorker(callback, baton));

  NanReturnUndefined();
}
