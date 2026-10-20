package com.volum.design.art

import android.graphics.Bitmap
import android.graphics.Canvas
import kotlin.math.ceil

/**
 * `ILayerPtr` / `art::ArtLayer`: one rect of art rendered once into a bitmap at
 * device pixels and blitted 1:1 afterwards. Building is the only place an art
 * allocates; the per-frame path only blits.
 */
class ArtLayer {
    var bitmap: Bitmap? = null
        private set
    private var add: Bitmap? = null
    var box: Box = Box(0f, 0f, 0f, 0f)
        private set
    var logicalW = 0f
        private set
    var logicalH = 0f
        private set
    private var density = 0f

    val ok: Boolean get() = bitmap != null

    fun matches(b: Box, d: Float): Boolean = bitmap != null && density == d && box.l == b.l && box.t == b.t && box.w == b.w && box.h == b.h

    fun build(b: Box, d: Float, paint: (ArtCanvas) -> Unit) {
        reset()
        val pw = ceil(b.w * d).toInt().coerceAtLeast(1)
        val ph = ceil(b.h * d).toInt().coerceAtLeast(1)
        val bmp = Bitmap.createBitmap(pw, ph, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        canvas.scale(d, d)
        canvas.translate(-b.l, -b.t)
        canvas.save()
        canvas.clipRect(b.l, b.t, b.l + pw / d, b.t + ph / d)
        paint(ArtCanvas().begin(canvas))
        canvas.restore()
        bitmap = bmp
        box = b
        density = d
        logicalW = pw / d
        logicalH = ph / d
    }

    /**
     * The copy `EBlend::Add` draws: NanoVG weights a layer pixel by its own alpha
     * once more, so a faint pixel (a sky wash) brightens far less than a solid one.
     */
    fun addBitmap(): Bitmap? {
        add?.let { return it }
        val src = bitmap ?: return null
        val px = IntArray(src.width * src.height)
        src.getPixels(px, 0, src.width, 0, 0, src.width, src.height)
        for (i in px.indices) {
            val c = px[i]
            val a = c ushr 24
            if (a == 0) continue
            px[i] = (c and 0xFFFFFF) or (((a * a + 127) / 255) shl 24)
        }
        val out = Bitmap.createBitmap(src.width, src.height, Bitmap.Config.ARGB_8888)
        out.setPixels(px, 0, src.width, 0, 0, src.width, src.height)
        add = out
        return out
    }

    fun reset() {
        bitmap = null
        add = null
        density = 0f
    }
}
