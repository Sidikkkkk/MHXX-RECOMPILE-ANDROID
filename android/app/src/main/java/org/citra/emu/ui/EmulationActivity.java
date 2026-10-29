package org.citra.emu.ui;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import java.lang.ref.WeakReference;

public class EmulationActivity extends AppCompatActivity {
    protected static WeakReference<EmulationActivity> sInstance = new WeakReference<>(null);

    public static EmulationActivity get() {
        return sInstance != null ? sInstance.get() : null;
    }

    public static void setInstance(EmulationActivity activity) {
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

    public void showInputBoxDialog(int maxLength, String error, String hint, String button0, String button1, String button2) {}
    public void showMiiSelectorDialog(boolean cancel, String title, String[] miis) {}
    public void handleNFCScanning(boolean isScanning) {}
    public void updateProgress(String name, long written, long total) {}
    public void pickImage(int width, int height) {}
    public boolean checkPermission(String permission) { return true; }
    public int getDisplayRotation() { return 0; }
    public int getSafeInsetLeft() { return 0; }
    public int getSafeInsetTop() { return 0; }
    public int getSafeInsetRight() { return 0; }
    public int getSafeInsetBottom() { return 0; }
    public float getScaleDensity() {
        try {
            return getResources().getDisplayMetrics().density;
        } catch (Exception e) {
            return 1.0f;
        }
    }
    public void showRunningSetting() {}
}
