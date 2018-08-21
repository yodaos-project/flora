LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-cli.jni
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := \
	android/jni/com_rokid_flora_Caps.cpp \
	android/jni/com_rokid_flora_Client.cpp \
	android/jni/onload.cpp \
	android/jni/defs.h
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/android/jni
LOCAL_SHARED_LIBRARIES := libflora-cli librlog libcaps
include $(BUILD_SHARED_LIBRARY)
