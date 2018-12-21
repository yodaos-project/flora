#include "defs.h"
#include "flora-cli.h"
#include "rlog.h"

using namespace std;

#define JAVA_METHOD_POST_CALLBACK 0
#define JAVA_METHOD_GET_CALLBACK 1
#define JAVA_METHOD_DISCONNECTED_CALLBACK 2
#define JAVA_METHOD_LIST_ADD 3
#define JAVA_METHOD_CAPS_CONSTRUCTOR 4
#define JAVA_METHOD_NUMBER 5

#define JAVA_FIELD_NATIVE_HANDLE 0
#define JAVA_FIELD_REPLY_RETCODE 1
#define JAVA_FIELD_REPLY_MSG 2
#define JAVA_FIELD_REPLY_EXTRA 3
#define JAVA_FIELD_NUMBER 4

typedef struct {
  JavaVM *jvm;
  jmethodID java_methods[JAVA_METHOD_NUMBER];
  jfieldID java_fields[JAVA_FIELD_NUMBER];
  jclass java_reply_cls;
  jclass java_caps_cls;
} GlobalConstants;
static GlobalConstants g_constants;

class FloraCallbackNative : public flora::ClientCallback {
public:
  void recv_post(const char *name, uint32_t msgtype, shared_ptr<Caps> &msg) {
    attachJavaThread();
    env->PushLocalFrame(16);
    jstring nameobj = env->NewStringUTF(name);
    CapsNativeHandle *capsh = new CapsNativeHandle();
    capsh->caps = msg;
    env->CallVoidMethod(javaThis,
                        g_constants.java_methods[JAVA_METHOD_POST_CALLBACK],
                        nameobj, (jlong)capsh, msgtype);
    env->PopLocalFrame(0);
    detachJavaThread();
  }

  int32_t recv_get(const char *name, shared_ptr<Caps> &msg,
                   shared_ptr<Caps> &reply) {
    attachJavaThread();
    env->PushLocalFrame(16);
    jstring nameobj = env->NewStringUTF(name);
    CapsNativeHandle *capsh = new CapsNativeHandle();
    CapsNativeHandle *replyh = new CapsNativeHandle();
    capsh->caps = msg;
    int32_t r = env->CallIntMethod(
        javaThis, g_constants.java_methods[JAVA_METHOD_GET_CALLBACK], nameobj,
        (jlong)capsh, (jlong)replyh);
    reply = replyh->caps;
    delete replyh;
    env->PopLocalFrame(0);
    detachJavaThread();
    return r;
  }

  void disconnected() {
    attachJavaThread();
    env->PushLocalFrame(16);
    env->CallVoidMethod(
        javaThis, g_constants.java_methods[JAVA_METHOD_DISCONNECTED_CALLBACK]);
    env->PopLocalFrame(0);
    detachJavaThread();
  }

private:
  void attachJavaThread() {
    if (env == nullptr)
      g_constants.jvm->AttachCurrentThreadAsDaemon(&env, nullptr);
  }

  void detachJavaThread() {
    if (env) {
      g_constants.jvm->DetachCurrentThread();
      env = nullptr;
    }
  }

public:
  jobject javaThis = nullptr;

private:
  JNIEnv *env = nullptr;
};

typedef struct {
  shared_ptr<flora::Client> cli;
  FloraCallbackNative callback;
} NativeHandle;

