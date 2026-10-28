package com.nativeport.mhxx

import android.annotation.SuppressLint
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.TextView
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

    // 3DS CTR Button Bitmasks
    companion object {
        const val KEY_A      = 1 shl 0
        const val KEY_B      = 1 shl 1
        const val KEY_SELECT = 1 shl 2
        const val KEY_START  = 1 shl 3
        const val KEY_RIGHT  = 1 shl 4
        const val KEY_LEFT   = 1 shl 5
        const val KEY_UP     = 1 shl 6
        const val KEY_DOWN   = 1 shl 7
        const val KEY_R      = 1 shl 8
        const val KEY_L      = 1 shl 9
        const val KEY_X      = 1 shl 10
        const val KEY_Y      = 1 shl 11
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Layar tetap menyala saat bermain
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // PENTING: setContentView terlebih dahulu sebelum WindowInsets!
        setContentView(R.layout.activity_main)
        enableImmersiveMode()

        val romfsDir = File(getExternalFilesDir(null), "romfs").absolutePath
        val saveDir = File(filesDir, "savedata").absolutePath

        // Inisialisasi engine
        NativeBridge.initNativeEngine(romfsDir, saveDir)

        // Setup OpenGL ES 3.2 View dari Layout XML
        glView = findViewById(R.id.gl_surface)
        glView.setEGLContextClientVersion(3)
        glView.setRenderer(object : GLSurfaceView.Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                NativeBridge.onSurfaceCreated(glView.width, glView.height)
            }

            override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) {
                NativeBridge.onSurfaceChanged(w, h)
            }

            override fun onDrawFrame(gl: GL10?) {
                NativeBridge.runFrame()
            }
        })
        glView.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY

        // Handle touch screen input untuk Bottom Screen 3DS
        glView.setOnTouchListener { _, event ->
            val w = if (glView.width > 0) glView.width.toFloat() else 1f
            val h = if (glView.height > 0) glView.height.toFloat() else 1f
            val normX = event.x / w
            val normY = event.y / h
            val isDown = event.action == MotionEvent.ACTION_DOWN || event.action == MotionEvent.ACTION_MOVE
            NativeBridge.sendTouchInput(normX, normY, isDown)
            true
        }

        // Setup Virtual Controller Button Listeners
        bindControllerButton(R.id.btn_a, KEY_A)
        bindControllerButton(R.id.btn_b, KEY_B)
        bindControllerButton(R.id.btn_x, KEY_X)
        bindControllerButton(R.id.btn_y, KEY_Y)
        bindControllerButton(R.id.btn_l, KEY_L)
        bindControllerButton(R.id.btn_r, KEY_R)
        bindControllerButton(R.id.btn_up, KEY_UP)
        bindControllerButton(R.id.btn_down, KEY_DOWN)
        bindControllerButton(R.id.btn_left, KEY_LEFT)
        bindControllerButton(R.id.btn_right, KEY_RIGHT)
        bindControllerButton(R.id.btn_start, KEY_START)
        bindControllerButton(R.id.btn_select, KEY_SELECT)
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun bindControllerButton(resId: Int, buttonMask: Int) {
        val btn = findViewById<TextView?>(resId) ?: return
        btn.setOnTouchListener { view, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    view.alpha = 0.5f
                    NativeBridge.sendButtonInput(buttonMask, true)
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    view.alpha = 1.0f
                    NativeBridge.sendButtonInput(buttonMask, false)
                    true
                }
                else -> false
            }
        }
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
        } catch (_: Exception) {
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
