#include "defs.h"
#include "rlog.h"

using namespace std;

#define JAVA_FIELD_INTEGER_VALUE 0
#define JAVA_FIELD_FLOAT_VALUE 1
#define JAVA_FIELD_LONG_VALUE 2
#define JAVA_FIELD_DOUBLE_VALUE 3
#define JAVA_FIELD_STRING_VALUE 4
#define JAVA_FIELD_BINARY_VALUE 5
#define JAVA_FIELD_CAPS_VALUE 6
#define JAVA_FIELD_NUMBER 7

typedef struct {
  jfieldID java_fields[JAVA_FIELD_NUMBER];
} GlobalConstants;
static GlobalConstants g_constants;

static void com_rokid_flora_Caps_native_init(JNIEnv *env, jclass thiz,
                                             jobjectArray classes) {
  jsize len = env->GetArrayLength(classes);
  if (len != JAVA_FIELD_NUMBER) {
    KLOGE(TAG, "native_init: classes array length incorrect %d/%d", len,
          JAVA_FIELD_NUMBER);
    return;
  }
  jclass cls;
  cls = (jclass)env->GetObjectArrayElement(classes, 0);
  g_constants.java_fields[JAVA_FIELD_INTEGER_VALUE] =
      env->GetFieldID(cls, "value", "I");
  cls = (jclass)env->GetObjectArrayElement(classes, 1);
  g_constants.java_fields[JAVA_FIELD_FLOAT_VALUE] =
      env->GetFieldID(cls, "value", "F");
  cls = (jclass)env->GetObjectArrayElement(classes, 2);
  g_constants.java_fields[JAVA_FIELD_LONG_VALUE] =
      env->GetFieldID(cls, "value", "J");
  cls = (jclass)env->GetObjectArrayElement(classes, 3);
  g_constants.java_fields[JAVA_FIELD_DOUBLE_VALUE] =
      env->GetFieldID(cls, "value", "D");
  cls = (jclass)env->GetObjectArrayElement(classes, 4);
  g_constants.java_fields[JAVA_FIELD_STRING_VALUE] =
      env->GetFieldID(cls, "value", "Ljava/lang/String;");
  cls = (jclass)env->GetObjectArrayElement(classes, 5);
  g_constants.java_fields[JAVA_FIELD_BINARY_VALUE] =
      env->GetFieldID(cls, "value", "[B");
  cls = (jclass)env->GetObjectArrayElement(classes, 6);
  g_constants.java_fields[JAVA_FIELD_CAPS_VALUE] =
      env->GetFieldID(cls, "value", "J");
}

static jlong com_rokid_flora_Caps_native_create(JNIEnv *env, jobject thiz) {
  CapsNativeHandle *handle = new CapsNativeHandle();
  handle->caps = Caps::new_instance();
  return (jlong)handle;
}

static void com_rokid_flora_Caps_native_destroy(JNIEnv *env, jobject thiz,
                                                jlong h) {
  if (h == 0)
    return;
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  delete handle;
}

static jint com_rokid_flora_Caps_native_write_integer(JNIEnv *env, jobject thiz,
                                                      jlong h, jint v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  return handle->caps->write((int32_t)v);
}

static jint com_rokid_flora_Caps_native_write_float(JNIEnv *env, jobject thiz,
                                                    jlong h, jfloat v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  return handle->caps->write(v);
}

static jint com_rokid_flora_Caps_native_write_long(JNIEnv *env, jobject thiz,
                                                   jlong h, jlong v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  return handle->caps->write((int64_t)v);
}

static jint com_rokid_flora_Caps_native_write_double(JNIEnv *env, jobject thiz,
                                                     jlong h, jdouble v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  return handle->caps->write(v);
}

static jint com_rokid_flora_Caps_native_write_string(JNIEnv *env, jobject thiz,
                                                     jlong h, jstring v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  char *str = nullptr;
  if (v != nullptr) {
    jsize len = env->GetStringUTFLength(v);
    str = new char[len + 1];
    env->GetStringUTFRegion(v, 0, len, str);
    str[len] = '\0';
  }
  int32_t r = handle->caps->write(str);
  if (str)
    delete[] str;
  return r;
}

static jint com_rokid_flora_Caps_native_write_binary(JNIEnv *env, jobject thiz,
                                                     jlong h, jbyteArray v,
                                                     jint offset, jint length) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  jbyte *data = nullptr;
  if (v != nullptr) {
    data = new jbyte[length];
    env->GetByteArrayRegion(v, offset, length, data);
  } else {
    offset = 0;
    length = 0;
  }
  int32_t r = handle->caps->write(data, length);
  if (data)
    delete[] data;
  return r;
}

