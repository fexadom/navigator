LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := screen
LOCAL_INIT_RC := screen.rc

LOCAL_SRC_FILES := \
	screen.cpp \
	oled/Edison_OLED.cpp \

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libbinderwrapper \
	libbrillo \
	libbrillo-binder \
	libc \
	libchrome \
	libmraa \
	libutils \

LOCAL_STATIC_LIBRARIES := \
	libservices-common \

LOCAL_CLANG := true
LOCAL_CFLAGS := -c -Wall -fexceptions

include $(BUILD_EXECUTABLE)
