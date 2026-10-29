package org.citra.emu.utils;

import android.content.Context;
import android.os.Build;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.citra.emu.NativeLibrary;

public final class CitraDirectory {
    private static boolean sInitialized = false;
    private static String mUserPath = "";

    public static synchronized void start(Context context) {
        if (sInitialized) {
            return;
        }

        File externalPath = context.getExternalFilesDir(null);
        if (externalPath == null) {
            externalPath = context.getFilesDir();
        }

        File userPath = new File(externalPath, "citra-emu");
        if (!userPath.exists()) {
            userPath.mkdirs();
        }

        mUserPath = userPath.getAbsolutePath();
        Log.i("citra", "CitraDirectory userPath: " + mUserPath);

        // Tell native core where user data lives
        NativeLibrary.SetUserPath(mUserPath);

        // Extract assets in background or sync
        initializeStorage(context);

        sInitialized = true;
    }

    public static boolean isInitialized() {
        return sInitialized;
    }

    public static String getUserDirectory() {
        return mUserPath;
    }

    public static String getConfigDirectory() {
        return mUserPath + File.separator + "config";
    }

    public static String getConfigFile() {
        return getConfigDirectory() + File.separator + "config-mmj.ini";
    }

    public static String getShadersDirectory() {
        return mUserPath + File.separator + "shaders";
    }

    public static String getSysDataDirectory() {
        return mUserPath + File.separator + "sysdata";
    }

    public static String getNandDirectory() {
        return mUserPath + File.separator + "nand";
    }

    public static String getSDMCDirectory() {
        return mUserPath + File.separator + "sdmc";
    }

    private static void initializeStorage(Context context) {
        try {
            File config = new File(getConfigDirectory());
            File shaders = new File(getShadersDirectory());
            File sysdata = new File(getSysDataDirectory());
            File nand = new File(getNandDirectory());
            File sdmc = new File(getSDMCDirectory());

            config.mkdirs();
            shaders.mkdirs();
            sysdata.mkdirs();
            nand.mkdirs();
            sdmc.mkdirs();

            copyAssetFolder("shaders", shaders, true, context);
            copyAssetFolder("sysdata", sysdata, true, context);
            copyAssetFolder("config", config, true, context);
            copyAssetFolder("nand", nand, false, context);
            copyAssetFolder("sdmc", sdmc, false, context);
        } catch (Exception e) {
            Log.e("citra", "initializeStorage error", e);
        }
    }

    private static void copyAsset(String asset, File output, boolean overwrite, Context context) {
        try {
            if (!output.exists() || overwrite) {
                InputStream in = context.getAssets().open(asset);
                OutputStream out = new FileOutputStream(output);
                copyFile(in, out);
                in.close();
                out.close();
            }
        } catch (IOException e) {
            // ignore missing subfiles
        }
    }

    private static void copyAssetFolder(String assetFolder, File outputFolder, boolean overwrite, Context context) {
        try {
            String[] files = context.getAssets().list(assetFolder);
            if (files == null || files.length == 0) {
                return;
            }
            if (!outputFolder.exists()) {
                outputFolder.mkdirs();
            }
            for (String file : files) {
                String subAsset = assetFolder + "/" + file;
                File subOut = new File(outputFolder, file);
                String[] subList = context.getAssets().list(subAsset);
                if (subList != null && subList.length > 0) {
                    copyAssetFolder(subAsset, subOut, overwrite, context);
                } else {
                    copyAsset(subAsset, subOut, overwrite, context);
                }
            }
        } catch (IOException e) {
            Log.e("citra", "copyAssetFolder error: " + assetFolder, e);
        }
    }

    public static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[8192];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }
}

