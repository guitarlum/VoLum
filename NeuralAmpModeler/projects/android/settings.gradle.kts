pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "VoLumAndroid"

include(":core", ":harness", ":design", ":ui-a", ":ui-b", ":ui-c", ":app")

// engine/ is ticket 06's native spike; it joins the build once it has a build file.
if (file("engine/build.gradle.kts").exists()) {
    include(":engine")
}
