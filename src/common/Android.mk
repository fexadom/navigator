LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libservices-common
LOCAL_AIDL_INCLUDES := $(LOCAL_PATH)/aidl
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := \
	aidl/navigator/services/screen/IScreenService.aidl \
	aidl/navigator/services/bluescan/IBluescanService.aidl \
	aidl/navigator/services/bluescan/IBluescanCallback.aidl \
	binder_constants.cpp \
	navigator_constants.cpp \

include $(BUILD_STATIC_LIBRARY)
