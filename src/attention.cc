#include <vips/vips8>

#include "nan.h"
#include "palette.h"
#include "region.h"

static void at_exit(void* arg) {
  NanScope();
  vips_shutdown();
}

extern "C" void init(v8::Handle<v8::Object> target) {
  NanScope();
  vips_init("attention");
  node::AtExit(at_exit);

  NODE_SET_METHOD(target, "palette", palette);
  NODE_SET_METHOD(target, "region", region);
}

NODE_MODULE(attention, init)
