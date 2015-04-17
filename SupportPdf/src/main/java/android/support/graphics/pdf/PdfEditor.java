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

package android.support.graphics.pdf;

import android.os.ParcelFileDescriptor;
import android.support.annotation.NonNull;

import java.io.IOException;

import dalvik.support.system.CloseGuard;

/**
 * Class for editing PDF files.
 *
 * @hide
 */
public final class PdfEditor {

    private final CloseGuard mCloseGuard = CloseGuard.get();

    private final long mNativeDocument;

    private int mPageCount;

    private ParcelFileDescriptor mInput;

    /**
     * Creates a new instance.
     * <p>
     * <strong>Note:</strong> The provided file descriptor must be <strong>seekable</strong>,
     * i.e. its data being randomly accessed, e.g. pointing to a file. After finishing
     * with this class you must call {@link #close()}.
     * </p>
     * <p>
     * <strong>Note:</strong> This class takes ownership of the passed in file descriptor
     * and is responsible for closing it when the editor is closed.
     * </p>
     *
     * @param input Seekable file descriptor to read from.
     *
     * @see #close()
     */
    public PdfEditor(@NonNull ParcelFileDescriptor input) throws IOException {
        if (input == null) {
            throw new NullPointerException("input cannot be null");
        }

        final long size = input.getStatSize();

        mInput = input;
        mNativeDocument = nativeOpen(mInput.getFd(), size);
        mPageCount = nativeGetPageCount(mNativeDocument);
        mCloseGuard.open("close");
    }

    /**
     * Gets the number of pages in the document.
     *
     * @return The page count.
     */
    public int getPageCount() {
        throwIfClosed();
        return mPageCount;
    }

    /**
     * Removes the page with a given index.
     *
     * @param pageIndex The page to remove.
     */
    public void removePage(int pageIndex) {
        throwIfClosed();
        throwIfPageNotInDocument(pageIndex);
        mPageCount = nativeRemovePage(mNativeDocument, pageIndex);
    }

    /**
     * Writes the PDF file to the provided destination.
     * <p>
     * <strong>Note:</strong> This method takes ownership of the passed in file
     * descriptor and is responsible for closing it when writing completes.
     * </p>
     * @param output The destination.
     */
    public void write(ParcelFileDescriptor output) throws IOException {
        try {
            throwIfClosed();
            nativeWrite(mNativeDocument, output.getFd());
        } finally {
            try {
                Class.forName("libcore.io.IoUtils").getDeclaredMethod("closeQuietly", AutoCloseable.class).invoke(null, output);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    /**
     * Closes this editor. You should not use this instance
     * after this method is called.
     */
    public void close() {
        throwIfClosed();
        doClose();
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            mCloseGuard.warnIfOpen();
            if (mInput != null) {
                doClose();
            }
        } finally {
            super.finalize();
        }
    }

    private void doClose() {
        nativeClose(mNativeDocument);
        try {
            Class.forName("libcore.io.IoUtils").getDeclaredMethod("closeQuietly", AutoCloseable.class).invoke(null, mInput);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        mInput = null;
        mCloseGuard.close();
    }

    private void throwIfClosed() {
        if (mInput == null) {
            throw new IllegalStateException("Already closed");
        }
    }

    private void throwIfPageNotInDocument(int pageIndex) {
        if (pageIndex < 0 || pageIndex >= mPageCount) {
            throw new IllegalArgumentException("Invalid page index");
        }
    }

    private static native long nativeOpen(int fd, long size);
    private static native void nativeClose(long documentPtr);
    private static native int nativeGetPageCount(long documentPtr);
    private static native int nativeRemovePage(long documentPtr, int pageIndex);
    private static native void nativeWrite(long documentPtr, int fd);
}
