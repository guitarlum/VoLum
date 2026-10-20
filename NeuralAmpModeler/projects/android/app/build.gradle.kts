plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.volum.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.volum.android"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "0.1-shootout"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    flavorDimensions += "candidate"
    productFlavors {
        create("a") {
            dimension = "candidate"
            applicationIdSuffix = ".a"
            resValue("string", "app_name", "VoLum A")
        }
        create("b") {
            dimension = "candidate"
            applicationIdSuffix = ".b"
            resValue("string", "app_name", "VoLum B")
        }
        create("c") {
            dimension = "candidate"
            applicationIdSuffix = ".c"
            resValue("string", "app_name", "VoLum C")
        }
        create("design") {
            dimension = "candidate"
            applicationIdSuffix = ".design"
            resValue("string", "app_name", "VoLum Design")
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
    }

    sourceSets {
        getByName("androidTest") {
            java.srcDir("../harness/src/sharedTest/kotlin")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        compose = true
    }
    packaging {
        resources.excludes += setOf("META-INF/AL2.0", "META-INF/LGPL2.1", "META-INF/LICENSE*")
    }
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    implementation(project(":core"))
    implementation(project(":harness"))
    implementation(project(":design"))
    "aImplementation"(project(":ui-a"))
    "bImplementation"(project(":ui-b"))
    "cImplementation"(project(":ui-c"))

    androidTestImplementation(platform(libs.compose.bom))
    androidTestImplementation(libs.compose.ui.test.junit4)
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(libs.junit)
}
