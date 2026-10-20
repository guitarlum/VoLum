package com.volum.design.art

import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/** The effects with a motif (`EVoLumEffectFocus`). */
enum class Motif { PITCH, COMP, NAM1, NAM2, CHORUS, DELAY, REVERB, TREM }

/**
 * `VoLumTriptychMotifs.h`: one motif per effect, shared by the pedal cards, the
 * quiet PRE/POST slots and the PLAY stomp wells. Gold and bloom only at card
 * size (`min(w, h) > 40`); thumbnails stay teal. [variant] 1 is the PITCH
 * Octaver chevrons, 0 the Transpose helix.
 */
object Motifs {
    private val kTeal = ArtInk.Teal
    private val kDim = ArtInk.Dim
    private val kMid = ArtInk.Mid
    private val kBlue = ArtInk.Blue
    private val kGold = ArtInk.Gold
    private val kGoldHi = ArtInk.GoldHi

    fun draw(g: ArtCanvas, r: Box, effect: Motif, dimmed: Boolean, variant: Int = 0) {
        val big = r.minSide > 40f
        when (effect) {
            Motif.PITCH -> if (variant == 1) octaver(g, r, dimmed, big) else helix(g, r, dimmed, big)
            Motif.COMP -> comp(g, r, dimmed, big)
            Motif.NAM1 -> fusionFigure(g, r, +1, dimmed)
            Motif.NAM2 -> fusionFigure(g, r, -1, dimmed)
            Motif.DELAY -> delay(g, r, dimmed, big)
            Motif.CHORUS -> chorusThroat(g, r, dimmed)
            Motif.TREM -> tremolo(g, r, dimmed, big)
            Motif.REVERB -> reverb(g, r, dimmed, big)
        }
    }

    private fun octaver(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val cx = r.mw
        val cyy = r.mh
        val ww = r.w * 0.26f
        val hh = r.h * 0.13f
        val gap = r.h * 0.13f
        if (big) ArtCommon.bloom(g, cx, cyy, r.w * 0.45f, kTeal, 0.08f * am)
        fun chev(dir: Int, c: Int) {
            for (k in 0 until 2) {
                val off = gap + k * hh * 0.95f
                val base = cyy + dir * off
                val tip = cyy + dir * (off + hh)
                val cc = withA(c, (0.92f - 0.32f * k) * am)
                if (big) {
                    ArtCommon.glowLine(g, c, cc, cx - ww, base, cx, tip, 2.6f, 4f)
                    ArtCommon.glowLine(g, c, cc, cx + ww, base, cx, tip, 2.6f, 4f)
                } else {
                    g.line(cc, cx - ww, base, cx, tip, 1.8f)
                    g.line(cc, cx + ww, base, cx, tip, 1.8f)
                }
            }
        }
        chev(-1, kTeal)
        chev(1, kBlue)
        if (big) ArtCommon.glowDot(g, kGold, kGoldHi, cx, cyy, 4f, 6f) else g.fillCircle(withA(kTeal, am), cx, cyy, 2.6f)
    }

    private fun helix(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val cx = r.mw
        if (big) ArtCommon.bloom(g, cx, r.mh, r.w * 0.5f, kTeal, 0.10f * am)
        val segs = 96
        val amp = r.w * 0.27f
        val y0 = r.t + r.h * 0.1f
        val y1 = r.b - r.h * 0.1f
        val ax = FloatArray(segs + 1); val ay = FloatArray(segs + 1); val az = FloatArray(segs + 1)
        val bx = FloatArray(segs + 1); val bz = FloatArray(segs + 1)
        for (s in 0..segs) {
            val t = s.toFloat() / segs
            val y = y0 + (y1 - y0) * t
            val ph = t * 2.4f * 6.28318f
            ax[s] = cx + sin(ph) * amp; ay[s] = y; az[s] = cos(ph)
            bx[s] = cx + sin(ph + 3.14159f) * amp; bz[s] = cos(ph + 3.14159f)
        }
        var s = 0
        while (s <= segs) {
            g.line(withA(if (big) kGold else kBlue, 0.32f * am), ax[s], ay[s], bx[s], ay[s], 1f)
            s += 6
        }
        fun strand(px: FloatArray, pz: FloatArray, c0: Int, c1: Int) {
            for (i in 1..segs) {
                val z = pz[i] * 0.5f + 0.5f
                val lw = (if (big) 2.5f else 1.6f) * (0.55f + 0.45f * z)
                g.line(withA(mix(c0, c1, i.toFloat() / (segs + 1)), (0.5f + 0.45f * z) * am), px[i - 1], ay[i - 1], px[i], ay[i], lw)
            }
        }
        strand(ax, az, kTeal, kBlue)
        strand(bx, bz, kBlue, kTeal)
        if (big) for (k in 0..segs) if (abs(ax[k] - bx[k]) < amp * 0.12f) ArtCommon.glowDot(g, kGold, kGoldHi, cx, ay[k], 2.4f, 6f)
    }

