#include "defs.h"
#include "rlog.h"

extern "C" jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;

  if (vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK) {
    KLOGE(TAG, "JNI_OnLoad failed");
    return -1;
  }
  register_com_rokid_flora_Caps(env);
  register_com_rokid_flora_Client(env);
  return JNI_VERSION_1_4;
}
