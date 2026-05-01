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
      name: "CMapLibreNativeC",
      pkgConfig: "maplibre-native-c"
    ),
    .executableTarget(
      name: "SwiftMap",
      dependencies: ["CMapLibreNativeC"],
      linkerSettings: [
        .linkedFramework("AppKit"),
        .linkedFramework("Metal"),
        .linkedFramework("QuartzCore"),
      ]
    ),
  ]
)
