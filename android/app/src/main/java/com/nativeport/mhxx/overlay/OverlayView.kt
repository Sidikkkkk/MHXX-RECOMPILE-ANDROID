package com.nativeport.mhxx.overlay

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import org.citra.emu.NativeLibrary
import org.citra.emu.NativeLibrary.ButtonType
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

/**
 * Controller overlay berkinerja tinggi untuk Monster Hunter XX Native Android.
 * Menggambar seluruh tombol virtual (Circle Pad, C-Stick, D-Pad, ABXY, L/R, ZL/ZR, Start/Select)
 * menggunakan hardware-accelerated Canvas langsung di atas SurfaceView tanpa membuat child View.
 * Sentuhan di luar tombol akan diteruskan sebagai sentuhan Stylus Touchscreen 3DS.
 */
class OverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    var controls: List<OverlayControl> = OverlayLayout.defaultControls()
    var isControlsVisible: Boolean = true
        set(value) {
            field = value
            if (!value) releaseAll()
            invalidate()
        }

    var opacity: Float = 0.70f
        set(value) {
            field = value.coerceIn(0.2f, 1.0f)
            invalidate()
        }

    var isHapticBuzzEnabled: Boolean = true

    private val pointerTargets = HashMap<Int, OverlayControl>()
    private val stickValues = HashMap<String, FloatArray>() // dx, dy in [-1.0 .. 1.0]
    private val heldButtons = HashSet<OverlayControl>()
    private var dpadState = 0

    // Paint resources
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3.5f
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    init {
        // Overlay view harus transparan penuh agar tidak menutupi SurfaceView
        setBackgroundColor(Color.TRANSPARENT)
    }

    private fun cx(c: OverlayControl): Float = c.x * width
    private fun cy(c: OverlayControl): Float = c.y * height
    private fun radius(c: OverlayControl): Float = c.size * height / 2.0f

    private fun hit(c: OverlayControl, x: Float, y: Float): Boolean {
        val r = radius(c)
        val tolerance = when (c.kind) {
            OverlayControl.Kind.CIRCLE_PAD -> r * 1.35f
            OverlayControl.Kind.C_STICK -> r * 1.35f
            OverlayControl.Kind.DPAD -> r * 1.30f
            OverlayControl.Kind.BUTTON -> {
                if (c.id == "start" || c.id == "select") r * 1.6f + 25f
                else r * 1.45f + 30f
            }
        }
        return hypot(x - cx(c), y - cy(c)) <= tolerance
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val index = event.actionIndex
                handlePointerDown(event, index)
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    handlePointerMove(event, i)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val index = event.actionIndex
                handlePointerUp(event, index)
            }
            MotionEvent.ACTION_CANCEL -> {
                releaseAll()
            }
        }
        invalidate()
        return true
    }

    private fun handlePointerDown(event: MotionEvent, index: Int) {
        val id = event.getPointerId(index)
        val x = event.getX(index)
        val y = event.getY(index)

        val hitControl = if (isControlsVisible) {
            controls.filter { it.visible && hit(it, x, y) }
                .minByOrNull { hypot(x - cx(it), y - cy(it)) }
        } else null

        if (hitControl != null) {
            pointerTargets[id] = hitControl
            buzz()
            when (hitControl.kind) {
                OverlayControl.Kind.BUTTON -> {
                    heldButtons.add(hitControl)
                    NativeLibrary.InputEvent(hitControl.buttonType, 1.0f)
                }
                OverlayControl.Kind.CIRCLE_PAD, OverlayControl.Kind.C_STICK -> {
                    updateStick(hitControl, x, y)
                }
                OverlayControl.Kind.DPAD -> {
                    updateDpadFromTouch(hitControl, x, y)
                }
            }
        }
        // Catatan Penting: JANGAN memanggil NativeLibrary.TouchEvent untuk sentuhan ambient di luar kontrol!
        // Citra MMJ menganggap ketukan pada layar atas sebagai gestur swap screen yang menyebabkan layar
        // berganti menjadi hitam (karena layar bawah MHXX hitam/kosong pada tampilan awal).
    }

    private fun handlePointerMove(event: MotionEvent, index: Int) {
        val id = event.getPointerId(index)
        val x = event.getX(index)
        val y = event.getY(index)

        val control = pointerTargets[id] ?: return
        when (control.kind) {
            OverlayControl.Kind.CIRCLE_PAD, OverlayControl.Kind.C_STICK -> {
                updateStick(control, x, y)
            }
            OverlayControl.Kind.DPAD -> {
                updateDpadFromTouch(control, x, y)
            }
            OverlayControl.Kind.BUTTON -> {}
        }
    }

    private fun handlePointerUp(event: MotionEvent, index: Int) {
        val id = event.getPointerId(index)
        val control = pointerTargets.remove(id) ?: return
        when (control.kind) {
            OverlayControl.Kind.BUTTON -> {
                heldButtons.remove(control)
                NativeLibrary.InputEvent(control.buttonType, 0.0f)
            }
            OverlayControl.Kind.CIRCLE_PAD -> {
                stickValues.remove(control.id)
                NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_X, 0.0f)
                NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_Y, 0.0f)
            }
            OverlayControl.Kind.C_STICK -> {
                stickValues.remove(control.id)
                NativeLibrary.InputEvent(ButtonType.N3DS_STICK_X, 0.0f)
                NativeLibrary.InputEvent(ButtonType.N3DS_STICK_Y, 0.0f)
            }
            OverlayControl.Kind.DPAD -> {
                updateDpad(0)
            }
        }
    }

    private fun updateStick(c: OverlayControl, x: Float, y: Float) {
        val r = radius(c)
        var dx = (x - cx(c)) / r
        var dy = (y - cy(c)) / r
        val dist = hypot(dx, dy)
        if (dist > 1.0f) {
            dx /= dist
            dy /= dist
        }
        stickValues[c.id] = floatArrayOf(dx, dy)

        // Pada Citra 3DS: Sumbu X positif = Kanan, Sumbu Y positif = Atas
        val emuX = dx
        val emuY = -dy

        if (c.kind == OverlayControl.Kind.CIRCLE_PAD) {
            NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_X, emuX)
            NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_Y, emuY)
        } else {
            NativeLibrary.InputEvent(ButtonType.N3DS_STICK_X, emuX)
            NativeLibrary.InputEvent(ButtonType.N3DS_STICK_Y, emuY)
        }
    }

    private fun updateDpadFromTouch(c: OverlayControl, x: Float, y: Float) {
        val r = radius(c)
        val dx = (x - cx(c)) / r
        val dy = (y - cy(c)) / r
        val dist = hypot(dx, dy)

        var mask = 0
        if (dist >= 0.15f) {
            if (dy < -0.30f) mask = mask or DPAD_UP
            if (dy > 0.30f) mask = mask or DPAD_DOWN
            if (dx < -0.30f) mask = mask or DPAD_LEFT
            if (dx > 0.30f) mask = mask or DPAD_RIGHT
        }
        updateDpad(mask)
    }

    private fun updateDpad(mask: Int) {
        val changed = dpadState xor mask
        if (changed and DPAD_UP != 0) {
            NativeLibrary.InputEvent(ButtonType.N3DS_DPAD_UP, if (mask and DPAD_UP != 0) 1.0f else 0.0f)
        }
        if (changed and DPAD_DOWN != 0) {
            NativeLibrary.InputEvent(ButtonType.N3DS_DPAD_DOWN, if (mask and DPAD_DOWN != 0) 1.0f else 0.0f)
        }
        if (changed and DPAD_LEFT != 0) {
            NativeLibrary.InputEvent(ButtonType.N3DS_DPAD_LEFT, if (mask and DPAD_LEFT != 0) 1.0f else 0.0f)
        }
        if (changed and DPAD_RIGHT != 0) {
            NativeLibrary.InputEvent(ButtonType.N3DS_DPAD_RIGHT, if (mask and DPAD_RIGHT != 0) 1.0f else 0.0f)
        }
        dpadState = mask
    }

    fun releaseAll() {
        for (c in heldButtons) {
            NativeLibrary.InputEvent(c.buttonType, 0.0f)
        }
        heldButtons.clear()
        stickValues.clear()
        pointerTargets.clear()
        updateDpad(0)
        NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_X, 0.0f)
        NativeLibrary.InputEvent(ButtonType.N3DS_CPAD_Y, 0.0f)
        NativeLibrary.InputEvent(ButtonType.N3DS_STICK_X, 0.0f)
        NativeLibrary.InputEvent(ButtonType.N3DS_STICK_Y, 0.0f)
        invalidate()
    }

    private fun buzz() {
        if (!isHapticBuzzEnabled) return
        try {
            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        } catch (e: Exception) {
            try {
                performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
            } catch (ignored: Exception) {}
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!isControlsVisible) return

        val alpha = (opacity * 255).toInt().coerceIn(20, 255)

        for (c in controls) {
            if (!c.visible) continue
            val r = radius(c)
            val isPressed = heldButtons.contains(c) || stickValues.containsKey(c.id)

            when (c.kind) {
                OverlayControl.Kind.CIRCLE_PAD -> drawCirclePad(canvas, c, r, alpha)
                OverlayControl.Kind.C_STICK -> drawCStick(canvas, c, r, alpha)
                OverlayControl.Kind.DPAD -> drawDpad(canvas, c, r, alpha)
                OverlayControl.Kind.BUTTON -> drawButton(canvas, c, r, isPressed, alpha)
            }
        }
    }

    private fun drawCirclePad(canvas: Canvas, c: OverlayControl, r: Float, alpha: Int) {
        val x = cx(c)
        val y = cy(c)

        // Outer base ring
        fillPaint.color = COLOR_BG_IDLE
        fillPaint.alpha = (alpha * 0.45f).toInt()
        canvas.drawCircle(x, y, r, fillPaint)

        strokePaint.color = COLOR_AMBER
        strokePaint.alpha = alpha
        strokePaint.strokeWidth = 3f
        canvas.drawCircle(x, y, r, strokePaint)

        // Center deadzone mark
        fillPaint.color = COLOR_AMBER
        fillPaint.alpha = (alpha * 0.35f).toInt()
        canvas.drawCircle(x, y, r * 0.15f, fillPaint)

        // Draggable joystick thumb nub
        val stick = stickValues[c.id]
        val nubX = x + (stick?.get(0) ?: 0.0f) * r * 0.65f
        val nubY = y + (stick?.get(1) ?: 0.0f) * r * 0.65f
        val nubRadius = r * 0.40f

        fillPaint.color = if (stick != null) COLOR_AMBER_BRIGHT else COLOR_AMBER
        fillPaint.alpha = if (stick != null) min(255, alpha + 60) else (alpha * 0.75f).toInt()
        canvas.drawCircle(nubX, nubY, nubRadius, fillPaint)

        strokePaint.color = COLOR_TEXT
        strokePaint.alpha = alpha
        strokePaint.strokeWidth = 2.5f
        canvas.drawCircle(nubX, nubY, nubRadius, strokePaint)
    }

    private fun drawCStick(canvas: Canvas, c: OverlayControl, r: Float, alpha: Int) {
        val x = cx(c)
        val y = cy(c)

        // Outer ring
        fillPaint.color = COLOR_BG_IDLE
        fillPaint.alpha = (alpha * 0.45f).toInt()
        canvas.drawCircle(x, y, r, fillPaint)

        strokePaint.color = COLOR_AMBER
        strokePaint.alpha = alpha
        strokePaint.strokeWidth = 3f
        canvas.drawCircle(x, y, r, strokePaint)

        // Nub
        val stick = stickValues[c.id]
        val nubX = x + (stick?.get(0) ?: 0.0f) * r * 0.65f
        val nubY = y + (stick?.get(1) ?: 0.0f) * r * 0.65f
        val nubRadius = r * 0.40f

        fillPaint.color = if (stick != null) COLOR_AMBER_BRIGHT else COLOR_AMBER
        fillPaint.alpha = if (stick != null) min(255, alpha + 60) else (alpha * 0.75f).toInt()
        canvas.drawCircle(nubX, nubY, nubRadius, fillPaint)

        strokePaint.color = COLOR_TEXT
        strokePaint.alpha = alpha
        strokePaint.strokeWidth = 2f
        canvas.drawCircle(nubX, nubY, nubRadius, strokePaint)

        // Label "C"
        textPaint.color = COLOR_TEXT
        textPaint.textSize = nubRadius * 0.9f
        textPaint.alpha = alpha
        canvas.drawText("C", nubX, nubY - (textPaint.ascent() + textPaint.descent()) / 2f, textPaint)
    }

    private fun drawDpad(canvas: Canvas, c: OverlayControl, r: Float, alpha: Int) {
        val x = cx(c)
        val y = cy(c)
        val armWidth = r * 0.38f

        val arms = listOf(
            RectF(x - armWidth, y - r, x + armWidth, y - armWidth * 0.15f) to DPAD_UP,
            RectF(x - armWidth, y + armWidth * 0.15f, x + armWidth, y + r) to DPAD_DOWN,
            RectF(x - r, y - armWidth, x - armWidth * 0.15f, y + armWidth) to DPAD_LEFT,
            RectF(x + armWidth * 0.15f, y - armWidth, x + r, y + armWidth) to DPAD_RIGHT
        )

        for ((rect, bit) in arms) {
            val pressed = (dpadState and bit) != 0
            fillPaint.color = if (pressed) COLOR_AMBER_BRIGHT else COLOR_BG_IDLE
            fillPaint.alpha = if (pressed) min(255, alpha + 70) else (alpha * 0.50f).toInt()
            canvas.drawRoundRect(rect, 10f, 10f, fillPaint)

            strokePaint.color = if (pressed) COLOR_AMBER_BRIGHT else COLOR_AMBER
            strokePaint.alpha = alpha
            strokePaint.strokeWidth = 3f
            canvas.drawRoundRect(rect, 10f, 10f, strokePaint)
        }

        // Center square connecting the arms
        val centerRect = RectF(x - armWidth, y - armWidth, x + armWidth, y + armWidth)
        fillPaint.color = COLOR_BG_IDLE
        fillPaint.alpha = (alpha * 0.50f).toInt()
        canvas.drawRect(centerRect, fillPaint)
    }

    private fun drawButton(canvas: Canvas, c: OverlayControl, r: Float, isPressed: Boolean, alpha: Int) {
        val x = cx(c)
        val y = cy(c)

        fillPaint.color = if (isPressed) COLOR_AMBER_BRIGHT else COLOR_BG_IDLE
        fillPaint.alpha = if (isPressed) min(255, alpha + 70) else (alpha * 0.55f).toInt()

        strokePaint.color = if (isPressed) COLOR_AMBER_BRIGHT else COLOR_AMBER
        strokePaint.alpha = alpha
        strokePaint.strokeWidth = 3f

        val isPill = c.label.length > 2 || c.kind == OverlayControl.Kind.BUTTON && (c.id == "start" || c.id == "select")
        if (isPill) {
            val pillWidth = r * 1.5f
            val pillHeight = r * 0.70f
            val rect = RectF(x - pillWidth, y - pillHeight, x + pillWidth, y + pillHeight)
            canvas.drawRoundRect(rect, pillHeight, pillHeight, fillPaint)
            canvas.drawRoundRect(rect, pillHeight, pillHeight, strokePaint)
        } else {
            canvas.drawCircle(x, y, r, fillPaint)
            canvas.drawCircle(x, y, r, strokePaint)
        }

        // Text label
        if (c.label.isNotEmpty()) {
            textPaint.color = if (isPressed) COLOR_TEXT_PRESSED else COLOR_TEXT
            textPaint.alpha = min(255, alpha + 40)
            textPaint.textSize = if (isPill) r * 0.55f else max(16f, r * 0.85f)
            canvas.drawText(c.label, x, y - (textPaint.ascent() + textPaint.descent()) / 2f, textPaint)
        }
    }

    companion object {
        const val DPAD_UP = 1
        const val DPAD_DOWN = 2
        const val DPAD_LEFT = 4
        const val DPAD_RIGHT = 8

        private const val COLOR_AMBER = 0xFFEBA133.toInt()
        private const val COLOR_AMBER_BRIGHT = 0xFFFFC25C.toInt()
        private const val COLOR_BG_IDLE = 0xFF242428.toInt()
        private const val COLOR_TEXT = 0xFFEDEBE3.toInt()
        private const val COLOR_TEXT_PRESSED = 0xFF141416.toInt()
    }
}
