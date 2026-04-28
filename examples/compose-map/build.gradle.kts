import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
  kotlin("jvm") version "2.3.21"
  id("org.jetbrains.kotlin.plugin.compose") version "2.3.21"
  id("org.jetbrains.compose") version "1.10.3"
}

kotlin {
  jvmToolchain(21)
}

dependencies {
  implementation(compose.desktop.currentOs)
}

val nativeOutputDir = layout.buildDirectory.dir("native")
val nativeLibrary = nativeOutputDir.map { it.file("libcompose_map_metal.dylib") }

tasks.register<Exec>("compileNativeMetal") {
  onlyIf { org.gradle.internal.os.OperatingSystem.current().isMacOsX }
  val javaHome = providers.systemProperty("java.home")
  inputs.file("src/main/native/NativeMetalBridge.mm")
  outputs.file(nativeLibrary)
  doFirst { nativeOutputDir.get().asFile.mkdirs() }
  commandLine(
    "xcrun",
    "--sdk", "macosx",
    "clang++",
    "-std=c++17",
    "-dynamiclib",
    "-fobjc-arc",
    "-I${javaHome.get()}/include",
    "-I${javaHome.get()}/include/darwin",
    "-framework", "Foundation",
    "-framework", "Metal",
    "-framework", "QuartzCore",
    "src/main/native/NativeMetalBridge.mm",
    "-o", nativeLibrary.get().asFile.absolutePath,
  )
}

compose.desktop {
  application {
    mainClass = "MainKt"
    nativeDistributions {
      targetFormats(TargetFormat.Dmg)
      packageName = "compose-map-poc"
      packageVersion = "1.0.0"
    }
  }
}

tasks.withType<JavaExec>().configureEach {
  dependsOn("compileNativeMetal")
  systemProperty("skiko.renderApi", System.getProperty("skiko.renderApi") ?: "METAL")
  systemProperty("java.library.path", nativeOutputDir.get().asFile.absolutePath)
}

tasks.named("build") {
  dependsOn("compileNativeMetal")
}
