package com.nativeport.mhxx

import android.annotation.SuppressLint
import android.app.AlertDialog
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.text.InputFilter
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.nativeport.mhxx.overlay.OverlayView
import org.citra.emu.NativeLibrary
import org.citra.emu.NativeLibrary.ButtonType
import org.citra.emu.ui.EmulationActivity
import org.citra.emu.utils.CitraDirectory
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader

class MainActivity : EmulationActivity() {

    private lateinit var gameSurface: SurfaceView
    private lateinit var controllerOverlay: OverlayView
    private lateinit var tvLog: TextView
    private lateinit var scrollLog: ScrollView
    private lateinit var btnToggleLog: TextView
    private lateinit var tvFpsInfo: TextView

    private var emulationThread: Thread? = null
    private var logcatThread: Thread? = null
    private var isEmulating = false
    private var isSurfaceReady = false
    private var isCapturingLog = true
    private var targetGamePath: String = ""

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Daftarkan activity singleton untuk JNI callbacks
        EmulationActivity.setInstance(this)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.setBackgroundDrawableResource(android.R.color.transparent)

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
        controllerOverlay = findViewById(R.id.controller_overlay)

        // SurfaceView z-ordering: media overlay false agar tidak menutupi window
        gameSurface.setZOrderMediaOverlay(false)

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

        appendLog("[SYSTEM] MHXX Native Engine aktif (Adreno 610 / GLES 3.2)")
        appendLog("[SYSTEM] Citra MMJ Full Touch Controls & Logcat aktif")

        // 2. Mulai Background Logcat Streamer untuk menampilkan log Citra langsung
        startLogcatCapture()

