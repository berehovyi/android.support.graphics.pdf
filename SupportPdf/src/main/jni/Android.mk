LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := SupportPdf
LOCAL_SRC_FILES := \
				SupportPdf.cpp \
				android_graphics_pdf/PdfDocument.cpp \
				android_graphics_pdf/PdfEditor.cpp \
				android_graphics_pdf/PdfRenderer.cpp

include $(BUILD_SHARED_LIBRARY)
