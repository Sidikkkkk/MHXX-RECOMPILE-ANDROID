package com.nativeport.mhxx.overlay

import org.citra.emu.NativeLibrary.ButtonType

object OverlayLayout {
    fun defaultControls(): List<OverlayControl> {
        val list = mutableListOf<OverlayControl>()

        // 1. Shoulder Buttons (Top edges)
        list += OverlayControl("l", OverlayControl.Kind.BUTTON, "L", ButtonType.N3DS_BUTTON_L, 0.08f, 0.08f, 0.11f)
        list += OverlayControl("zl", OverlayControl.Kind.BUTTON, "ZL", ButtonType.N3DS_BUTTON_ZL, 0.08f, 0.20f, 0.10f)

        list += OverlayControl("r", OverlayControl.Kind.BUTTON, "R", ButtonType.N3DS_BUTTON_R, 0.92f, 0.08f, 0.11f)
        list += OverlayControl("zr", OverlayControl.Kind.BUTTON, "ZR", ButtonType.N3DS_BUTTON_ZR, 0.92f, 0.20f, 0.10f)

        // 2. Analog Circle Pad (Movement Joystick - Left)
        list += OverlayControl("circle_pad", OverlayControl.Kind.CIRCLE_PAD, "", -1, 0.15f, 0.68f, 0.32f)

        // 3. 4-way D-Pad (Camera / Target - Lower Center Left)
        list += OverlayControl("dpad", OverlayControl.Kind.DPAD, "", -1, 0.33f, 0.83f, 0.22f)

        // 4. Action Buttons (Diamond - Right)
        // X (Top)
        list += OverlayControl("x", OverlayControl.Kind.BUTTON, "X", ButtonType.N3DS_BUTTON_X, 0.88f, 0.54f, 0.12f)
        // Y (Left)
        list += OverlayControl("y", OverlayControl.Kind.BUTTON, "Y", ButtonType.N3DS_BUTTON_Y, 0.80f, 0.66f, 0.12f)
        // A (Right)
        list += OverlayControl("a", OverlayControl.Kind.BUTTON, "A", ButtonType.N3DS_BUTTON_A, 0.95f, 0.66f, 0.12f)
        // B (Bottom)
        list += OverlayControl("b", OverlayControl.Kind.BUTTON, "B", ButtonType.N3DS_BUTTON_B, 0.88f, 0.78f, 0.12f)

        // 5. C-Stick (Right analog camera stick - Lower Center Right)
        list += OverlayControl("c_stick", OverlayControl.Kind.C_STICK, "", -1, 0.68f, 0.83f, 0.22f)

        // 6. System Buttons (Bottom center)
        list += OverlayControl("select", OverlayControl.Kind.BUTTON, "SELECT", ButtonType.N3DS_BUTTON_SELECT, 0.44f, 0.94f, 0.07f)
        list += OverlayControl("start", OverlayControl.Kind.BUTTON, "START", ButtonType.N3DS_BUTTON_START, 0.56f, 0.94f, 0.07f)

        return list
    }
}
