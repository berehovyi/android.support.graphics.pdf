/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include <fpdfview.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#include "fsdk_rendercontext.h"
#pragma GCC diagnostic pop

#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>

#include "Mutex.h"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PdfEditor", __VA_ARGS__)

void jniThrowException(JNIEnv* env, const char* className, const char* msg) {
    jclass clazz = env->FindClass(className);
    if (!clazz) {
        ALOGE("Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return;
    }

    if (env->ThrowNew(clazz, msg) != JNI_OK) {
        ALOGE("Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
    }
    env->DeleteLocalRef(clazz);
}


namespace android {

static const int RENDER_MODE_FOR_DISPLAY = 1;
static const int RENDER_MODE_FOR_PRINT = 2;

static struct {
    jfieldID x;
    jfieldID y;
} gPointClassInfo;

static Mutex sLock;

static int sUnmatchedInitRequestCount = 0;

static void initializeLibraryIfNeeded() {
    Autolock _l(sLock);
    if (sUnmatchedInitRequestCount == 0) {
        FPDF_InitLibrary(NULL);
    }
    sUnmatchedInitRequestCount++;
}

static void destroyLibraryIfNeeded() {
    Autolock _l(sLock);
    sUnmatchedInitRequestCount--;
    if (sUnmatchedInitRequestCount == 0) {
       FPDF_DestroyLibrary();
    }
}

static int getBlock(void* param, unsigned long position, unsigned char* outBuffer,
        unsigned long size) {
    const int fd = reinterpret_cast<intptr_t>(param);
    const int readCount = pread(fd, outBuffer, size, position);
    if (readCount < 0) {
        ALOGE("Cannot read from file descriptor. Error:%d", errno);
        return 0;
    }
    return 1;
}

extern "C"
jlong Java_android_support_graphics_pdf_PdfRenderer_nativeCreate(JNIEnv* env, jclass thiz, jint fd, jlong size) {
    initializeLibraryIfNeeded();

    FPDF_FILEACCESS loader;
    loader.m_FileLen = size;
    loader.m_Param = reinterpret_cast<void*>(intptr_t(fd));
    loader.m_GetBlock = &getBlock;

    FPDF_DOCUMENT document = FPDF_LoadCustomDocument(&loader, NULL);

    if (!document) {
        const long error = FPDF_GetLastError();
        char buffer[36];
        sprintf(buffer, "cannot create document. Error: %ld", error);

        jniThrowException(env, "java/io/IOException", buffer);
        destroyLibraryIfNeeded();
        return -1;
    }

    return reinterpret_cast<jlong>(document);
}

extern "C"
jlong Java_android_support_graphics_pdf_PdfRenderer_nativeOpenPageAndGetSize(JNIEnv* env, jclass thiz, jlong documentPtr,
        jint pageIndex, jobject outSize) {
    FPDF_DOCUMENT document = reinterpret_cast<FPDF_DOCUMENT>(documentPtr);

    FPDF_PAGE page = FPDF_LoadPage(document, pageIndex);

    if (!page) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "cannot load page");
        return -1;
    }

    double width = 0;
    double height = 0;

    const int result = FPDF_GetPageSizeByIndex(document, pageIndex, &width, &height);

    if (!result) {
        jniThrowException(env, "java/lang/IllegalStateException",
                    "cannot get page size");
        return -1;
    }

    env->SetIntField(outSize, gPointClassInfo.x, width);
    env->SetIntField(outSize, gPointClassInfo.y, height);

    return reinterpret_cast<jlong>(page);
}

extern "C"
void Java_android_support_graphics_pdf_PdfRenderer_nativeClosePage(JNIEnv* env, jclass thiz, jlong pagePtr) {
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    FPDF_ClosePage(page);
}

extern "C"
void Java_android_support_graphics_pdf_PdfRenderer_nativeClose(JNIEnv* env, jclass thiz, jlong documentPtr) {
    FPDF_DOCUMENT document = reinterpret_cast<FPDF_DOCUMENT>(documentPtr);
    FPDF_CloseDocument(document);
    destroyLibraryIfNeeded();
}

extern "C"
jint Java_android_support_graphics_pdf_PdfRenderer_nativeGetPageCount(JNIEnv* env, jclass thiz, jlong documentPtr) {
    FPDF_DOCUMENT document = reinterpret_cast<FPDF_DOCUMENT>(documentPtr);
    return FPDF_GetPageCount(document);
}

extern "C"
jboolean Java_android_support_graphics_pdf_PdfRenderer_nativeScaleForPrinting(JNIEnv* env, jclass thiz, jlong documentPtr) {
    FPDF_DOCUMENT document = reinterpret_cast<FPDF_DOCUMENT>(documentPtr);
    return FPDF_VIEWERREF_GetPrintScaling(document);
}

static void DropContext(void* data) {
    delete (CRenderContext*) data;
}

void renderPageBitmap(FPDF_BITMAP bitmap, FPDF_PAGE page, int destLeft, int destTop,
        int destRight, int destBottom, /*SkMatrix* transform,*/ int flags) {
    // Note: this code ignores the currently unused RENDER_NO_NATIVETEXT,
    // FPDF_RENDER_LIMITEDIMAGECACHE, FPDF_RENDER_FORCEHALFTONE, FPDF_GRAYSCALE,
    // and FPDF_ANNOT flags. To add support for that refer to FPDF_RenderPage_Retail
    // in fpdfview.cpp

    CRenderContext* pContext = FX_NEW CRenderContext;

    CPDF_Page* pPage = (CPDF_Page*) page;
    pPage->SetPrivateData((void*) 1, pContext, DropContext);

    CFX_FxgeDevice* fxgeDevice = FX_NEW CFX_FxgeDevice;
    pContext->m_pDevice = fxgeDevice;

    // Reverse the bytes (last argument TRUE) since the Android
    // format is ARGB while the renderer uses BGRA internally.
    fxgeDevice->Attach((CFX_DIBitmap*) bitmap, 0, TRUE);

    CPDF_RenderOptions* renderOptions = pContext->m_pOptions;

    if (!renderOptions) {
        renderOptions = FX_NEW CPDF_RenderOptions;
        pContext->m_pOptions = renderOptions;
    }

    if (flags & FPDF_LCD_TEXT) {
        renderOptions->m_Flags |= RENDER_CLEARTYPE;
    } else {
        renderOptions->m_Flags &= ~RENDER_CLEARTYPE;
    }

    const CPDF_OCContext::UsageType usage = (flags & FPDF_PRINTING)
            ? CPDF_OCContext::Print : CPDF_OCContext::View;

    renderOptions->m_AddFlags = flags >> 8;
    renderOptions->m_pOCContext = new CPDF_OCContext(pPage->m_pDocument, usage);

    fxgeDevice->SaveState();

    FX_RECT clip;
    clip.left = destLeft;
    clip.right = destRight;
    clip.top = destTop;
    clip.bottom = destBottom;
    fxgeDevice->SetClip_Rect(&clip);

    CPDF_RenderContext* pageContext = FX_NEW CPDF_RenderContext;
    pContext->m_pContext = pageContext;
    pageContext->Create(pPage);

    CFX_AffineMatrix matrix;
//    if (!transform) {
//        pPage->GetDisplayMatrix(matrix, destLeft, destTop, destRight - destLeft,
//                destBottom - destTop, 0);
//    } else {
        // PDF's coordinate system origin is left-bottom while
        // in graphics it is the top-left, so remap the origin.
        matrix.Set(1, 0, 0, -1, 0, pPage->GetPageHeight());

//        SkScalar transformValues[6];
//        transform->asAffine(transformValues);
//
//        matrix.Concat(transformValues[SkMatrix::kAScaleX], transformValues[SkMatrix::kASkewY],
//                transformValues[SkMatrix::kASkewX], transformValues[SkMatrix::kAScaleY],
//                transformValues[SkMatrix::kATransX], transformValues[SkMatrix::kATransY]);
//    }
    pageContext->AppendObjectList(pPage, &matrix);

    pContext->m_pRenderer = FX_NEW CPDF_ProgressiveRenderer;
    pContext->m_pRenderer->Start(pageContext, fxgeDevice, renderOptions, NULL);

    fxgeDevice->RestoreState();

    pPage->RemovePrivateData((void*) 1);

    delete pContext;
}

extern "C"
void Java_android_support_graphics_pdf_PdfRenderer_nativeRenderPage(JNIEnv* env, jclass thiz, jlong documentPtr, jlong pagePtr,
        jlong bitmapPtr, jint destLeft, jint destTop, jint destRight, jint destBottom,
        /*jlong matrixPtr,*/ jint renderMode) {

    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
//    SkBitmap* skBitmap = reinterpret_cast<SkBitmap*>(bitmapPtr);
//    SkMatrix* skMatrix = reinterpret_cast<SkMatrix*>(matrixPtr);

//    skBitmap->lockPixels();

//    const int stride = skBitmap->width() * 4;

    FPDF_BITMAP bitmap = NULL;//FPDFBitmap_CreateEx(skBitmap->width(), skBitmap->height(),
            //FPDFBitmap_BGRA, skBitmap->getPixels(), stride);

    if (!bitmap) {
        ALOGE("Erorr creating bitmap");
        return;
    }

    int renderFlags = 0;
    if (renderMode == RENDER_MODE_FOR_DISPLAY) {
        renderFlags |= FPDF_LCD_TEXT;
    } else if (renderMode == RENDER_MODE_FOR_PRINT) {
        renderFlags |= FPDF_PRINTING;
    }

    renderPageBitmap(bitmap, page, destLeft, destTop, destRight,
            destBottom, /*skMatrix,*/ renderFlags);

//    skBitmap->notifyPixelsChanged();
//    skBitmap->unlockPixels();
}

extern "C"
jint JNI_OnLoad(JavaVM* jvm, void*) {
	JNIEnv* env;
	if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
		return JNI_ERR;
	}

    jclass clazz = env->FindClass("android/graphics/Point");
    if (clazz == NULL) {
    	return JNI_ERR;
    }
    gPointClassInfo.x = env->GetFieldID(clazz, "x", "I");
    gPointClassInfo.y = env->GetFieldID(clazz, "y", "I");

	ALOGE("JNI_OnLoad");
	return JNI_VERSION_1_6;
}

};
