package com.lum.volum

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager

/**
 * Enumerates audio endpoints so the user can pick the USB interface explicitly.
 * The primary real-time path is a class-compliant USB audio interface; built-in
 * mic/speaker are only a fallback for bring-up on an emulator/phone.
 */
data class AudioEndpoint(val id: Int, val label: String, val isUsb: Boolean)

object AudioDevices {

    private val usbTypes = setOf(
        AudioDeviceInfo.TYPE_USB_DEVICE,
        AudioDeviceInfo.TYPE_USB_ACCESSORY,
        AudioDeviceInfo.TYPE_USB_HEADSET
    )

    fun inputs(ctx: Context): List<AudioEndpoint> = query(ctx, AudioManager.GET_DEVICES_INPUTS)
    fun outputs(ctx: Context): List<AudioEndpoint> = query(ctx, AudioManager.GET_DEVICES_OUTPUTS)

    private fun query(ctx: Context, flag: Int): List<AudioEndpoint> {
        val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val out = mutableListOf(AudioEndpoint(0, "System default", isUsb = false))
        am.getDevices(flag).forEach { d ->
            val usb = d.type in usbTypes
            val name = runCatching { d.productName?.toString() }.getOrNull().orEmpty()
            val label = "${typeName(d.type)}${if (name.isNotBlank()) " · $name" else ""} (#${d.id})"
            out += AudioEndpoint(d.id, label, usb)
        }
        // USB first so the interface is easy to pick.
        return out.sortedByDescending { it.isUsb }
    }

    private fun typeName(type: Int): String = when (type) {
        AudioDeviceInfo.TYPE_USB_DEVICE -> "USB device"
        AudioDeviceInfo.TYPE_USB_ACCESSORY -> "USB accessory"
        AudioDeviceInfo.TYPE_USB_HEADSET -> "USB headset"
        AudioDeviceInfo.TYPE_BUILTIN_MIC -> "Built-in mic"
        AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "Built-in speaker"
        AudioDeviceInfo.TYPE_WIRED_HEADSET -> "Wired headset"
        AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> "Wired headphones"
        AudioDeviceInfo.TYPE_BLUETOOTH_A2DP -> "Bluetooth"
        else -> "Type $type"
    }
}
