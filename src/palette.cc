#include <vips/vips8>

#include "nan.h"
#include "exoquant/exoquant.h"
#include "resizer.h"
#include "palette.h"

struct PaletteBaton {
  // Input
  void* buffer;
  size_t bufferLength;
  std::string file;
  int swatches;

  // Output
  unsigned char *palette;
  int duration;
  std::string err;

  PaletteBaton():
    buffer(NULL),
    bufferLength(0),
    swatches(10),
    palette(NULL),
    duration(-1) {}
};

class PaletteWorker : public Nan::AsyncWorker {

public:
  PaletteWorker(Nan::Callback *callback, PaletteBaton *baton) : Nan::AsyncWorker(callback), baton(baton) {}
  ~PaletteWorker() {}

  void Execute() {
    GTimer *timer = g_timer_new();

    // Raw image data
    void *data = NULL;
    size_t size = 0;

    try {

      // Input
      ImageResizer resizer = ImageResizer(120);
      vips::VImage input;
      if (baton->buffer != NULL && baton->bufferLength > 0) {
        // From buffer
        input = resizer.FromBuffer(baton->buffer, baton->bufferLength);
      } else {
        // From file
        input = resizer.FromFile(baton->file);
      }

      // Ensure sRGB with alpha channel
      input = input.colourspace(VIPS_INTERPRETATION_sRGB);
      if (input.bands() == 3) {
        vips::VImage alpha = vips::VImage::black(1, 1).invert().zoom(input.width(), input.height());
        input = input.bandjoin(alpha);
      }

      // Get raw image data
      data = input.write_to_memory(&size);

    } catch (vips::VError err) {
      baton->err = err.what();
    }

    if (data != NULL && size > 0) {
      // Quantise
      exq_data *exoquant = exq_init();
      exq_no_transparency(exoquant);
      exq_feed(exoquant, static_cast<unsigned char*>(data), size / 4);
      exq_quantize(exoquant, baton->swatches);

      // Get palette
      baton->palette = new unsigned char[baton->swatches * 4]();
      exq_get_palette(exoquant, baton->palette, baton->swatches);
      exq_free(exoquant);
      g_free(data);
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
      // Palette Object
      v8::Local<v8::Object> palette = Nan::New<v8::Object>();
      v8::Local<v8::Array> swatches = Nan::New<v8::Array>(baton->swatches);
      for (int i = 0; i < baton->swatches; i++) {
        // Get colour components
        int red = baton->palette[i * 4];
        int green = baton->palette[i * 4 + 1];
        int blue = baton->palette[i * 4 + 2];
        char css[8];
        snprintf(css, sizeof(css), "#%02x%02x%02x", red, green, blue);
        // Add swatch
        v8::Local<v8::Object> swatch = Nan::New<v8::Object>();
        Nan::Set(swatch, Nan::New("r").ToLocalChecked(), Nan::New<v8::Integer>(red));
        Nan::Set(swatch, Nan::New("g").ToLocalChecked(), Nan::New<v8::Integer>(green));
        Nan::Set(swatch, Nan::New("b").ToLocalChecked(), Nan::New<v8::Integer>(blue));
        Nan::Set(swatch, Nan::New("css").ToLocalChecked(), Nan::New<v8::String>(css).ToLocalChecked());
        Nan::Set(swatches, i, swatch);
      }
      Nan::Set(palette, Nan::New("swatches").ToLocalChecked(), swatches);
      Nan::Set(palette, Nan::New("duration").ToLocalChecked(), Nan::New<v8::Integer>(baton->duration));
      argv[1] = palette;
    }
    if (baton->palette != NULL) {
      delete baton->palette;
    }
    delete baton;

    // Return to JavaScript
    callback->Call(2, argv);
  }

private:
  PaletteBaton *baton;
};

NAN_METHOD(palette) {
  Nan::HandleScope();
  PaletteBaton *baton = new PaletteBaton;

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
  // Number of colour swatches
  baton->swatches = Nan::To<int32_t>(Nan::Get(options, Nan::New("swatches").ToLocalChecked()).ToLocalChecked()).FromJust();

  // Join queue for worker thread
  Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
  Nan::AsyncQueueWorker(new PaletteWorker(callback, baton));
}
