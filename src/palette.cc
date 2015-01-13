#include <vips/vips8>

#include "exoquant.h"
#include "nan.h"
#include "palette.h"

struct PaletteBaton {
  // Input
  std::string file;
  void* buffer;
  size_t bufferLength;
  int swatches;

  // Output
  std::string err;
  unsigned char *palette;
  int duration;

  PaletteBaton():
    bufferLength(0),
    swatches(10),
    palette(NULL),
    duration(-1) {}
};

class PaletteWorker : public NanAsyncWorker {

 public:
  PaletteWorker(NanCallback *callback, PaletteBaton *baton) : NanAsyncWorker(callback), baton(baton) {}
  ~PaletteWorker() {}

  void Execute() {
    using vips::VImage;
    using vips::VError;

    try {
      GTimer *timer = g_timer_new();

      // Input
      VImage image;
      if (baton->bufferLength > 0) {
        // From buffer
        image = VImage::new_from_buffer(baton->buffer, baton->bufferLength, NULL);
      } else {
        // From file
        image = VImage::new_from_file(baton->file.c_str());
      }

      // Import embedded colour profile, if any
      if (image.get_typeof(VIPS_META_ICC_NAME) > 0) {
        image = image.icc_import(VImage::option()->set("embedded", TRUE));
      }

      // Resize shortest edge to 120px
      double shrink = 120.0 / static_cast<double>(std::min(image.width(), image.height()));
      if (shrink < 1.0) {
        image = image.resize(shrink);
      }

      // Ensure sRGB with alpha channel
      image = image.colourspace(VIPS_INTERPRETATION_sRGB);
      if (image.bands() == 3) {
        VImage alpha = VImage::black(1, 1).invert().zoom(image.width(), image.height());
        image = image.bandjoin(alpha);
      }

      // Get raw image data
      size_t size;
      void *data = image.write_to_memory(&size);

      // Quantise
      exq_data *exoquant = exq_init();
      exq_no_transparency(exoquant);
      exq_feed(exoquant, static_cast<unsigned char*>(data), size / 4);
      exq_quantize(exoquant, baton->swatches);

      // Get palette
      baton->palette = new unsigned char[baton->swatches * 4]();
      exq_get_palette(exoquant, baton->palette, baton->swatches);
      exq_free(exoquant);

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
      // Palette Object
      v8::Local<v8::Object> palette = NanNew<v8::Object>();
      v8::Local<v8::Array> swatches = NanNew<v8::Array>(baton->swatches);
      for (int i = 0; i < baton->swatches; i++) {
        // Get colour components
        int red = baton->palette[i * 4];
        int green = baton->palette[i * 4 + 1];
        int blue = baton->palette[i * 4 + 2];
        char css[8];
        snprintf(css, sizeof(css), "#%02x%02x%02x", red, green, blue);
        // Add swatch
        v8::Local<v8::Object> swatch = NanNew<v8::Object>();
        swatch->Set(NanNew<v8::String>("r"), NanNew<v8::Integer>(red));
        swatch->Set(NanNew<v8::String>("g"), NanNew<v8::Integer>(green));
        swatch->Set(NanNew<v8::String>("b"), NanNew<v8::Integer>(blue));
        swatch->Set(NanNew<v8::String>("css"), NanNew<v8::String>(css));
        swatches->Set(i, swatch);
      }
      palette->Set(NanNew<v8::String>("swatches"), swatches);
      palette->Set(NanNew<v8::String>("duration"), NanNew<v8::Integer>(baton->duration));
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
  NanScope();
  PaletteBaton *baton = new PaletteBaton;

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
  // Number of colour swatches
  baton->swatches = options->Get(NanNew<v8::String>("swatches"))->IntegerValue();

  // Join queue for worker thread
  NanCallback *callback = new NanCallback(args[1].As<v8::Function>());
  NanAsyncQueueWorker(new PaletteWorker(callback, baton));

  NanReturnUndefined();
}
