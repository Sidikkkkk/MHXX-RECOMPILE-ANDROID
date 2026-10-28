# Monster Hunter Double Cross (MHXX) - Android Native Port

Proyek recompile & native porting untuk **Monster Hunter Double Cross (Nintendo 3DS)** ke arsitektur **Android ARM64 (arm64-v8a)**.

---

## 📁 Struktur Repositori & Lokasi File

Berikut adalah struktur folder kerja proyek ini:

```text
mhxx-android/
│
├── inputs/                        <-- [TEMPAT ANDA MENYIMPAN FILE GAME]
│   ├── rom/                       <-- Simpan file 'MHXX.3ds' atau 'MHXX.cia' di sini
│   ├── updates/                   <-- Simpan file update 'v1.4.0.cia' di sini
│   └── patches/                   <-- Patch terjemahan / mod 60fps (opsional)
│
├── extracted/                     <-- [HASIL EKSTRAKSI OTOMATIS]
│   ├── exefs/                     <-- Berisi code.bin (biner ARM11 game)
│   ├── romfs/                     <-- Aset game (model 3D, suara, tekstur)
│   └── updates/                   <-- Aset & patch update v1.4.0
│
├── tools/                         <-- Tooling ekstraksi & pemrosesan biner
│   ├── extract_game.ps1           <-- Script ekstraksi untuk Windows PowerShell
│   ├── extract_game.py            <-- Script ekstraksi Python
│   └── requirements.txt
│
├── core/                          <-- Mesin Native C++ (Android NDK)
│   ├── CMakeLists.txt             <-- Konfigurasi build CMake
│   ├── include/                   <-- Header HLE & Recompiler
│   └── src/                       <-- Implementasi syscall CTR & JNI Bridge
│
├── android/                       <-- Proyek Android Studio (Gradle / Kotlin)
│   ├── app/                       <-- Source code aplikasi Android
│   └── build.gradle.kts
│
└── .github/workflows/             <-- GitHub Actions CI/CD (Otomatis Build APK)
    └── build-android.yml
```

---

## 🚀 Panduan Memulai

### Langkah 1: Letakkan File Game Anda
1. Salin file ROM MHXX Anda (`.3ds` atau `.cia` yang sudah di-dekripsi) ke:
   ```
   mhxx-android/inputs/rom/
   ```
2. (Opsional namun disarankan) Salin file update MHXX v1.4.0 (`.cia`) ke:
   ```
   mhxx-android/inputs/updates/
   ```

### Langkah 2: Jalankan Ekstraksi Aset & Biner
Buka terminal PowerShell pada folder proyek ini, lalu jalankan:
```powershell
powershell -ExecutionPolicy Bypass -File .\tools\extract_game.ps1
```
Script akan otomatis memeriksa integritas file, memverifikasi status dekripsi, dan mengekstrak `code.bin` ke dalam folder `extracted/exefs/`.

### Langkah 3: Mengunggah ke GitHub
Untuk menghubungkan proyek ini ke repositori GitHub pribadi Anda:
```powershell
git init
git add .
git commit -m "Initial commit: MHXX Android Native Port structure"
git branch -M main
git remote add origin https://github.com/USERNAME/NAMA-REPO-ANDA.git
git push -u origin main
```
> [!NOTE]
> File `.gitignore` telah dikonfigurasi untuk secara otomatis mengabaikan file `.3ds`, `.cia`, dan `code.bin` Capcom sehingga repositori Anda aman dari pelanggaran hak cipta (*DMCA*).

---

## 🛠️ Build APK Android
Proyek ini dapat langsung dibuka menggunakan **Android Studio**:
1. Buka Android Studio $\rightarrow$ **Open** $\rightarrow$ pilih folder `android/`.
2. Pastikan Android NDK (r26b+) dan CMake terpasang di Android Studio SDK Manager.
3. Klik **Build** $\rightarrow$ **Build Bundle(s) / APK(s)** $\rightarrow$ **Build APK(s)**.

