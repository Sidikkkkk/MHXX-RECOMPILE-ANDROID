package com.nativeport.mhxx.bridge

object NativeBridge {
    init {
        System.loadLibrary("mhxx_native")
    }

    var logCallback: ((String) -> Unit)? = null

    @JvmStatic
    fun postNativeLog(message: String) {
        logCallback?.invoke(message)
    }

    external fun initNativeEngine(romfsPath: String, savePath: String): Boolean
    external fun onSurfaceCreated(width: Int, height: Int)
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun runFrame()
    external fun sendTouchInput(x: Float, y: Float, isDown: Boolean)
    external fun sendButtonInput(buttonMask: Int, isDown: Boolean)
}