static void com_rokid_flora_Client_native_init(JNIEnv *env, jclass thiz) {
  env->GetJavaVM(&g_constants.jvm);
  g_constants.java_methods[JAVA_METHOD_POST_CALLBACK] =
      env->GetMethodID(thiz, "nativePostCallback", "(Ljava/lang/String;JI)V");
  g_constants.java_methods[JAVA_METHOD_GET_CALLBACK] =
      env->GetMethodID(thiz, "nativeGetCallback", "(Ljava/lang/String;JJ)I");
  g_constants.java_methods[JAVA_METHOD_DISCONNECTED_CALLBACK] =
      env->GetMethodID(thiz, "nativeDisconnectCallback", "()V");
  g_constants.java_fields[JAVA_FIELD_NATIVE_HANDLE] =
      env->GetFieldID(thiz, "native_handle", "J");

  jclass cls;
  cls = env->FindClass("com/rokid/flora/Client$Reply");
  g_constants.java_reply_cls = (jclass)env->NewGlobalRef(cls);
  KLOGI(TAG, "find java class Reply %p", g_constants.java_reply_cls);
  g_constants.java_fields[JAVA_FIELD_REPLY_RETCODE] =
      env->GetFieldID(g_constants.java_reply_cls, "retCode", "I");
  g_constants.java_fields[JAVA_FIELD_REPLY_MSG] = env->GetFieldID(
      g_constants.java_reply_cls, "msg", "Lcom/rokid/flora/Caps;");
  g_constants.java_fields[JAVA_FIELD_REPLY_EXTRA] = env->GetFieldID(
      g_constants.java_reply_cls, "extra", "Ljava/lang/String;");

  cls = env->FindClass("com/rokid/flora/Caps");
  g_constants.java_caps_cls = (jclass)env->NewGlobalRef(cls);
  g_constants.java_methods[JAVA_METHOD_CAPS_CONSTRUCTOR] =
      env->GetMethodID(g_constants.java_caps_cls, "<init>", "(J)V");

  cls = env->FindClass("java/util/List");
  g_constants.java_methods[JAVA_METHOD_LIST_ADD] =
      env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
}

static jint com_rokid_flora_Client_native_connect(JNIEnv *env, jobject thiz,
                                                  jstring uri, jint bufsize) {
  if (thiz == nullptr || uri == nullptr)
    return FLORA_CLI_EINVAL;
  NativeHandle *handle = new NativeHandle();
  char *uristr;
  jsize slen;
  handle->callback.javaThis = env->NewGlobalRef(thiz);
  slen = env->GetStringUTFLength(uri);
  uristr = new char[slen + 1];
  env->GetStringUTFRegion(uri, 0, slen, uristr);
  uristr[slen] = '\0';
  int32_t r =
      flora::Client::connect(uristr, &handle->callback, bufsize, handle->cli);
  delete[] uristr;
  if (r == FLORA_CLI_SUCCESS) {
    env->SetLongField(thiz, g_constants.java_fields[JAVA_FIELD_NATIVE_HANDLE],
                      (jlong)handle);
  } else {
    env->DeleteGlobalRef(handle->callback.javaThis);
    delete handle;
  }
  return r;
}

static void com_rokid_flora_Client_native_destroy(JNIEnv *env, jobject thiz,
                                                  jlong handle) {
  if (handle == 0)
    return;
  NativeHandle *nh = (NativeHandle *)handle;
  env->DeleteGlobalRef(nh->callback.javaThis);
  delete nh;
}

static jint com_rokid_flora_Client_native_subscribe(JNIEnv *env, jobject thiz,
                                                    jlong handle, jstring name,
                                                    jint type) {
  NativeHandle *nh = (NativeHandle *)handle;
  char *namestr;
  jsize slen;
  slen = env->GetStringUTFLength(name);
  namestr = new char[slen + 1];
  env->GetStringUTFRegion(name, 0, slen, namestr);
  namestr[slen] = '\0';
  int32_t r = nh->cli->subscribe(namestr, type);
  delete[] namestr;
  return r;
}

static jint com_rokid_flora_Client_native_unsubscribe(JNIEnv *env, jobject thiz,
                                                      jlong handle,
                                                      jstring name, jint type) {
  NativeHandle *nh = (NativeHandle *)handle;
  char *namestr;
  jsize slen;
  slen = env->GetStringUTFLength(name);
  namestr = new char[slen + 1];
  env->GetStringUTFRegion(name, 0, slen, namestr);
  namestr[slen] = '\0';
  int32_t r = nh->cli->unsubscribe(namestr, type);
  delete[] namestr;
  return r;
}

static jint com_rokid_flora_Client_native_post(JNIEnv *env, jobject thiz,
                                               jlong handle, jstring name,
                                               jlong msg, jint type) {
  NativeHandle *nh = (NativeHandle *)handle;
  char *namestr;
  jsize slen;
  slen = env->GetStringUTFLength(name);
  namestr = new char[slen + 1];
  env->GetStringUTFRegion(name, 0, slen, namestr);
  namestr[slen] = '\0';
  CapsNativeHandle *cmsg = (CapsNativeHandle *)msg;
  shared_ptr<Caps> t;
  int32_t r = nh->cli->post(namestr, cmsg ? cmsg->caps : t, type);
  delete[] namestr;
  return r;
}

