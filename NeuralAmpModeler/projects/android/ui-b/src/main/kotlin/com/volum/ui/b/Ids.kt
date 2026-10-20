package com.volum.ui.b

import com.volum.core.Lane
import com.volum.core.Param
import com.volum.core.PostBlock
import com.volum.core.PreBlock
import com.volum.core.VoLumState

/** Feature ids (features.md) each control of candidate B carries. */
internal object Ids {
    fun stomp(p: Param): Array<String> = when (p) {
        Param.PITCH_ON -> arrayOf("play.stomp.pitch")
        Param.COMP_ON -> arrayOf("play.stomp.comp")
        Param.NAM1_ON -> arrayOf("play.stomp.nam1")
        Param.NAM2_ON -> arrayOf("play.stomp.nam2")
        Param.CHORUS_ON -> arrayOf("play.stomp.chorus")
        Param.DELAY_ON -> arrayOf("play.stomp.delay")
        Param.REVERB_ON -> arrayOf("play.stomp.reverb")
        Param.TREM_ON -> arrayOf("play.stomp.trem")
        else -> emptyArray()
    }

    fun footswitch(p: Param): Array<String> = when (p) {
        Param.PITCH_ON -> arrayOf("pre.pitch.on")
        Param.COMP_ON -> arrayOf("pre.comp.on")
        Param.NAM1_ON -> arrayOf("pre.nam1.on")
        Param.NAM2_ON -> arrayOf("pre.nam2.on")
        Param.CHORUS_ON -> arrayOf("post.chorus.on")
        Param.DELAY_ON -> arrayOf("post.delay.on")
        Param.REVERB_ON -> arrayOf("post.reverb.on")
        Param.TREM_ON -> arrayOf("post.trem.on")
        else -> emptyArray()
    }

    fun preCard(b: PreBlock): Array<String> = arrayOf("pre.card.${b.name.lowercase()}", "nav.section.pre")
    fun postCard(b: PostBlock): Array<String> = arrayOf("post.card.${b.name.lowercase()}", "nav.section.post")

    fun ampKnob(p: Param, lane: Lane): Array<String> = when (p) {
        Param.MAIN_PAN -> arrayOf("dual.pan.main")
        Param.SUPPORT_PAN -> arrayOf("dual.pan.support")
        Param.BASS -> arrayOf("amp.knob.bass", "amp.knob.reset", "amp.knob.exact")
        else -> if (lane == Lane.SUPPORT) arrayOf("dual.support.knob.${p.label.lowercase()}")
        else arrayOf("amp.knob.${p.label.lowercase()}")
    }

    fun preKnob(p: Param, s: VoLumState): Array<String> = when (p) {
        Param.PITCH_SEMI -> arrayOf("pre.pitch.semi")
        Param.PITCH_MIX -> arrayOf("pre.pitch.mix")
        Param.PITCH_LEVEL -> arrayOf(if (s.param(Param.PITCH_MODE) == 0.0) "pre.pitch.level" else "pre.pitch.oct_level")
        Param.PITCH_OCT_DOWN -> arrayOf("pre.pitch.octdown")
        Param.PITCH_OCT_UP -> arrayOf("pre.pitch.octup")
        Param.PITCH_DRY -> arrayOf("pre.pitch.dry")
        Param.COMP_AMOUNT -> arrayOf("pre.comp.input")
        Param.COMP_ATTACK -> arrayOf("pre.comp.attack")
        Param.COMP_RELEASE -> arrayOf("pre.comp.release")
        Param.COMP_LEVEL -> arrayOf("pre.comp.output")
        Param.NAM1_GAIN, Param.NAM1_BASS, Param.NAM1_MID, Param.NAM1_TREBLE, Param.NAM1_LEVEL,
        Param.NAM2_GAIN, Param.NAM2_BASS, Param.NAM2_MID, Param.NAM2_TREBLE, Param.NAM2_LEVEL ->
            arrayOf("pre.${p.name.substringBefore('_').lowercase()}.${p.name.substringAfter('_').lowercase()}")
        Param.NAM1_MID_HZ -> arrayOf("pre.nam1.midhz")
        Param.NAM2_MID_HZ -> arrayOf("pre.nam2.midhz")
        else -> emptyArray()
    }

    fun postKnob(p: Param, s: VoLumState): Array<String> = when (p) {
        Param.DELAY_AGE -> arrayOf("post.delay.${listOf("grit", "wear", "bloom")[s.param(Param.DELAY_MODE).toInt().coerceIn(0, 2)]}")
        Param.DELAY_FEEDBACK -> arrayOf("post.delay.feedback")
        Param.REVERB_PRE_DELAY -> arrayOf("post.reverb.predelay")
        Param.REVERB_SHIMMER -> arrayOf("post.reverb.intensity")
        Param.TREM_XOVER -> arrayOf("post.trem.xover")
        else -> arrayOf("post.${p.name.substringBefore('_').lowercase()}.${p.label.lowercase()}")
    }
}