    private fun comp(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val arcCol = argb((120f * am).toInt(), 100, 180, 200)
        val tickTeal = argb((150f * am).toInt(), 100, 180, 200)
        val tickGold = argb((200f * am).toInt(), 200, 165, 87)
        val needleGlow = argb((120f * am).toInt(), 200, 165, 87)
        val needleCol = if (big) argb((235f * am).toInt(), 252, 222, 145) else argb((235f * am).toInt(), 120, 210, 220)
        val cx = r.mw
        val margin = 3f
        val tickOut = if (big) 8f else 3f
        val wLimit = (r.w * 0.5f - margin) / 0.9f - tickOut
        val hLimit = (r.h * 0.5f - margin - tickOut) / 0.5f
        val rr = max(6f, min(wLimit, hLimit))
        val cym = r.mh + rr * 0.5f
        val a0 = 3.14159f * 1.15f
        val a1 = 3.14159f * 1.85f
        var pax = cx + rr * cos(a0)
        var pay = cym + rr * sin(a0)
        for (s in 1..40) {
            val a = a0 + (a1 - a0) * s / 40f
            val x = cx + rr * cos(a)
            val y = cym + rr * sin(a)
            g.line(arcCol, pax, pay, x, y, 2f)
            pax = x
            pay = y
        }
        for (i in 0..10) {
            val a = a0 + (a1 - a0) * (i / 10f)
            val r1 = rr - 6f
            val r2 = rr + if (i % 5 == 0) tickOut else tickOut * 0.5f
            val tc = if (i > 7 && big) tickGold else tickTeal
            g.line(tc, cx + r1 * cos(a), cym + r1 * sin(a), cx + r2 * cos(a), cym + r2 * sin(a), if (i % 5 == 0) 1.6f else 1f)
        }
        val na = a0 + (a1 - a0) * 0.68f
        if (big) g.line(needleGlow, cx, cym, cx + rr * 0.92f * cos(na), cym + rr * 0.92f * sin(na), 3.2f)
        g.line(needleCol, cx, cym, cx + rr * 0.92f * cos(na), cym + rr * 0.92f * sin(na), 1.8f)
        g.fillCircle(needleCol, cx, cym, 3.5f)
    }

    private fun delay(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val cy = r.mh
        if (big) ArtCommon.bloom(g, r.l + r.w * 0.14f, cy, r.w * 0.62f, kTeal, 0.09f * am)
        g.line(withA(kDim, 0.3f * am), r.l, cy, r.r, cy, 0.5f)
        val taps = 5
        val tapW = r.w / taps
        for (t in 0 until taps) {
            val decay = 1f - t.toFloat() / taps * 0.72f
            val ampY = r.h * 0.36f * decay
            val baseX = r.l + t * tapW
            val first = t == 0
            val core = if (first && big) kGoldHi else if (t % 2 != 0) kMid else kTeal
            var pX = baseX
            var pY = cy
            for (j in 1..40) {
                val t1 = j / 40f
                val x = baseX + t1 * tapW
                val env = sin(t1 * 3.14159f)
                val y = cy + sin(t1 * 6.28318f * 3f) * ampY * env
                val c = withA(core, (0.5f + 0.4f * decay) * am)
                val lw = (if (big) 1.9f else 1.4f) * decay
                if (first && big) ArtCommon.glowLine(g, kGold, c, pX, pY, x, y, lw, 3f * decay) else g.line(c, pX, pY, x, y, lw)
                pX = x
                pY = y
            }
            if (t > 0) g.line(withA(kDim, 0.3f * decay * am), baseX, r.t + r.h * 0.16f, baseX, r.b - r.h * 0.16f, 0.6f)
        }
    }

