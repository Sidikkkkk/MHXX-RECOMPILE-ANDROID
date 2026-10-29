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

    public void showInputBoxDialog(int maxLength, String error, String hint, String button0, String button1, String button2) {
        runOnUiThread(() -> {
            try {
                android.widget.EditText input = new android.widget.EditText(this);
                input.setHint(hint != null ? hint : "Nama Hunter");
                if (maxLength > 0) {
                    input.setFilters(new android.text.InputFilter[] { new android.text.InputFilter.LengthFilter(maxLength) });
                }
                input.setText("Hunter");
                input.setSelectAllOnFocus(true);
                input.setSingleLine(true);
                input.setTextColor(android.graphics.Color.WHITE);
                input.setHintTextColor(android.graphics.Color.GRAY);

                android.widget.FrameLayout container = new android.widget.FrameLayout(this);
                container.setPadding(50, 20, 50, 10);
                container.addView(input);

                String title = (error != null && !error.isEmpty()) ? error : ((hint != null && !hint.isEmpty()) ? hint : "Monster Hunter XX - Masukkan Nama");

                androidx.appcompat.app.AlertDialog.Builder builder = new androidx.appcompat.app.AlertDialog.Builder(this)
                        .setTitle(title)
                        .setView(container)
                        .setCancelable(false)
                        .setPositiveButton(button0 != null ? button0 : "OK", (d, w) -> {
                            String text = input.getText().toString().trim();
                            if (text.isEmpty()) text = "Hunter";
                            final String finalText = text;
                            new Thread(() -> org.citra.emu.NativeLibrary.KeyboardEvent(0, finalText)).start();
                        });

                if (button1 != null && !button1.isEmpty()) {
                    builder.setNegativeButton(button1, (d, w) -> {
                        new Thread(() -> org.citra.emu.NativeLibrary.KeyboardEvent(1, "")).start();
                    });
                }

                if (button2 != null && !button2.isEmpty()) {
                    builder.setNeutralButton(button2, (d, w) -> {
                        new Thread(() -> org.citra.emu.NativeLibrary.KeyboardEvent(2, "")).start();
                    });
                }

                builder.show();
            } catch (Exception e) {
                android.util.Log.e("citra", "showInputBoxDialog exception", e);
                new Thread(() -> org.citra.emu.NativeLibrary.KeyboardEvent(0, "Hunter")).start();
            }
        });
    }

    public void showMiiSelectorDialog(boolean cancel, String title, String[] miis) {
        runOnUiThread(() -> {
            try {
                if (miis == null || miis.length == 0) return;
                androidx.appcompat.app.AlertDialog.Builder builder = new androidx.appcompat.app.AlertDialog.Builder(this)
                        .setTitle(title != null ? title : "Pilih Mii")
                        .setCancelable(cancel)
                        .setItems(miis, (d, which) -> {
                            android.util.Log.i("citra", "Mii selected: " + which);
                        });
                if (cancel) {
                    builder.setNegativeButton("Batal", null);
                }
                builder.show();
            } catch (Exception e) {
                android.util.Log.e("citra", "showMiiSelectorDialog exception", e);
            }
        });
    }
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

