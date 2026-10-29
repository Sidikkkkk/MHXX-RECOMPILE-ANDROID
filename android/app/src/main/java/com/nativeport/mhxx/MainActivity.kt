package com.nativeport.mhxx

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Bundle
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.citra.emu.NativeLibrary
import org.citra.emu.NativeLibrary.ButtonType
import org.citra.emu.ui.EmulationActivity
import org.citra.emu.utils.CitraDirectory
import java.io.File

class MainActivity : EmulationActivity() {

    private lateinit var gameSurface: SurfaceView
    private lateinit var tvLog: TextView
    private lateinit var scrollLog: ScrollView
    private lateinit var btnToggleLog: TextView
    private lateinit var tvFpsInfo: TextView

    private var emulationThread: Thread? = null
    private var isEmulating = false
    private var isSurfaceReady = false
    private var targetGamePath: String = ""

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Daftarkan activity singleton untuk JNI callbacks
        EmulationActivity.setInstance(this)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContentView(R.layout.activity_main)
        enableImmersiveMode()

        // 1. Inisialisasi Storage & Core Config
        CitraDirectory.start(this)

        // Migrasi path jika ada game.bin lama
        val romfsDir = File(getExternalFilesDir(null), "romfs")
        val legacyBin = File(romfsDir, "game.bin")
        val standard3ds = File(romfsDir, "game.3ds")
        if (legacyBin.exists() && (!standard3ds.exists() || standard3ds.length() == 0L)) {
            legacyBin.renameTo(standard3ds)
        }

        val passedPath = intent.getStringExtra("game_path")
        targetGamePath = when {
            passedPath != null && File(passedPath).exists() -> {
                if (passedPath.endsWith(".bin") && standard3ds.exists()) {
                    standard3ds.absolutePath
                } else {
                    passedPath
                }
            }
            standard3ds.exists() -> standard3ds.absolutePath
            legacyBin.exists() -> legacyBin.absolutePath
            else -> File(romfsDir, "game.3ds").absolutePath
        }

        tvLog = findViewById(R.id.tv_onscreen_log)
        scrollLog = findViewById(R.id.scroll_log)
        btnToggleLog = findViewById(R.id.btn_toggle_log)
        tvFpsInfo = findViewById(R.id.tv_fps_info)
        gameSurface = findViewById(R.id.game_surface)

        btnToggleLog.setOnClickListener {
            if (scrollLog.visibility == View.VISIBLE) {
                scrollLog.visibility = View.GONE
                btnToggleLog.text = "[ LOG: OFF ]"
                btnToggleLog.setTextColor(0xFF888888.toInt())
            } else {
                scrollLog.visibility = View.VISIBLE
                btnToggleLog.text = "[ LOG: ON ]"
                btnToggleLog.setTextColor(0xFF4CAF50.toInt())
            }
        }

        findViewById<TextView>(R.id.btn_reset_rom)?.setOnClickListener {
            val romfsFlag = File(getExternalFilesDir(null), "romfs/installed.flag")
            if (romfsFlag.exists()) romfsFlag.delete()
            stopEmulation()
            startActivity(Intent(this, SetupActivity::class.java))
            finish()
        }

        appendLog("[SYSTEM] MHXX Native Engine siap.")
        appendLog("[SYSTEM] Menyiapkan EGL Display & Surface...")

