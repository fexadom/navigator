LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bluescan
LOCAL_INIT_RC := bluescan.rc

LOCAL_SRC_FILES := \
	bluescan.cpp \
	callbacks.cpp \

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libbinderwrapper \
	libbrillo \
	libbrillo-binder \
	libchrome \
	libutils \

LOCAL_STATIC_LIBRARIES := \
	libbluetooth-client \
	libservices-common \

LOCAL_CFLAGS += $(bluetooth_CFLAGS) $(btservice_orig_TARGET_NDEBUG)
LOCAL_CONLYFLAGS += $(bluetooth_CONLYFLAGS)
LOCAL_CPPFLAGS += $(bluetooth_CPPFLAGS)

include $(BUILD_EXECUTABLE)

