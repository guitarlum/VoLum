plugins {
    alias(libs.plugins.kotlin.jvm)
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    api(libs.kotlinx.coroutines.core)
    testImplementation(libs.junit)
}

val featuresMd = rootProject.file("../../../.scratch/android-shootout/features.md")
val userGuide = rootProject.file("../../../docs/user-guide.en.md")

tasks.test {
    inputs.file(featuresMd)
    inputs.file(userGuide)
    systemProperty("volum.featuresMd", featuresMd.absolutePath)
    systemProperty("volum.userGuide", userGuide.absolutePath)
}

val dumpRegistry by tasks.registering(JavaExec::class) {
    description = "Writes the screen, capture and motion registries as TSV for the box scripts."
    classpath = sourceSets["main"].runtimeClasspath
    mainClass.set("com.volum.core.RegistryDumpKt")
    val out = layout.buildDirectory.dir("registry")
    outputs.dir(out)
    args(out.get().asFile.absolutePath)
}