static void gen_java_reply_list(JNIEnv *env, jobject replys,
                                vector<flora::Reply> &nreps) {
  jobject jr;
  jobject jm;
  jobject je;
  vector<flora::Reply>::iterator it;

  for (it = nreps.begin(); it != nreps.end(); ++it) {
    jr = env->AllocObject(g_constants.java_reply_cls);
    env->SetIntField(jr, g_constants.java_fields[JAVA_FIELD_REPLY_RETCODE],
                     (*it).ret_code);
    CapsNativeHandle *cnh = new CapsNativeHandle();
    cnh->caps = (*it).data;
    jm = env->NewObject(g_constants.java_caps_cls,
                        g_constants.java_methods[JAVA_METHOD_CAPS_CONSTRUCTOR],
                        (jlong)cnh);
    env->SetObjectField(jr, g_constants.java_fields[JAVA_FIELD_REPLY_MSG], jm);
    je = env->NewStringUTF((*it).extra.c_str());
    env->SetObjectField(jr, g_constants.java_fields[JAVA_FIELD_REPLY_EXTRA],
                        je);
    env->CallVoidMethod(replys, g_constants.java_methods[JAVA_METHOD_LIST_ADD],
                        jr);
  }
}

static jint com_rokid_flora_Client_native_get(JNIEnv *env, jobject thiz,
                                              jlong handle, jstring name,
                                              jlong msg, jobject replys,
                                              jint timeout) {
  NativeHandle *nh = (NativeHandle *)handle;
  char *namestr;
  jsize slen;
  slen = env->GetStringUTFLength(name);
  namestr = new char[slen + 1];
  env->GetStringUTFRegion(name, 0, slen, namestr);
  namestr[slen] = '\0';
  CapsNativeHandle *cmsg = (CapsNativeHandle *)msg;
  vector<flora::Reply> nreps;
  shared_ptr<Caps> t;
  int32_t r = nh->cli->get(namestr, cmsg ? cmsg->caps : t, nreps, timeout);
  delete[] namestr;
  if (r == FLORA_CLI_SUCCESS) {
    gen_java_reply_list(env, replys, nreps);
  }
  return r;
}

static void com_rokid_flora_Client_native_set_pointer(JNIEnv *env, jobject thiz,
                                                      jlong src, jlong dst) {
  CapsNativeHandle *srch = (CapsNativeHandle *)src;
  CapsNativeHandle *dsth = (CapsNativeHandle *)dst;
  dsth->caps = srch->caps;
}

static JNINativeMethod _cli_nmethods[] = {
    {"native_init", "()V", (void *)com_rokid_flora_Client_native_init},
    {"native_connect", "(Ljava/lang/String;I)I",
     (void *)com_rokid_flora_Client_native_connect},
    {"native_destroy", "(J)V", (void *)com_rokid_flora_Client_native_destroy},
    {"native_subscribe", "(JLjava/lang/String;I)I",
     (void *)com_rokid_flora_Client_native_subscribe},
    {"native_unsubscribe", "(JLjava/lang/String;I)I",
     (void *)com_rokid_flora_Client_native_unsubscribe},
    {"native_post", "(JLjava/lang/String;JI)I",
     (void *)com_rokid_flora_Client_native_post},
    {"native_get", "(JLjava/lang/String;JLjava/util/List;I)I",
     (void *)com_rokid_flora_Client_native_get},
    {"native_set_pointer", "(JJ)V",
     (void *)com_rokid_flora_Client_native_set_pointer},
};

int register_com_rokid_flora_Client(JNIEnv *env) {
  const char *kclass = "com/rokid/flora/Client";
  jclass target = env->FindClass(kclass);
  if (target == NULL) {
    KLOGE("find class for %s failed", kclass);
    return -1;
  }
  return env->RegisterNatives(target, _cli_nmethods,
                              sizeof(_cli_nmethods) / sizeof(JNINativeMethod));
}
