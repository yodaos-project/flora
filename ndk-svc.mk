LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-svc
LOCAL_LDLIBS := -llog
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
	include/flora-svc.h \
	include/flora-cli.h \
	src/defs.h \
	src/adap.h \
	src/sock-adap.h \
	src/sock-adap.cc \
	src/poll.cc \
	src/tcp-poll.h \
	src/tcp-poll.cc \
	src/unix-poll.h \
	src/unix-poll.cc \
	src/disp.h \
	src/disp.cc \
	src/reply-mgr.h \
	src/reply-mgr.cc \
	src/ser-helper.h \
	src/ser-helper.cc
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := librlog libcaps
LOCAL_STATIC_LIBRARIES := libmisc
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_SHARED_LIBRARY)

