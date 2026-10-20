package com.volum.engine

object NativeEngine {
    init {
        System.loadLibrary("volum_engine_jni")
    }

    external fun render(inputPath: String, rigPath: String, outputPath: String): String
}
