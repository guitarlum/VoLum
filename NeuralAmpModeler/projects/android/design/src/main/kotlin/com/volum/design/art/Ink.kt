package com.volum.design.art

import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/**
 * Colours as packed ARGB ints, in the desktop's `IColor(A, R, G, B)` argument
 * order, so the art ports keep the C++ literals and never allocate a colour.
 */
fun argb(a: Int, r: Int, g: Int, b: Int): Int =
    (a.coerceIn(0, 255) shl 24) or (r.coerceIn(0, 255) shl 16) or (g.coerceIn(0, 255) shl 8) or b.coerceIn(0, 255)

fun alphaOf(c: Int): Int = c ushr 24
fun redOf(c: Int): Int = (c shr 16) and 0xFF
fun greenOf(c: Int): Int = (c shr 8) and 0xFF
fun blueOf(c: Int): Int = c and 0xFF

/** `volumart::WithA`: alpha replaced by `(int)(a * 255)`, clamped. */
fun withA(c: Int, a: Float): Int = (c and 0xFFFFFF) or ((a * 255f).toInt().coerceIn(0, 255) shl 24)

/** `IColor::WithOpacity`. */
fun opacity(c: Int, a: Float): Int = withA(c, a)

/** `volumart::Mix`: per-channel lerp with the desktop's int truncation. */
fun mix(a: Int, b: Int, t: Float): Int {
    val k = t.coerceIn(0f, 1f)
    fun ch(x: Int, y: Int) = (x + (y - x) * k).toInt()
    return argb(ch(alphaOf(a), alphaOf(b)), ch(redOf(a), redOf(b)), ch(greenOf(a), greenOf(b)), ch(blueOf(a), blueOf(b)))
}

/** `IColor::WithContrast`: adds `c * 255` to every channel. */
fun contrast(col: Int, c: Float): Int {
    val m = (c * 255f).toInt()
    return argb(alphaOf(col), redOf(col) + m, greenOf(col) + m, blueOf(col) + m)
}

/** The shared art inks of `art/VoLumArtCommon.h`. */
object ArtInk {
    val Teal = argb(255, 120, 210, 220)
    val Mid = argb(255, 100, 180, 200)
    val Dim = argb(255, 80, 150, 170)
    val Blue = argb(255, 150, 205, 245)
    val Gold = argb(255, 200, 165, 87)
    val GoldHi = argb(255, 252, 222, 145)
    val HeroBright = argb(150, 120, 210, 220)
    val HeroMid = argb(80, 100, 180, 200)
    val HeroDim = argb(45, 80, 150, 170)
    const val HeroTk = 2f
    const val HeroTkThin = 1.5f

    val CustomArtBright = argb(220, 120, 210, 220)
    val CustomArtDim = argb(205, 100, 178, 208)
    val HeroBg = argb(255, 12, 12, 18)
    val VoTeal = argb(255, 91, 196, 196)
}

/** `volumart::Frand`: the LCG every art seeds its scatter with. State lives in [s]. */
class Frand(seed: Int) {
    var s: Int = seed
    fun next(): Float {
        s = s * 1664525 + 1013904223
        return (s.toLong() and 0xFFFFFFFFL).toFloat() / 4294967295f
    }
}

/** `volumart::Hash01` (uint32 arithmetic on Int bits). */
fun hash01(v: Int): Float {
    var x = v
    x = x xor (x ushr 16)
    x *= 0x7feb352d
    x = x xor (x ushr 15)
    x *= 0x846ca68b.toInt()
    x = x xor (x ushr 16)
    return (x ushr 8).toFloat() * (1f / 16777216f)
}

/** `volumart::Noise1`: smooth value noise in [-0.5, 0.5], one curve per seed. */
fun noise1(x: Float, seed: Int): Float {
    val fl = floor(x)
    val f = x - fl
    val i = fl.toInt()
    val salt = seed * 97 + 13
    val a = hash01(i * 2654435761L.toInt() xor salt)
    val b = hash01((i + 1) * 2654435761L.toInt() xor salt)
    val s = f * f * (3f - 2f * f)
    return a + (b - a) * s - 0.5f
}

fun smoothstep(e0: Float, e1: Float, x: Float): Float {
    if (e1 == e0) return if (x < e0) 0f else 1f
    val t = ((x - e0) / (e1 - e0)).coerceIn(0f, 1f)
    return t * t * (3f - 2f * t)
}

/** A desktop `IRECT` (left, top, right, bottom). Built only at prepare / layer time. */
class Box(val l: Float, val t: Float, val r: Float, val b: Float) {
    val w get() = r - l
    val h get() = b - t
    val mw get() = (l + r) * 0.5f
    val mh get() = (t + b) * 0.5f
    fun padded(p: Float) = Box(l - p, t - p, r + p, b + p)
    /** `IRECT::GetPadded(l, t, r, b)`: negative values shrink. */
    fun padded(pl: Float, pt: Float, pr: Float, pb: Float) = Box(l + pl, t + pt, r + pr, b + pb)
    fun contains(x: Float, y: Float) = x >= l && x < r && y >= t && y < b
    val minSide get() = min(w, h)
    val maxSide get() = max(w, h)
}
