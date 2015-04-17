package vb.ua.supportpdfexamples;

import android.app.Activity;
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
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
