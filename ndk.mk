LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-cli
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := \
	src/cli.cpp \
	src/common.cpp
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libcaps
LOCAL_STATIC_LIBRARIES := libmutils
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -std=c++11 \
    -fexceptions \
    -DROKID_LOG_ENABLED=1
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libflora-svc
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := \
	src/svc.cpp \
	src/common.cpp
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libcaps
LOCAL_STATIC_LIBRARIES := libmutils
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -std=c++11 \
    -fexceptions \
    -DROKID_LOG_ENABLED=1
include $(BUILD_SHARED_LIBRARY)
