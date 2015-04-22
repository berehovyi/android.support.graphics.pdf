package vb.ua.supportpdfexamples;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.support.graphics.pdf.PdfRenderer;
import android.widget.ImageView;

import java.io.File;


public class MainActivity extends Activity {

    private ImageView imageView;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        imageView = (ImageView) findViewById(R.id.image_for_pdf);

        final File file = new File(Environment.getExternalStorageDirectory(), "example.pdf");
        try {
            final ParcelFileDescriptor parcelFileDescriptor = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
            final PdfRenderer pdfRenderer = new PdfRenderer(parcelFileDescriptor);

            final int pageCount = pdfRenderer.getPageCount();
            final PdfRenderer.Page page = pdfRenderer.openPage(6);
            final int width = page.getWidth();
            final int height = page.getHeight();

            final Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

            imageView.setBackgroundColor(0xffffffff);

            page.render(bitmap, null, PdfRenderer.Page.RENDER_MODE_FOR_PRINT);
            imageView.setImageBitmap(bitmap);

            System.err.println("PageCount: " + pageCount);
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }
}
