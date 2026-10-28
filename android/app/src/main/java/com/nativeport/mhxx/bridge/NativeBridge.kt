package com.nativeport.mhxx.bridge

object NativeBridge {
    init {
        System.loadLibrary("mhxx_native")
    }

    external fun initNativeEngine(romfsPath: String, savePath: String): Boolean
    external fun onSurfaceCreated(width: Int, height: Int)
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun runFrame()
    external fun sendTouchInput(x: Float, y: Float, isDown: Boolean)
    external fun sendButtonInput(buttonMask: Int, isDown: Boolean)
}