    private class Mouth(val cx: Float, val cy: Float, val rx: Float, val ry: Float)

    private fun chorusThroat(g: ArtCanvas, r: Box, dimmed: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val w = r.w
        val h = r.h
        val big = min(w, h) > 40f
        val front = Mouth(r.l + w * 0.50f, r.t + h * (if (big) 0.715f else 0.68f), w * (if (big) 0.44f else 0.38f), h * (if (big) 0.155f else 0.14f))
        val back = Mouth(r.l + w * 0.50f, r.t + h * (if (big) 0.185f else 0.32f), w * (if (big) 0.135f else 0.14f), h * (if (big) 0.055f else 0.06f))
        fun lerp(a: Mouth, b: Mouth, t: Float) = Mouth(a.cx + (b.cx - a.cx) * t, a.cy + (b.cy - a.cy) * t, a.rx + (b.rx - a.rx) * t, a.ry + (b.ry - a.ry) * t)
        fun strokeMouth(e: Mouth, segs: Int, col: Int, lw: Float) {
            var px = e.cx + e.rx
            var py = e.cy
            for (i in 1..segs) {
                val a = i.toFloat() / segs * 6.28318f
                val x = e.cx + e.rx * cos(a)
                val y = e.cy + e.ry * sin(a)
                g.line(col, px, py, x, y, lw)
                px = x
                py = y
            }
        }
        if (big) ArtCommon.bloom(g, back.cx, back.cy, min(w, h) * 0.42f, kGold, 0.14f * am)
        val nLat = if (big) 5 else 3
        val nGen = if (big) 14 else 6
        val twist = 0.55f
        val segs = if (big) 48 else 16
        for (i in 0 until nLat) {
            val t = i.toFloat() / (nLat - 1)
            val e = lerp(front, back, t)
            val col = if (t > 0.72f) (if (big) kGold else kTeal) else mix(kTeal, kBlue, t)
            strokeMouth(e, segs, withA(col, (0.35f + 0.55f * t) * am), if (big) (if (t > 0.8f) 2.2f else 1.35f) else 1.15f)
        }
        for (i in 0 until nGen) {
            val a0 = i.toFloat() / nGen * 6.28318f
            val a1 = a0 + twist
            val col = withA(mix(kDim, kTeal, if (i % 2 != 0) 0.4f else 0.85f), 0.55f * am)
            g.line(col, front.cx + front.rx * cos(a0), front.cy + front.ry * sin(a0), back.cx + back.rx * cos(a1), back.cy + back.ry * sin(a1), if (big) 1.15f else 0.95f)
        }
        strokeMouth(front, segs, withA(kTeal, 0.95f * am), if (big) 2.3f else 1.4f)
        if (big) {
            strokeMouth(back, segs, withA(kGoldHi, 0.95f * am), 1.8f)
            g.fillCircle(withA(kGoldHi, 0.95f * am), back.cx, back.cy, 2.8f)
        } else {
            g.fillCircle(withA(kTeal, 0.95f * am), back.cx, back.cy, 1.5f)
        }
    }

    private fun tremolo(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val cy = r.mh
        val barTeal = argb((215f * am).toInt(), 120, 210, 220)
        val barMid = argb((215f * am).toInt(), 100, 180, 200)
        val n = 17
        val span = r.w * 0.7f
        val x0 = r.l + r.w * 0.15f
        val slot = span / n
        val bw = slot * 0.62f
        for (i in 0 until n) {
            val t = i.toFloat() / (n - 1)
            val lfo = 0.2f + 0.8f * (0.5f + 0.5f * sin(t * 6.28318f * 2f - 1.5708f))
            val bh = r.h * 0.4f * lfo
            val bx = x0 + i * slot + (slot - bw) * 0.5f
            g.fillRect(if (i % 2 != 0) barTeal else barMid, bx, cy - bh, bx + bw, cy + bh)
        }
        var pX = x0
        var pY = cy - r.h * 0.4f * 0.2f
        for (s in 1..80) {
            val t = s / 80f
            val x = x0 + t * span
            val lfo = 0.2f + 0.8f * (0.5f + 0.5f * sin(t * 6.28318f * 2f - 1.5708f))
            val y = cy - r.h * 0.4f * lfo
            if (big) g.line(argb((70f * am).toInt(), 200, 165, 87), pX, pY, x, y, 3.2f)
            g.line(if (big) argb((210f * am).toInt(), 252, 222, 145) else argb((210f * am).toInt(), 120, 210, 220), pX, pY, x, y, 1.4f)
            pX = x
            pY = y
        }
    }

