plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
}

val ndkVersionFile = file("${System.getProperty("user.home")}/volum-android/ndk-version.txt")
require(ndkVersionFile.isFile) {
    "The box NDK pin is missing: $ndkVersionFile"
}

android {
    namespace = "com.volum.engine"
    compileSdk = 35
    ndkVersion = ndkVersionFile.readText().trim()

    defaultConfig {
        minSdk = 29
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-O3")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("androidTest").assets.srcDirs(
            "$rootDir/../../tests/golden_rigs",
            "$rootDir/../../../rigs/Ampete One",
        )
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.androidx.test.ext.junit)
}
