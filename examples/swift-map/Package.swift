// swift-tools-version: 6.0

import PackageDescription

let package = Package(
  name: "swift-map",
  platforms: [.macOS(.v14)],
  products: [
    .executable(name: "swift-map", targets: ["SwiftMap"]),
  ],
  targets: [
    .target(
      name: "CMapLibreNativeABI",
      publicHeadersPath: "include"
    ),
    .executableTarget(
      name: "SwiftMap",
      dependencies: ["CMapLibreNativeABI"],
      resources: [.process("Shaders")],
      linkerSettings: [
        .linkedFramework("AppKit"),
        .linkedFramework("Metal"),
        .linkedFramework("QuartzCore"),
        .unsafeFlags([
          "-L../../build",
          "-lmaplibre_native_abi",
          "-Xlinker", "-rpath",
          "-Xlinker", "../../build",
        ]),
      ]
    ),
  ]
)