static jint com_rokid_flora_Caps_native_write_caps(JNIEnv *env, jobject thiz,
                                                   jlong h, jlong v) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  int32_t r;
  if (v == 0) {
    shared_ptr<Caps> t;
    r = handle->caps->write(t);
  } else {
    CapsNativeHandle *sub = (CapsNativeHandle *)v;
    r = handle->caps->write(sub->caps);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_integer(JNIEnv *env, jobject thiz,
                                                     jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  int32_t v;
  int32_t r = handle->caps->read(v);
  if (r == CAPS_SUCCESS) {
    env->SetIntField(res, g_constants.java_fields[JAVA_FIELD_INTEGER_VALUE], v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_float(JNIEnv *env, jobject thiz,
                                                   jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  float v;
  int32_t r = handle->caps->read(v);
  if (r == CAPS_SUCCESS) {
    env->SetFloatField(res, g_constants.java_fields[JAVA_FIELD_FLOAT_VALUE], v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_long(JNIEnv *env, jobject thiz,
                                                  jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  int64_t v;
  int32_t r = handle->caps->read(v);
  if (r == CAPS_SUCCESS) {
    env->SetLongField(res, g_constants.java_fields[JAVA_FIELD_LONG_VALUE],
                      (jlong)v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_double(JNIEnv *env, jobject thiz,
                                                    jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  double v;
  int32_t r = handle->caps->read(v);
  if (r == CAPS_SUCCESS) {
    env->SetDoubleField(res, g_constants.java_fields[JAVA_FIELD_DOUBLE_VALUE],
                        v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_string(JNIEnv *env, jobject thiz,
                                                    jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  string s;
  int32_t r = handle->caps->read_string(s);
  if (r == CAPS_SUCCESS) {
    jstring v = env->NewStringUTF(s.c_str());
    env->SetObjectField(res, g_constants.java_fields[JAVA_FIELD_STRING_VALUE],
                        v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_binary(JNIEnv *env, jobject thiz,
                                                    jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  string b;
  int32_t r = handle->caps->read_binary(b);
  if (r == CAPS_SUCCESS) {
    jbyteArray v = env->NewByteArray(b.length());
    env->SetByteArrayRegion(v, 0, b.length(), (const jbyte *)b.data());
    env->SetObjectField(res, g_constants.java_fields[JAVA_FIELD_BINARY_VALUE],
                        v);
  }
  return r;
}

static jint com_rokid_flora_Caps_native_read_caps(JNIEnv *env, jobject thiz,
                                                  jlong h, jobject res) {
  CapsNativeHandle *handle = (CapsNativeHandle *)h;
  shared_ptr<Caps> v;
  int32_t r = handle->caps->read(v);
  if (r == CAPS_SUCCESS) {
    CapsNativeHandle *rh = new CapsNativeHandle();
    rh->caps = v;
    env->SetLongField(res, g_constants.java_fields[JAVA_FIELD_LONG_VALUE],
                      (jlong)rh);
  }
  return r;
}

static JNINativeMethod _caps_nmethods[] = {
    {"native_init", "([Ljava/lang/Class;)V",
     (void *)com_rokid_flora_Caps_native_init},
    {"native_create", "()J", (void *)com_rokid_flora_Caps_native_create},
    {"native_destroy", "(J)V", (void *)com_rokid_flora_Caps_native_destroy},
    {"native_write_integer", "(JI)I",
     (void *)com_rokid_flora_Caps_native_write_integer},
    {"native_write_float", "(JF)I",
     (void *)com_rokid_flora_Caps_native_write_float},
    {"native_write_long", "(JJ)I",
     (void *)com_rokid_flora_Caps_native_write_long},
    {"native_write_double", "(JD)I",
     (void *)com_rokid_flora_Caps_native_write_double},
    {"native_write_string", "(JLjava/lang/String;)I",
     (void *)com_rokid_flora_Caps_native_write_string},
    {"native_write_binary", "(J[BII)I",
     (void *)com_rokid_flora_Caps_native_write_binary},
    {"native_write_caps", "(JJ)I",
     (void *)com_rokid_flora_Caps_native_write_caps},
    {"native_read", "(JLcom/rokid/flora/Caps$IntegerValue;)I",
     (void *)com_rokid_flora_Caps_native_read_integer},
    {"native_read", "(JLcom/rokid/flora/Caps$FloatValue;)I",
     (void *)com_rokid_flora_Caps_native_read_float},
    {"native_read", "(JLcom/rokid/flora/Caps$LongValue;)I",
     (void *)com_rokid_flora_Caps_native_read_long},
    {"native_read", "(JLcom/rokid/flora/Caps$DoubleValue;)I",
     (void *)com_rokid_flora_Caps_native_read_double},
    {"native_read", "(JLcom/rokid/flora/Caps$StringValue;)I",
     (void *)com_rokid_flora_Caps_native_read_string},
    {"native_read", "(JLcom/rokid/flora/Caps$BinaryValue;)I",
     (void *)com_rokid_flora_Caps_native_read_binary},
    {"native_read", "(JLcom/rokid/flora/Caps$CapsValue;)I",
     (void *)com_rokid_flora_Caps_native_read_caps},
};

int register_com_rokid_flora_Caps(JNIEnv *env) {
  const char *kclass = "com/rokid/flora/Caps";
  jclass target = env->FindClass(kclass);
  if (target == NULL) {
    KLOGE("find class for %s failed", kclass);
    return -1;
  }
  return env->RegisterNatives(target, _caps_nmethods,
                              sizeof(_caps_nmethods) / sizeof(JNINativeMethod));
}
