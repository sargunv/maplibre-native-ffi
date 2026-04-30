// swift-tools-version: 6.0

import PackageDescription

let package = Package(
  name: "swift-map",
  platforms: [.macOS(.v14)],
  products: [
    .executable(name: "swift-map", targets: ["SwiftMap"]),
  ],
  targets: [
    .systemLibrary(
      name: "CMapLibreNativeABI",
      pkgConfig: "maplibre-native-ffi"
    ),
    .executableTarget(
      name: "SwiftMap",
      dependencies: ["CMapLibreNativeABI"],
      resources: [.process("Shaders")],
      linkerSettings: [
        .linkedFramework("AppKit"),
        .linkedFramework("Metal"),
        .linkedFramework("QuartzCore"),
      ]
    ),
  ]
)
