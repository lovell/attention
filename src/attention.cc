#include <vips/vips8>

#include "nan.h"
#include "palette.h"
#include "region.h"
#include "point.h"

NAN_MODULE_INIT(init) {
  vips_init("attention");

  Nan::Set(target, Nan::New("palette").ToLocalChecked(),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(palette)).ToLocalChecked());
  Nan::Set(target, Nan::New("region").ToLocalChecked(),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(region)).ToLocalChecked());
  Nan::Set(target, Nan::New("point").ToLocalChecked(),
    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(point)).ToLocalChecked());
}

NODE_MODULE(attention, init)
