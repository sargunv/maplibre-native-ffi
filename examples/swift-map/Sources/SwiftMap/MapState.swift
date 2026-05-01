import CMapLibreNativeC
import Foundation
import QuartzCore

struct Viewport: Equatable {
  var logicalWidth: UInt32
  var logicalHeight: UInt32
  var physicalWidth: UInt32
  var physicalHeight: UInt32
  var scaleFactor: Double
}

@MainActor
final class MapState {
  nonisolated(unsafe) private(set) var runtime: OpaquePointer? = nil
  nonisolated(unsafe) private(set) var map: OpaquePointer? = nil
  nonisolated(unsafe) private(set) var surface: OpaquePointer? = nil

  init(viewport: Viewport, layer: CAMetalLayer) throws {
    var createdRuntime: OpaquePointer?
    var createdMap: OpaquePointer?
    var createdSurface: OpaquePointer?

    do {
      try Self.createRuntime(&createdRuntime)
      try Self.createMap(runtime: createdRuntime, viewport: viewport, outMap: &createdMap)
      try Self.loadStyle(map: createdMap)
      try Self.setInitialCamera(map: createdMap)
      try Self.attachSurface(map: createdMap, viewport: viewport, layer: layer, outSurface: &createdSurface)
    } catch {
      if let createdSurface { _ = mln_surface_destroy(createdSurface) }
      if let createdMap { _ = mln_map_destroy(createdMap) }
      if let createdRuntime { _ = mln_runtime_destroy(createdRuntime) }
      throw error
    }

    runtime = createdRuntime
    map = createdMap
    surface = createdSurface
  }

  deinit {
    if let surface { _ = mln_surface_destroy(surface) }
    if let map { _ = mln_map_destroy(map) }
    if let runtime { _ = mln_runtime_destroy(runtime) }
  }

  func resize(_ viewport: Viewport) throws {
    try checkCAPI(
      mln_surface_resize(surface, viewport.logicalWidth, viewport.logicalHeight, viewport.scaleFactor),
      "surface resize failed"
    )
  }

  func runOnce() {
    if let runtime { _ = mln_runtime_run_once(runtime) }
  }

  func drainEvents() throws -> Bool {
    var renderUpdateAvailable = false
    while true {
      var event = mln_runtime_event()
      event.size = UInt32(MemoryLayout<mln_runtime_event>.size)
      var hasEvent = false
      try checkCAPI(mln_runtime_poll_event(runtime, &event, &hasEvent), "event poll failed")
      if !hasEvent { return renderUpdateAvailable }
      if event.source_type == MLN_RUNTIME_EVENT_SOURCE_MAP.rawValue
        && event.source == UnsafeMutableRawPointer(map)
        && event.type == MLN_RUNTIME_EVENT_MAP_RENDER_UPDATE_AVAILABLE.rawValue
      {
        renderUpdateAvailable = true
      }
    }
  }

  func render() throws -> Bool {
    let status = mln_surface_render_update(surface)
    if status == MLN_STATUS_OK { return true }
    if status == MLN_STATUS_INVALID_STATE { return false }
    try checkCAPI(status, "surface render failed")
    return false
  }

  private static func createRuntime(_ outRuntime: inout OpaquePointer?) throws {
    var runtimeOptions = mln_runtime_options_default()
    try ":memory:".withCString { cachePath in
      runtimeOptions.cache_path = cachePath
      try checkCAPI(mln_runtime_create(&runtimeOptions, &outRuntime), "runtime create failed")
    }
  }

  private static func createMap(
    runtime: OpaquePointer?,
    viewport: Viewport,
    outMap: inout OpaquePointer?
  ) throws {
    var mapOptions = mln_map_options_default()
    mapOptions.width = viewport.logicalWidth
    mapOptions.height = viewport.logicalHeight
    mapOptions.scale_factor = viewport.scaleFactor
    mapOptions.map_mode = MLN_MAP_MODE_CONTINUOUS.rawValue
    try checkCAPI(mln_map_create(runtime, &mapOptions, &outMap), "map create failed")
  }

  private static func loadStyle(map: OpaquePointer?) throws {
    try "https://tiles.openfreemap.org/styles/bright".withCString { styleURL in
      try checkCAPI(mln_map_set_style_url(map, styleURL), "style load failed")
    }
  }

  private static func setInitialCamera(map: OpaquePointer?) throws {
    var camera = mln_camera_options_default()
    camera.fields = MLN_CAMERA_OPTION_CENTER.rawValue
      | MLN_CAMERA_OPTION_ZOOM.rawValue
      | MLN_CAMERA_OPTION_BEARING.rawValue
      | MLN_CAMERA_OPTION_PITCH.rawValue
    camera.latitude = 37.7749
    camera.longitude = -122.4194
    camera.zoom = 13.0
    camera.bearing = 12.0
    camera.pitch = 30.0
    try checkCAPI(mln_map_jump_to(map, &camera), "camera jump failed")
  }

  private static func attachSurface(
    map: OpaquePointer?,
    viewport: Viewport,
    layer: CAMetalLayer,
    outSurface: inout OpaquePointer?
  ) throws {
    var descriptor = mln_metal_surface_descriptor_default()
    descriptor.width = viewport.logicalWidth
    descriptor.height = viewport.logicalHeight
    descriptor.scale_factor = viewport.scaleFactor
    descriptor.layer = Unmanaged.passUnretained(layer).toOpaque()
    try checkCAPI(mln_metal_surface_attach(map, &descriptor, &outSurface), "Metal surface attach failed")
  }
}
