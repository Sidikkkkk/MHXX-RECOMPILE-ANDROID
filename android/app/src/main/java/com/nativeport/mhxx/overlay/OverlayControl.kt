package com.nativeport.mhxx.overlay

data class OverlayControl(
    val id: String,
    val kind: Kind,
    val label: String,
    val buttonType: Int = -1,
    var x: Float,
    var y: Float,
    var size: Float,
    var visible: Boolean = true
) {
    enum class Kind {
        BUTTON,
        DPAD,
        CIRCLE_PAD,
        C_STICK
    }
}