    private fun reverb(g: ArtCanvas, r: Box, dimmed: Boolean, big: Boolean) {
        val am = if (dimmed) 0.4f else 1f
        val cy = r.mh
        val bright = argb(if (dimmed) 70 else 150, 120, 210, 220)
        val mid = argb(if (dimmed) 40 else 80, 100, 180, 200)
        val dim = argb(if (dimmed) 20 else 45, 80, 150, 170)
        if (big) ArtCommon.bloom(g, r.mw, r.mh + r.h * 0.1f, r.minSide * 0.6f, kTeal, 0.10f * am)
        val scale = r.minSide
        val sizeFactor = (scale / 150f).coerceIn(0.18f, 1f)
        val count = max(450, (12000f * sizeFactor * sizeFactor).toInt())
        val lenScale = max(0.55f, sizeFactor)
        val brightCount = (count * 0.025f).toInt()
        val midCount = (count * 0.125f).toInt()
        val xs = FloatArray(count + 32)
        val ys = FloatArray(count + 32)
        var n = 0
        fun seedRow(from: Float, to: Float, step: Float, y: Float) {
            var f = from
            while (f <= to + 1e-6f) {
                xs[n] = r.l + r.w * f; ys[n] = y; n++
                f += step
            }
        }
        seedRow(0.05f, 0.95f, 0.15f, r.b)
        seedRow(0.1f, 0.9f, 0.2f, cy + r.h * 0.15f)
        seedRow(0.15f, 0.85f, 0.25f, cy - r.h * 0.15f)
        seedRow(0.2f, 0.8f, 0.3f, r.t + r.h * 0.1f)
        var rng = 54321
        for (i in 0 until count) {
            rng = rng * 1103515245 + 12345
            val parent = Integer.remainderUnsigned(rng, n)
            val angle = Integer.remainderUnsigned(rng, 360).toFloat()
            val rad = angle * 3.14159f / 180f
            val len = (2f + Integer.remainderUnsigned(rng, 4)) * lenScale
            val nx = xs[parent] + len * cos(rad)
            val ny = ys[parent] + len * sin(rad)
            if (nx >= r.l && nx <= r.r && ny >= r.t && ny <= r.b) {
                val col = if (i < brightCount) bright else if (i < midCount) mid else dim
                val tk = if (i < (brightCount * 0.7f).toInt()) 2f else if (i < midCount) 1.5f else 1f
                g.line(col, xs[parent], ys[parent], nx, ny, tk)
                xs[n] = nx; ys[n] = ny; n++
            }
        }
    }

    // ---- NAM: the Fusion Dance "HA!" figure (`DrawFusionFigure`) ---------------

    private const val ASPECT = 1.34f
    private const val HEAD_R = 0.112f
    private val POSE = floatArrayOf(
        0.719f, 0.322f, 0.599f, 0.392f, 0.523f, 0.483f, 0.464f, 0.594f, // head neck waist pelvis
        0.542f, 0.350f, 0.711f, 0.024f, 0.906f, 0.087f, 1.000f, 0.108f, // shoulderOut elbowTop wristTop tipTop
        0.646f, 0.448f, 0.807f, 0.636f, 0.911f, 0.566f, 1.000f, 0.535f, // shoulderIn elbowLow wristLow tipLow
        0.432f, 0.608f, 0.255f, 0.727f, 0.125f, 0.916f, 0.146f, 0.972f, 0.005f, 0.979f, // hipOut kneeOut ankleOut heelOut toeOut
        0.505f, 0.622f, 0.635f, 0.720f, 0.563f, 0.916f, 0.521f, 0.972f, 0.672f, 0.983f, // hipIn kneeIn ankleIn heelIn toeIn
    )
    private const val HEAD = 0; private const val NECK = 1; private const val WAIST = 2; private const val PELVIS = 3
    private const val SH_OUT = 4; private const val ELBOW_TOP = 5; private const val WRIST_TOP = 6; private const val TIP_TOP = 7
    private const val SH_IN = 8; private const val ELBOW_LOW = 9; private const val WRIST_LOW = 10; private const val TIP_LOW = 11
    private const val HIP_OUT = 12; private const val TOE_OUT = 16; private const val HIP_IN = 17

