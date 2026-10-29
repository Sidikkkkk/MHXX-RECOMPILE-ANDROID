package com.nativeport.mhxx

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.citra.emu.NativeLibrary
import org.citra.emu.ui.MainActivity as CitraMainActivity
import org.citra.emu.utils.CitraDirectory
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

class SetupActivity : CitraMainActivity() {

    private var selectedRomUri: Uri? = null
    private var selectedUpdateUri: Uri? = null

    private lateinit var tvRomStatus: TextView
    private lateinit var tvUpdateStatus: TextView
    private lateinit var btnInstall: Button
    private lateinit var progressBar: ProgressBar
    private lateinit var tvProgress: TextView

    private val selectRomLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        uri?.let {
            selectedRomUri = it
            tvRomStatus.text = "File Terpilih: " + getFileName(it)
            tvRomStatus.setTextColor(0xFF4CAF50.toInt())
            btnInstall.isEnabled = true
        }
    }

    private val selectUpdateLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
        uri?.let {
            selectedUpdateUri = it
            tvUpdateStatus.text = "Update Terpilih: " + getFileName(it)
            tvUpdateStatus.setTextColor(0xFF4CAF50.toInt())
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CitraMainActivity.setInstance(this)

        // 1. Inisialisasi storage
        CitraDirectory.start(this)

        // Migrasi otomatis jika sebelumnya tersimpan sebagai game.bin
        val romfsDir = File(getExternalFilesDir(null), "romfs")
        if (!romfsDir.exists()) romfsDir.mkdirs()

        val oldBin = File(romfsDir, "game.bin")
        val game3ds = File(romfsDir, "game.3ds")
        if (oldBin.exists() && (!game3ds.exists() || game3ds.length() == 0L)) {
            oldBin.renameTo(game3ds)
        }

        // Cek apakah aset game sudah pernah di-install sebelumnya
        val romfsFlag = File(romfsDir, "installed.flag")
        if (romfsFlag.exists() && game3ds.exists() && game3ds.length() > 0L) {
            launchGame()
            return
        }

        setContentView(R.layout.activity_setup)
        enableImmersiveMode()

        tvRomStatus = findViewById(R.id.tv_rom_status)
        tvUpdateStatus = findViewById(R.id.tv_update_status)
        btnInstall = findViewById(R.id.btn_install_and_play)
        progressBar = findViewById(R.id.progress_bar)
        tvProgress = findViewById(R.id.tv_progress_text)

        findViewById<Button>(R.id.btn_select_rom).setOnClickListener {
            selectRomLauncher.launch("*/*")
        }

        findViewById<Button>(R.id.btn_select_update).setOnClickListener {
            selectUpdateLauncher.launch("*/*")
        }

        btnInstall.setOnClickListener {
            startExtractionPipeline()
        }
    }

    override fun updateProgress(name: String?, written: Long, total: Long) {
        if (total > 0) {
            val pct = ((written * 100) / total).toInt()
            runOnUiThread {
                progressBar.progress = pct
                tvProgress.text = "Menginstal CIA ($pct%)..."
            }
        }
    }

    private fun startExtractionPipeline() {
        val romUri = selectedRomUri ?: return

        btnInstall.isEnabled = false
        progressBar.visibility = View.VISIBLE
        tvProgress.visibility = View.VISIBLE
        tvProgress.text = "Menyiapkan instalasi aset ke penyimpanan lokal..."

        Thread {
            try {
                val romfsDir = File(getExternalFilesDir(null), "romfs")
                if (!romfsDir.exists()) romfsDir.mkdirs()

                // Salin ROM dengan ekstensi resmi .3ds yang dikenali Citra Loader
                val targetFile = File(romfsDir, "game.3ds")
                val inputStream: InputStream? = contentResolver.openInputStream(romUri)
                val outputStream = FileOutputStream(targetFile)

                val buffer = ByteArray(1024 * 1024 * 8) // 8 MB Buffer
                var bytesRead: Int

                runOnUiThread { tvProgress.text = "Menginstal aset game (RomFS / 3DS)..." }

                while (inputStream?.read(buffer).also { bytesRead = it ?: -1 } != -1) {
                    outputStream.write(buffer, 0, bytesRead)
                }

                outputStream.flush()
                outputStream.close()
                inputStream?.close()

                // Pasang Update jika ada
                selectedUpdateUri?.let { updateUri ->
                    runOnUiThread { tvProgress.text = "Menginstal patch update (.cia)..." }
                    val tempCia = File(cacheDir, "update.cia")
                    val ciaIn = contentResolver.openInputStream(updateUri)
                    val ciaOut = FileOutputStream(tempCia)
                    val ciaBuf = ByteArray(1024 * 1024 * 4)
                    var cRead: Int
                    while (ciaIn?.read(ciaBuf).also { cRead = it ?: -1 } != -1) {
                        ciaOut.write(ciaBuf, 0, cRead)
                    }
                    ciaOut.flush()
                    ciaOut.close()
                    ciaIn?.close()

                    NativeLibrary.InstallCIA(arrayOf(tempCia.absolutePath))
                    tempCia.delete()
                }

                // Buat flag penanda bahwa instalasi selesai
                File(romfsDir, "installed.flag").writeText("INSTALLED_OK")

                runOnUiThread {
                    Toast.makeText(this, "Instalasi Berhasil! Memulai Monster Hunter XX...", Toast.LENGTH_SHORT).show()
                    launchGame()
                }
            } catch (e: Exception) {
                runOnUiThread {
                    Toast.makeText(this, "Gagal menginstal aset: " + e.message, Toast.LENGTH_LONG).show()
                    btnInstall.isEnabled = true
                    progressBar.visibility = View.GONE
                    tvProgress.visibility = View.GONE
                }
            }
        }.start()
    }

    private fun launchGame() {
        val gameFile = File(getExternalFilesDir(null), "romfs/game.3ds")
        val intent = Intent(this, MainActivity::class.java).apply {
            putExtra("game_path", gameFile.absolutePath)
        }
        startActivity(intent)
        finish()
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

    private fun getFileName(uri: Uri): String {
        var name = "ROM_File.3ds"
        try {
            contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                val nameIndex = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
                if (nameIndex != -1 && cursor.moveToFirst()) {
                    name = cursor.getString(nameIndex)
                }
            }
        } catch (e: Exception) {
            name = uri.lastPathSegment ?: "ROM_File.3ds"
        }
        return name
    }
}
