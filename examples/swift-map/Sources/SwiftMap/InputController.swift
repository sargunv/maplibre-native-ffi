import AppKit
import CMapLibreNativeC

private let keyboardAnimationDurationMS = 160.0
private let resetAnimationDurationMS = 220.0
private let preciseScrollDeltaPerWheelStep = 10.0

@MainActor
final class InputController {
  enum DragMode {
    case none
    case pan
    case rotate
  }

  private var dragMode = DragMode.none
  private var lastLocation = CGPoint.zero

  func mouseDown(_ event: NSEvent, map: OpaquePointer?) throws -> Bool {
    lastLocation = event.locationInWindow
    dragMode = event.modifierFlags.contains(.control) ? .rotate : .pan
    try checkCAPI(mln_map_cancel_transitions(map), "cancel camera transitions failed")
    return true
  }

  func rightMouseDown(_ event: NSEvent, map: OpaquePointer?) throws -> Bool {
    lastLocation = event.locationInWindow
    dragMode = .rotate
    try checkCAPI(mln_map_cancel_transitions(map), "cancel camera transitions failed")
    return true
  }

  func mouseUp(_ event: NSEvent) -> Bool {
    lastLocation = event.locationInWindow
    dragMode = .none
    return true
  }

  func mouseDragged(_ event: NSEvent, map: OpaquePointer?) throws -> Bool {
    let location = event.locationInWindow
    let dx = Double(location.x - lastLocation.x)
    let dy = Double(lastLocation.y - location.y)
    defer { lastLocation = location }

    switch dragMode {
    case .none:
      return false
    case .pan:
      if dx == 0 && dy == 0 { return true }
      try checkCAPI(mln_map_move_by(map, dx, dy), "camera pan failed")
    case .rotate:
      if dx == 0 && dy == 0 { return true }
      try adjustBearing(map, dx * 0.5)
      try adjustPitch(map, -dy / 2.0)
    }
    return true
  }

  func scrollWheel(_ event: NSEvent, map: OpaquePointer?, in view: NSView) throws -> Bool {
    let rawDelta = -Double(event.scrollingDeltaY)
    let delta = event.hasPreciseScrollingDeltas ? rawDelta / preciseScrollDeltaPerWheelStep : rawDelta
    if delta == 0 { return true }

    let location = view.convert(event.locationInWindow, from: nil)
    var anchor = mln_screen_point(x: Double(location.x), y: Double(view.bounds.height - location.y))
    let scale = pow(2.0, delta * 0.25)
    try checkCAPI(mln_map_scale_by(map, scale, &anchor), "camera zoom failed")
    return true
  }

  func keyDown(_ event: NSEvent, map: OpaquePointer?, viewport: Viewport) throws -> Bool {
    let panStep = 120.0
    let zoomStep = 1.25
    let bearingStep = 10.0
    let pitchStep = 5.0
    var animation = cameraAnimation(durationMS: keyboardAnimationDurationMS)
    var center = mln_screen_point(
      x: Double(viewport.logicalWidth) / 2.0,
      y: Double(viewport.logicalHeight) / 2.0
    )

    switch event.keyCode {
    case 123, 0:
      try checkCAPI(mln_map_move_by_animated(map, panStep, 0, &animation), "keyboard pan failed")
    case 124, 2:
      try checkCAPI(mln_map_move_by_animated(map, -panStep, 0, &animation), "keyboard pan failed")
    case 126, 13:
      try checkCAPI(mln_map_move_by_animated(map, 0, panStep, &animation), "keyboard pan failed")
    case 125, 1:
      try checkCAPI(mln_map_move_by_animated(map, 0, -panStep, &animation), "keyboard pan failed")
    case 24, 69:
      try checkCAPI(mln_map_scale_by_animated(map, zoomStep, &center, &animation), "keyboard zoom failed")
    case 27, 78:
      try checkCAPI(mln_map_scale_by_animated(map, 1.0 / zoomStep, &center, &animation), "keyboard zoom failed")
    case 12:
      try adjustBearingAnimated(map, -bearingStep, animation: &animation)
    case 14:
      try adjustBearingAnimated(map, bearingStep, animation: &animation)
    case 116, 30:
      try adjustPitchAnimated(map, pitchStep, animation: &animation)
    case 121, 33:
      try adjustPitchAnimated(map, -pitchStep, animation: &animation)
    case 29:
      var resetAnimation = cameraAnimation(durationMS: resetAnimationDurationMS)
      try resetPitchAndBearingAnimated(map, animation: &resetAnimation)
    default:
      return false
    }
    return true
  }

  private func adjustBearing(_ map: OpaquePointer?, _ delta: Double) throws {
    var camera = try currentCamera(map)
    camera.fields = MLN_CAMERA_OPTION_BEARING.rawValue
    camera.bearing += delta
    try checkCAPI(mln_map_jump_to(map, &camera), "keyboard rotate failed")
  }

  private func adjustBearingAnimated(
    _ map: OpaquePointer?,
    _ delta: Double,
    animation: UnsafePointer<mln_animation_options>
  ) throws {
    var camera = try currentCamera(map)
    camera.fields = MLN_CAMERA_OPTION_BEARING.rawValue
    camera.bearing += delta
    try checkCAPI(mln_map_ease_to(map, &camera, animation), "keyboard rotate failed")
  }

  private func adjustPitch(_ map: OpaquePointer?, _ delta: Double) throws {
    var camera = try currentCamera(map)
    camera.fields = MLN_CAMERA_OPTION_PITCH.rawValue
    camera.pitch = min(max(camera.pitch + delta, 0.0), 60.0)
    try checkCAPI(mln_map_jump_to(map, &camera), "keyboard pitch failed")
  }

  private func adjustPitchAnimated(
    _ map: OpaquePointer?,
    _ delta: Double,
    animation: UnsafePointer<mln_animation_options>
  ) throws {
    var camera = try currentCamera(map)
    camera.fields = MLN_CAMERA_OPTION_PITCH.rawValue
    camera.pitch = min(max(camera.pitch + delta, 0.0), 60.0)
    try checkCAPI(mln_map_ease_to(map, &camera, animation), "keyboard pitch failed")
  }

  private func resetPitchAndBearingAnimated(
    _ map: OpaquePointer?,
    animation: UnsafePointer<mln_animation_options>
  ) throws {
    var camera = mln_camera_options_default()
    camera.fields = MLN_CAMERA_OPTION_BEARING.rawValue | MLN_CAMERA_OPTION_PITCH.rawValue
    camera.bearing = 0
    camera.pitch = 0
    try checkCAPI(mln_map_ease_to(map, &camera, animation), "camera reset failed")
  }

  private func currentCamera(_ map: OpaquePointer?) throws -> mln_camera_options {
    var camera = mln_camera_options_default()
    try checkCAPI(mln_map_get_camera(map, &camera), "camera snapshot failed")
    return camera
  }

  private func cameraAnimation(durationMS: Double) -> mln_animation_options {
    var animation = mln_animation_options_default()
    animation.fields = MLN_ANIMATION_OPTION_DURATION.rawValue
    animation.duration_ms = durationMS
    return animation
  }
}