        // 3. Setup Surface Callback - Core HANYA dimulai setelah EGL ANativeWindow valid
        gameSurface.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                appendLog("[GLES] Surface diciptakan, mengaitkan ANativeWindow...")
                NativeLibrary.SurfaceChanged(holder.surface)
                isSurfaceReady = true
                maybeStartEmulation()
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                appendLog("[GLES] Resolusi layar: ${width}x${height}")
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
            if (::controllerOverlay.isInitialized) {
                controllerOverlay.releaseAll()
            }
            NativeLibrary.StopEmulation()
            NativeLibrary.SurfaceDestroyed()
        } catch (e: Exception) {
            // ignore
        }
    }

    // Dukungan Physical Gamepad (Bluetooth / USB / Android Joystick)
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val button = when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> ButtonType.N3DS_BUTTON_A
            KeyEvent.KEYCODE_BUTTON_B -> ButtonType.N3DS_BUTTON_B
            KeyEvent.KEYCODE_BUTTON_X -> ButtonType.N3DS_BUTTON_X
            KeyEvent.KEYCODE_BUTTON_Y -> ButtonType.N3DS_BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_L1 -> ButtonType.N3DS_BUTTON_L
            KeyEvent.KEYCODE_BUTTON_R1 -> ButtonType.N3DS_BUTTON_R
            KeyEvent.KEYCODE_BUTTON_L2 -> ButtonType.N3DS_BUTTON_ZL
            KeyEvent.KEYCODE_BUTTON_R2 -> ButtonType.N3DS_BUTTON_ZR
            KeyEvent.KEYCODE_BUTTON_START -> ButtonType.N3DS_BUTTON_START
            KeyEvent.KEYCODE_BUTTON_SELECT -> ButtonType.N3DS_BUTTON_SELECT
            KeyEvent.KEYCODE_DPAD_UP -> ButtonType.N3DS_DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> ButtonType.N3DS_DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> ButtonType.N3DS_DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> ButtonType.N3DS_DPAD_RIGHT
            else -> -1
        }
        if (button != -1) {
            val value = if (event.action == KeyEvent.ACTION_DOWN) 1.0f else 0.0f
            NativeLibrary.InputEvent(button, value)
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE) {
            val x = event.getAxisValue(MotionEvent.AXIS_X)
            val y = event.getAxisValue(MotionEvent.AXIS_Y)
            NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_X, x)
            NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_Y, -y)
            val zx = event.getAxisValue(MotionEvent.AXIS_Z)
            val rz = event.getAxisValue(MotionEvent.AXIS_RZ)
            NativeLibrary.InputEvent(ButtonType.N3DS_STICK_X, zx)
            NativeLibrary.InputEvent(ButtonType.N3DS_STICK_Y, -rz)
            return true
        }
        return super.dispatchGenericMotionEvent(event)
    }

    /**
     * Implementasi 3DS Software Keyboard Applet (Swkbd)
     * Dipanggil Citra saat pemilihan nama Hunter/Palico/Pesan
     */
    override fun showInputBoxDialog(
        maxLength: Int,
        error: String?,
        hint: String?,
        button0: String?,
        button1: String?,
        button2: String?
    ) {
        appendLog("[SWKBD] Dialog keyboard dibuka (max=$maxLength hint='$hint')")
        runOnUiThread {
            try {
                val inputView = EditText(this).apply {
                    this.hint = hint ?: "Masukkan Nama Hunter"
                    if (maxLength > 0) {
                        filters = arrayOf(InputFilter.LengthFilter(maxLength))
                    }
                    setText("Hunter")
                    setSelectAllOnFocus(true)
                    isSingleLine = true
                    setTextColor(Color.WHITE)
                    setHintTextColor(Color.GRAY)
                }

                val container = FrameLayout(this).apply {
                    setPadding(50, 20, 50, 10)
                    addView(inputView)
                }

                val title = when {
                    !error.isNullOrEmpty() -> error
                    !hint.isNullOrEmpty() -> hint
                    else -> "Monster Hunter XX - Masukkan Nama"
                }

                val dialog = AlertDialog.Builder(this)
                    .setTitle(title)
                    .setView(container)
                    .setCancelable(false)
                    .setPositiveButton(button0 ?: "OK") { _, _ ->
                        val text = inputView.text.toString().trim().ifEmpty { "Hunter" }
                        appendLog("[SWKBD] Nama disimpan: '$text'")
                        NativeLibrary.KeyboardEvent(0, text)
                        enableImmersiveMode()
                        try {
                            NativeLibrary.WindowChanged()
                            NativeLibrary.ResumeEmulation()
                        } catch (ignored: Exception) {}
                    }

                if (!button1.isNullOrEmpty()) {
                    dialog.setNegativeButton(button1) { _, _ ->
                        appendLog("[SWKBD] Batal dipilih")
                        NativeLibrary.KeyboardEvent(1, "")
                        enableImmersiveMode()
                        try {
                            NativeLibrary.WindowChanged()
                            NativeLibrary.ResumeEmulation()
                        } catch (ignored: Exception) {}
                    }
                }

                if (!button2.isNullOrEmpty()) {
                    dialog.setNeutralButton(button2) { _, _ ->
                        appendLog("[SWKBD] Tombol opsi 2 dipilih")
                        NativeLibrary.KeyboardEvent(2, "")
                        enableImmersiveMode()
                        try {
                            NativeLibrary.WindowChanged()
                            NativeLibrary.ResumeEmulation()
                        } catch (ignored: Exception) {}
                    }
                }

                dialog.setOnDismissListener {
                    enableImmersiveMode()
                    try {
                        NativeLibrary.WindowChanged()
                        NativeLibrary.ResumeEmulation()
                    } catch (ignored: Exception) {}
                }

                dialog.show()
            } catch (e: Exception) {
                appendLog("[SWKBD] Gagal menampilkan dialog: ${e.message}, fallback: 'Hunter'")
                NativeLibrary.KeyboardEvent(0, "Hunter")
                try {
                    NativeLibrary.ResumeEmulation()
                } catch (ignored: Exception) {}
            }
        }
    }

    override fun showMiiSelectorDialog(cancel: Boolean, title: String?, miis: Array<out String>?) {
        appendLog("[MII] Dialog Mii selector dipanggil (title='$title')")
        runOnUiThread {
            try {
                if (miis.isNullOrEmpty()) {
                    return@runOnUiThread
                }
                AlertDialog.Builder(this)
                    .setTitle(title ?: "Pilih Karakter Mii")
                    .setCancelable(cancel)
                    .setItems(miis) { _, which ->
                        appendLog("[MII] Mii dipilih: index $which")
                        enableImmersiveMode()
                        try {
                            NativeLibrary.WindowChanged()
                            NativeLibrary.ResumeEmulation()
                        } catch (ignored: Exception) {}
                    }
                    .setOnDismissListener {
                        enableImmersiveMode()
                        try {
                            NativeLibrary.WindowChanged()
                            NativeLibrary.ResumeEmulation()
                        } catch (ignored: Exception) {}
                    }
                    .show()
            } catch (e: Exception) {
                appendLog("[MII] Gagal menampilkan Mii: ${e.message}")
            }
        }
    }

    /**
     * Streaming logcat internal Citra C++ ke layar dan file citra_log.txt
     * Menggunakan filter --pid untuk membaca semua log tanpa diblokir SELinux Android 11
     */
    private fun startLogcatCapture() {
        logcatThread = Thread({
            var process: Process? = null
            var fileWriter: java.io.BufferedWriter? = null
            try {
                val logFile = File(getExternalFilesDir(null), "citra_log.txt")
                fileWriter = logFile.bufferedWriter()

                val myPid = android.os.Process.myPid()
                process = Runtime.getRuntime().exec(arrayOf("logcat", "-v", "time", "--pid=$myPid"))
                val reader = BufferedReader(InputStreamReader(process.inputStream))
                var line: String? = null

                while (isCapturingLog && reader.readLine().also { line = it } != null) {
                    val l = line ?: continue
                    fileWriter.write(l)
                    fileWriter.newLine()
                    fileWriter.flush()

                    if (::tvLog.isInitialized && ::scrollLog.isInitialized && scrollLog.visibility == View.VISIBLE) {
                        val clean = if (l.length > 110) l.substring(0, 110) + "..." else l
                        runOnUiThread {
                            if (scrollLog.visibility == View.VISIBLE) {
                                tvLog.append("$clean\n")
                                if (tvLog.lineCount > 250) {
                                    val cur = tvLog.text
                                    val idx = cur.indexOf('\n', 800)
                                    if (idx != -1) {
                                        tvLog.text = cur.substring(idx + 1)
                                    }
                                }
                                scrollLog.post { scrollLog.fullScroll(View.FOCUS_DOWN) }
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                // stream finished or interrupted
            } finally {
                try { fileWriter?.close() } catch (e: Exception) {}
                try { process?.destroy() } catch (e: Exception) {}
            }
        }, "CitraLogcatThread").apply {
            isDaemon = true
            start()
        }
    }

    private fun appendLog(msg: String) {
        runOnUiThread {
            if (::tvLog.isInitialized && ::scrollLog.isInitialized) {
                tvLog.append("$msg\n")
                if (scrollLog.visibility == View.VISIBLE) {
                    scrollLog.post { scrollLog.fullScroll(View.FOCUS_DOWN) }
                }
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

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enableImmersiveMode()
            try {
                NativeLibrary.WindowChanged()
                NativeLibrary.ResumeEmulation()
            } catch (e: Exception) {}
        }
    }

    override fun onResume() {
        super.onResume()
        EmulationActivity.setInstance(this)
        enableImmersiveMode()
        try {
            NativeLibrary.WindowChanged()
            NativeLibrary.ResumeEmulation()
        } catch (e: Exception) {
            // ignore
        }
    }

    override fun onPause() {
        super.onPause()
        if (::controllerOverlay.isInitialized) {
            controllerOverlay.releaseAll()
        }
        try {
            NativeLibrary.PauseEmulation()
        } catch (e: Exception) {
            // ignore
        }
    }

    override fun onDestroy() {
        isCapturingLog = false
        try {
            logcatThread?.interrupt()
        } catch (e: Exception) {}
        stopEmulation()
        super.onDestroy()
    }
}
