package com.nativeport.mhxx

import android.annotation.SuppressLint
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import androidx.appcompat.app.AppCompatActivity
import com.nativeport.mhxx.bridge.NativeBridge
import java.io.File
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : AppCompatActivity() {

    private lateinit var glView: GLSurfaceView

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableImmersiveMode()

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

                override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                    NativeBridge.onSurfaceChanged(width, height)
                }

                override fun onDrawFrame(gl: GL10?) {
                    NativeBridge.runFrame()
                }
            })
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY

            // Handle touch events
            setOnTouchListener { _, event ->
                val normX = event.x / width.toFloat()
                val normY = event.y / height.toFloat()
                val isDown = event.action == MotionEvent.ACTION_DOWN || event.action == MotionEvent.ACTION_MOVE
                NativeBridge.sendTouchInput(normX, normY, isDown)
                true
            }
        }

        setContentView(glView)
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
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            window.insetsController?.let { controller ->
                controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                controller.systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
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

