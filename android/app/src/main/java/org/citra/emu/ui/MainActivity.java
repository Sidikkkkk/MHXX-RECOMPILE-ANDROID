package org.citra.emu.ui;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import java.lang.ref.WeakReference;

public class MainActivity extends AppCompatActivity {
    protected static WeakReference<MainActivity> sInstance = new WeakReference<>(null);

    public static MainActivity get() {
        return sInstance != null ? sInstance.get() : null;
    }

    public static void setInstance(MainActivity activity) {
        sInstance = new WeakReference<>(activity);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = new WeakReference<>(this);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (sInstance != null && sInstance.get() == this) {
            sInstance = new WeakReference<>(null);
        }
    }

    public void updateProgress(String name, long written, long total) {}
}