        // 2. Setup Surface Callback - Core HANYA dimulai setelah EGL ANativeWindow valid
        gameSurface.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                appendLog("[GLES] Surface diciptakan, mengaitkan ANativeWindow...")
                NativeLibrary.SurfaceChanged(holder.surface)
                isSurfaceReady = true
                maybeStartEmulation()
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                appendLog("[GLES] Resolusi layar aktif: ${width}x${height}")
                NativeLibrary.SurfaceChanged(holder.surface)
                isSurfaceReady = true
                if (!isEmulating) {
                    maybeStartEmulation()
                }
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                appendLog("[GLES] Surface dihancurkan.")
                isSurfaceReady = false
                NativeLibrary.SurfaceDestroyed()
            }
        })

        // 3. Forward touch events pada SurfaceView (Bottom Screen Touch)
        gameSurface.setOnTouchListener { _, event ->
            val action = when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> NativeLibrary.TouchEvent.TOUCH_PRESSED
                MotionEvent.ACTION_MOVE -> NativeLibrary.TouchEvent.TOUCH_MOVED
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> NativeLibrary.TouchEvent.TOUCH_RELEASED
                else -> return@setOnTouchListener false
            }
            NativeLibrary.TouchEvent(action, event.x.toInt(), event.y.toInt())
            true
        }

        // 4. Setup On-Screen Controller Buttons
        bindButton(findViewById(R.id.btn_a), ButtonType.N3DS_BUTTON_A)
        bindButton(findViewById(R.id.btn_b), ButtonType.N3DS_BUTTON_B)
        bindButton(findViewById(R.id.btn_x), ButtonType.N3DS_BUTTON_X)
        bindButton(findViewById(R.id.btn_y), ButtonType.N3DS_BUTTON_Y)
        bindButton(findViewById(R.id.btn_up), ButtonType.N3DS_DPAD_UP)
        bindButton(findViewById(R.id.btn_down), ButtonType.N3DS_DPAD_DOWN)
        bindButton(findViewById(R.id.btn_left), ButtonType.N3DS_DPAD_LEFT)
        bindButton(findViewById(R.id.btn_right), ButtonType.N3DS_DPAD_RIGHT)
        bindButton(findViewById(R.id.btn_l), ButtonType.N3DS_BUTTON_L)
        bindButton(findViewById(R.id.btn_r), ButtonType.N3DS_BUTTON_R)
        bindButton(findViewById(R.id.btn_start), ButtonType.N3DS_BUTTON_START)
        bindButton(findViewById(R.id.btn_select), ButtonType.N3DS_BUTTON_SELECT)
    }

    private fun maybeStartEmulation() {
        if (isEmulating) return
        if (!isSurfaceReady) {
            appendLog("[WAIT] Menunggu Surface EGL siap...")
            return
        }

        val gameFile = File(targetGamePath)
        if (!gameFile.exists()) {
            appendLog("[ERROR] File ROM tidak ditemukan di: $targetGamePath")
            return
        }

        if (!NativeLibrary.isValidFile(gameFile.name)) {
            appendLog("[WARN] Ekstensi bukan format 3ds/cci: ${gameFile.name}")
        }

        isEmulating = true
        appendLog("[BOOT] Memulai Citra MMJ Core untuk: ${gameFile.name} (${gameFile.length() / (1024 * 1024)} MB)")

        emulationThread = Thread({
            try {
                NativeLibrary.Run(gameFile.absolutePath)
                appendLog("[CORE] Emulasi game berhenti.")
            } catch (t: Throwable) {
                appendLog("[FATAL] Core Error: ${t.message}")
            } finally {
                isEmulating = false
            }
        }, "MHXX-CoreThread").apply { start() }
    }

    private fun stopEmulation() {
        if (!isEmulating) return
        isEmulating = false
        try {
            NativeLibrary.StopEmulation()
            NativeLibrary.SurfaceDestroyed()
        } catch (e: Exception) {
            // ignore
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun bindButton(view: View?, buttonType: Int) {
        view?.setOnTouchListener { v, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    NativeLibrary.InputEvent(buttonType, 1.0f)
                    v.isPressed = true
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    NativeLibrary.InputEvent(buttonType, 0.0f)
                    v.isPressed = false
                    true
                }
                else -> false
            }
        }
    }

    private fun appendLog(msg: String) {
        runOnUiThread {
            if (::tvLog.isInitialized) {
                tvLog.append("$msg\n")
                scrollLog.post { scrollLog.fullScroll(View.FOCUS_DOWN) }
            }
        }
    }

    private fun enableImmersiveMode() {
        try {
            WindowCompat.setDecorFitsSystemWindows(window, false)
            val controller = WindowCompat.getInsetsController(window, window.decorView)
            controller?.hide(WindowInsetsCompat.Type.systemBars())
            controller?.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } catch (e: Exception) {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            )
        }
    }

    override fun onResume() {
        super.onResume()
        EmulationActivity.setInstance(this)
        enableImmersiveMode()
        try {
            NativeLibrary.ResumeEmulation()
        } catch (e: Exception) {
            // ignore
        }
    }

    override fun onPause() {
        super.onPause()
        try {
            NativeLibrary.PauseEmulation()
        } catch (e: Exception) {
            // ignore
        }
    }

    override fun onDestroy() {
        stopEmulation()
        super.onDestroy()
    }
}
