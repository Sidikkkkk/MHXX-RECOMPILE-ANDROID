package com.nativeport.mhxx.overlay

import org.citra.emu.NativeLibrary.ButtonType

object OverlayLayout {
    fun defaultControls(): List<OverlayControl> {
        val list = mutableListOf<OverlayControl>()

        // 1. Shoulder Buttons (Top edges)
        list += OverlayControl("l", OverlayControl.Kind.BUTTON, "L", ButtonType.N3DS_BUTTON_L, 0.08f, 0.10f, 0.14f)
        list += OverlayControl("zl", OverlayControl.Kind.BUTTON, "ZL", ButtonType.N3DS_BUTTON_ZL, 0.08f, 0.24f, 0.13f)

        list += OverlayControl("r", OverlayControl.Kind.BUTTON, "R", ButtonType.N3DS_BUTTON_R, 0.92f, 0.10f, 0.14f)
        list += OverlayControl("zr", OverlayControl.Kind.BUTTON, "ZR", ButtonType.N3DS_BUTTON_ZR, 0.92f, 0.24f, 0.13f)

        // 2. Analog Circle Pad (Movement Joystick - Left)
        list += OverlayControl("circle_pad", OverlayControl.Kind.CIRCLE_PAD, "", -1, 0.16f, 0.70f, 0.36f)

        // 3. 4-way D-Pad (Camera / Target - Lower Center Left)
        list += OverlayControl("dpad", OverlayControl.Kind.DPAD, "", -1, 0.35f, 0.80f, 0.28f)

        // 4. Action Buttons (Diamond - Right)
        // X (Top)
        list += OverlayControl("x", OverlayControl.Kind.BUTTON, "X", ButtonType.N3DS_BUTTON_X, 0.86f, 0.52f, 0.16f)
        // Y (Left)
        list += OverlayControl("y", OverlayControl.Kind.BUTTON, "Y", ButtonType.N3DS_BUTTON_Y, 0.77f, 0.65f, 0.16f)
        // A (Right)
        list += OverlayControl("a", OverlayControl.Kind.BUTTON, "A", ButtonType.N3DS_BUTTON_A, 0.95f, 0.65f, 0.16f)
        // B (Bottom)
        list += OverlayControl("b", OverlayControl.Kind.BUTTON, "B", ButtonType.N3DS_BUTTON_B, 0.86f, 0.78f, 0.16f)

        // 5. C-Stick (Right analog camera stick - Lower Center Right)
        list += OverlayControl("c_stick", OverlayControl.Kind.C_STICK, "", -1, 0.68f, 0.80f, 0.26f)

        // 6. System Buttons (Bottom center)
        list += OverlayControl("select", OverlayControl.Kind.BUTTON, "SELECT", ButtonType.N3DS_BUTTON_SELECT, 0.44f, 0.93f, 0.09f)
        list += OverlayControl("start", OverlayControl.Kind.BUTTON, "START", ButtonType.N3DS_BUTTON_START, 0.56f, 0.93f, 0.09f)

        return list
    }
}

