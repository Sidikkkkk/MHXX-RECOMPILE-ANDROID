package com.nativeport.mhxx

import android.annotation.SuppressLint
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.nativeport.mhxx.bridge.NativeBridge
import java.io.File
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : AppCompatActivity() {

    private lateinit var glView: GLSurfaceView

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Layar tetap menyala saat bermain
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val romfsDir = File(getExternalFilesDir(null), "romfs").absolutePath
        val saveDir = File(filesDir, "savedata").absolutePath

        // Inisialisasi engine
        NativeBridge.initNativeEngine(romfsDir, saveDir)

        // Setup OpenGL ES 3.2 View
        glView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                    NativeBridge.onSurfaceCreated(width, height)
                }

                override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) {
                    NativeBridge.onSurfaceChanged(w, h)
                }

                override fun onDrawFrame(gl: GL10?) {
                    NativeBridge.runFrame()
                }
            })
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY

            // Handle touch events
            setOnTouchListener { _, event ->
                val w = if (width > 0) width.toFloat() else 1f
                val h = if (height > 0) height.toFloat() else 1f
                val normX = event.x / w
                val normY = event.y / h
                val isDown = event.action == MotionEvent.ACTION_DOWN || event.action == MotionEvent.ACTION_MOVE
                NativeBridge.sendTouchInput(normX, normY, isDown)
                true
            }
        }

        // PENTING: setContentView dipanggil SEBELUM mengaktifkan Immersive Mode!
        setContentView(glView)

        enableImmersiveMode()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enableImmersiveMode()
        }
    }

    override fun onResume() {
        super.onResume()
        if (::glView.isInitialized) glView.onResume()
    }

    override fun onPause() {
        super.onPause()
        if (::glView.isInitialized) glView.onPause()
    }

    private fun enableImmersiveMode() {
        try {
            WindowCompat.setDecorFitsSystemWindows(window, false)
            val controller = WindowInsetsControllerCompat(window, window.decorView)
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } catch (e: Exception) {
            // Fallback untuk custom ROM / MIUI lama
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            )
        }
    }
}
