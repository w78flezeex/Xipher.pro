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

enableFeaturePreview("TYPESAFE_PROJECT_ACCESSORS")

rootProject.name = "xipher-android"

include(
    ":app",
    ":core:common",
    ":core:ui",
    ":core:network",
    ":core:storage",
    ":feature:auth",
    ":feature:chats",
    ":feature:chat",
    ":feature:settings",
    ":feature:groups",
    ":feature:channels",
    ":feature:calls",
    ":feature:bots",
    ":feature:admin"
)
