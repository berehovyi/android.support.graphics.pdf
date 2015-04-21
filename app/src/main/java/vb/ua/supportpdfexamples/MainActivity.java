package vb.ua.supportpdfexamples;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.support.graphics.pdf.PdfRenderer;
import android.widget.ImageView;

import java.io.File;
import java.io.IOException;


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
            PdfRenderer pdfRenderer = new PdfRenderer(parcelFileDescriptor);

            final int pageCount = pdfRenderer.getPageCount();
            final PdfRenderer.Page page = pdfRenderer.openPage(0);
            final Bitmap bitmap = Bitmap.createBitmap(page.getWidth(), page.getHeight(), Bitmap.Config.ARGB_8888);

            page.render(bitmap, null, null, PdfRenderer.Page.RENDER_MODE_FOR_DISPLAY);
            imageView.setImageBitmap(bitmap);

            System.err.println("PageCount: " + pageCount);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