    private class Pt(val x: Float, val y: Float)

    fun fusionFigure(g: ArtCanvas, r: Box, dir: Int, dimmed: Boolean) {
        val am = if (dimmed) 0.28f else 1f
        val big = r.minSide > 40f
        val bh = min(if (big) r.h - 12f else r.h, r.w * (if (big) 0.96f else 1f) / ASPECT)
        val bw = bh * ASPECT
        val seam = if (dir > 0) r.r else r.l
        val top = r.mh - bh * 0.5f
        val fd = dir.toFloat()
        fun j(k: Int) = Pt(seam - fd * (1f - POSE[2 * k]) * bw, top + POSE[2 * k + 1] * bh)
        val minRad = if (big) 0.9f else 0.8f
        fun rr(f: Float) = max(minRad, f * bh)
        fun along(a: Pt, b: Pt, t: Float) = Pt(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t)
        fun through(a: Pt, m: Pt, b: Pt) = Pt(2f * m.x - 0.5f * (a.x + b.x), 2f * m.y - 0.5f * (a.y + b.y))
        fun segment(a: Pt, ra: Float, b: Pt, rb: Float) {
            val dx = b.x - a.x
            val dy = b.y - a.y
            val len = sqrt(dx * dx + dy * dy)
            if (len < 1e-3f) return
            val nx = -dy / len
            val ny = dx / len
            g.moveTo(a.x + nx * ra, a.y + ny * ra)
            g.lineTo(b.x + nx * rb, b.y + ny * rb)
            g.lineTo(b.x - nx * rb, b.y - ny * rb)
            g.lineTo(a.x - nx * ra, a.y - ny * ra)
            g.closePath()
        }
        fun joint(p: Pt, rad: Float) = g.pathCircle(p.x, p.y, rad)
        fun limb(a: Pt, c: Pt, b: Pt, ra: Float, rb: Float, steps: Int) {
            var prev = a
            var prevR = ra
            joint(a, ra)
            for (i in 1..steps) {
                val t = i.toFloat() / steps
                val s = 1f - t
                val p = Pt(s * s * a.x + 2f * s * t * c.x + t * t * b.x, s * s * a.y + 2f * s * t * c.y + t * t * b.y)
                val rad = ra + (rb - ra) * t
                segment(prev, prevR, p, rad)
                joint(p, rad)
                prev = p
                prevR = rad
            }
        }
        fun straight(a: Pt, b: Pt, ra: Float, rb: Float) = limb(a, along(a, b, 0.5f), b, ra, rb, 1)

        val head = j(HEAD); val neck = j(NECK); val waist = j(WAIST); val pelvis = j(PELVIS)
        val shOut = j(SH_OUT); val elbowTop = j(ELBOW_TOP); val wristTop = j(WRIST_TOP); val touchTop = j(TIP_TOP)
        val shIn = j(SH_IN); val elbowLow = j(ELBOW_LOW); val wristLow = j(WRIST_LOW); val touchLow = j(TIP_LOW)
        val headR = max(if (big) 3f else 1.8f, HEAD_R * bh)
        val armR = if (big) rr(0.03f) else 0.62f
        val foreR = if (big) rr(0.024f) else 0.58f
        val wristR = if (big) rr(0.019f) else 0.55f
        val handR = if (big) rr(0.03f) else 0.6f
        val tipR = max(0.35f, 0.004f * bh)

        fun body(grow: Float) {
            g.clearPath()
            limb(pelvis, through(pelvis, waist, neck), neck, rr(0.062f) + grow, rr(0.07f) + grow, if (big) 6 else 2)
            straight(shOut, shIn, armR + grow, armR + grow)
            straight(neck, head, rr(0.034f) + grow, rr(0.03f) + grow)
            joint(head, headR + grow)
            limb(shOut, through(shOut, elbowTop, wristTop), wristTop, armR + grow, wristR + grow, if (big) 10 else 5)
            joint(wristTop, handR + grow)
            segment(wristTop, wristR + grow, touchTop, tipR + grow)
            straight(shIn, elbowLow, armR + grow, foreR + grow)
            straight(elbowLow, wristLow, foreR + grow, wristR + grow)
            joint(wristLow, handR + grow)
            segment(wristLow, wristR + grow, touchLow, tipR + grow)
            for (base in intArrayOf(HIP_OUT, HIP_IN)) {
                val leg = Array(5) { j(base + it) }
                straight(leg[0], leg[1], rr(0.046f) + grow, rr(0.033f) + grow)
                straight(leg[1], leg[2], rr(0.033f) + grow, rr(0.022f) + grow)
                straight(leg[2], leg[3], rr(0.022f) + grow, rr(0.02f) + grow)
                straight(leg[3], leg[4], rr(0.02f) + grow, rr(0.012f) + grow)
            }
        }

        if (big) {
            val mid = along(pelvis, neck, 0.5f)
            ArtCommon.bloom(g, mid.x, mid.y, bh * 0.6f, kTeal, 0.07f * am)
            for (k in 0 until 3) {
                val rad = bh * (0.42f + 0.07f * k)
                val a0 = 3.5f
                val a1 = 4.45f - 0.08f * k
                val segs = 18
                for (i in 0 until segs) {
                    val t0 = i.toFloat() / segs
                    val t1 = (i + 1).toFloat() / segs
                    val p0 = a0 + (a1 - a0) * t0
                    val p1 = a0 + (a1 - a0) * t1
                    g.line(withA(kTeal, (0.05f + 0.3f * t1 * t1) * (1f - 0.28f * k) * am),
                        shOut.x + fd * cos(p0) * rad, shOut.y + sin(p0) * rad, shOut.x + fd * cos(p1) * rad, shOut.y + sin(p1) * rad, 1.3f)
                }
            }
            body(5f)
            g.fillPath(withA(kTeal, 0.05f * am))
            body(2.2f)
            g.fillPath(withA(kTeal, 0.1f * am))
        }
        val toe = j(TOE_OUT)
        body(0f)
        // NanoVG keeps the first and last of the three stops.
        g.fillPath(ArtCanvas.linear(toe.x, toe.y, touchTop.x, touchTop.y, withA(mix(kDim, kBlue, 0.4f), 0.45f * am), withA(mix(kTeal, kBlue, 0.35f), 0.98f * am)))

        if (big) {
            val span = r.minSide
            ArtCommon.bloom(g, touchTop.x, touchTop.y, span * 0.5f, kGold, 0.2f * am)
            ArtCommon.bloom(g, touchLow.x, touchLow.y, span * 0.3f, kGold, 0.14f * am)
            fun flare(c: Pt, len: Float, wid: Float, a: Float) {
                g.moveTo(c.x - len, c.y); g.lineTo(c.x, c.y - wid); g.lineTo(c.x + len, c.y); g.lineTo(c.x, c.y + wid); g.closePath()
                g.moveTo(c.x, c.y - len); g.lineTo(c.x + wid, c.y); g.lineTo(c.x, c.y + len); g.lineTo(c.x - wid, c.y); g.closePath()
                g.fillPath(withA(kGoldHi, a))
            }
            g.fillCircle(withA(kGold, 0.26f * am), touchTop.x, touchTop.y, 11f)
            g.fillCircle(withA(kGold, 0.45f * am), touchTop.x, touchTop.y, 6.5f)
            flare(touchTop, bh * 0.3f, 2.2f, 0.95f * am)
            g.fillCircle(withA(kGoldHi, am), touchTop.x, touchTop.y, 3.2f)
            g.fillCircle(withA(kGold, 0.35f * am), touchLow.x, touchLow.y, 7f)
            flare(touchLow, bh * 0.17f, 1.6f, 0.85f * am)
            g.fillCircle(withA(kGoldHi, am), touchLow.x, touchLow.y, 2.4f)
        } else {
            g.fillCircle(withA(kTeal, 0.9f * am), touchTop.x, touchTop.y, 1.3f)
            g.fillCircle(withA(kTeal, 0.9f * am), touchLow.x, touchLow.y, 1.1f)
        }
    }
}
