LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-cli
LOCAL_LDLIBS := -llog
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
	include/flora-cli.h \
	src/cli.h \
	src/cli.cc \
	src/conn.h \
	src/tcp-conn.h \
	src/tcp-conn.cc \
	src/unix-conn.h \
	src/unix-conn.cc \
	src/defs.h \
	src/ser-helper.h \
	src/ser-helper.cc
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := librlog libcaps
LOCAL_STATIC_LIBRARIES := libmisc
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_SHARED_LIBRARY)
