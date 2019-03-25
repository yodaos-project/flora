LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-svc
LOCAL_LDLIBS := -llog
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
	include/flora-svc.h \
	src/defs.h \
	src/adap.h \
	src/sock-adap.h \
	src/sock-adap.cc \
	src/poll.cc \
	src/sock-poll.h \
	src/sock-poll.cc \
	src/beep-sock-poll.h \
	src/disp.h \
	src/disp.cc \
	src/ser-helper.h \
	src/ser-helper.cc
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := librlog libcaps
LOCAL_STATIC_LIBRARIES := libmisc
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(BUILD_SHARED_LIBRARY)

