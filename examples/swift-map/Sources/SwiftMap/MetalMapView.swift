import AppKit
import CMapLibreNativeC
import QuartzCore

@MainActor
final class MetalMapView: NSView {
  private let metalLayer = CAMetalLayer()
  private let input = InputController()
  private var mapState: MapState?
  private var timer: Timer?
  private var currentViewport: Viewport?
  private var renderPending = true
  private var consecutiveRenderFailures = 0
  private var setupError: Error?
  private var errorLabel: NSTextField?

  override var acceptsFirstResponder: Bool { true }

  init() {
    super.init(frame: .zero)

    wantsLayer = true
    layer = metalLayer
    postsFrameChangedNotifications = true
    NotificationCenter.default.addObserver(
      self,
      selector: #selector(shutdown),
      name: AppDelegate.willTerminateMapViews,
      object: nil
    )
    if let setupError {
      showError(setupError)
    }
  }

  required init?(coder: NSCoder) {
    return nil
  }

  override func viewDidMoveToWindow() {
    super.viewDidMoveToWindow()
    window?.makeFirstResponder(self)
    startTimerIfNeeded()
    updateViewport()
  }

  override func viewWillMove(toWindow newWindow: NSWindow?) {
    super.viewWillMove(toWindow: newWindow)
    if newWindow == nil {
      shutdown()
    }
  }

  @objc private func shutdown() {
    timer?.invalidate()
    timer = nil
    mapState = nil
    NotificationCenter.default.removeObserver(self)
  }

  override func layout() {
    super.layout()
    updateViewport()
  }

  override func viewDidChangeBackingProperties() {
    super.viewDidChangeBackingProperties()
    updateViewport()
  }

  override func mouseDown(with event: NSEvent) {
    handleInput { map in try input.mouseDown(event, map: map) }
  }

  override func rightMouseDown(with event: NSEvent) {
    handleInput { map in try input.rightMouseDown(event, map: map) }
  }

  override func mouseUp(with event: NSEvent) {
    if input.mouseUp(event) { renderPending = true }
  }

  override func rightMouseUp(with event: NSEvent) {
    if input.mouseUp(event) { renderPending = true }
  }

  override func mouseDragged(with event: NSEvent) {
    handleInput { map in try input.mouseDragged(event, map: map) }
  }

  override func rightMouseDragged(with event: NSEvent) {
    handleInput { map in try input.mouseDragged(event, map: map) }
  }

  override func scrollWheel(with event: NSEvent) {
    handleInput { map in try input.scrollWheel(event, map: map, in: self) }
  }

  override func keyDown(with event: NSEvent) {
    guard let viewport = currentViewport else { return }
    handleInput { map in try input.keyDown(event, map: map, viewport: viewport) }
  }

  private func handleInput(_ action: (OpaquePointer) throws -> Bool) {
    guard let map = mapState?.map else { return }
    do {
      if try action(map) { renderPending = true }
    } catch {
      print(error)
    }
  }

  private func startTimerIfNeeded() {
    guard timer == nil else { return }
    timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
      Task { @MainActor in self?.tick() }
    }
    RunLoop.main.add(timer!, forMode: .common)
  }

  private func updateViewport() {
    guard setupError == nil else { return }
    guard bounds.width > 0, bounds.height > 0 else { return }
    let scale = window?.backingScaleFactor ?? NSScreen.main?.backingScaleFactor ?? 1.0
    let logicalWidth = max(UInt32(ceil(bounds.width)), 1)
    let logicalHeight = max(UInt32(ceil(bounds.height)), 1)
    let physicalWidth = max(UInt32(ceil(bounds.width * scale)), 1)
    let physicalHeight = max(UInt32(ceil(bounds.height * scale)), 1)
    let viewport = Viewport(
      logicalWidth: logicalWidth,
      logicalHeight: logicalHeight,
      physicalWidth: physicalWidth,
      physicalHeight: physicalHeight,
      scaleFactor: scale
    )
    metalLayer.contentsScale = scale

    guard viewport != currentViewport else { return }
    do {
      if mapState == nil {
        mapState = try MapState(viewport: viewport, layer: metalLayer)
      } else {
        try mapState?.resize(viewport)
      }
      currentViewport = viewport
      renderPending = true
    } catch {
      print(error)
      showError(error)
    }
  }

  private func tick() {
    guard let mapState else { return }
    do {
      mapState.runOnce()
      renderPending = try mapState.drainEvents() || renderPending
      guard renderPending else { return }
      if try mapState.render() {
        renderPending = false
        consecutiveRenderFailures = 0
      }
    } catch {
      print(error)
      consecutiveRenderFailures += 1
      if consecutiveRenderFailures >= 3 {
        timer?.invalidate()
        timer = nil
        showError(error)
      }
    }
  }

  private func showError(_ error: Error) {
    if errorLabel == nil {
      let label = NSTextField(labelWithString: "")
      label.translatesAutoresizingMaskIntoConstraints = false
      label.maximumNumberOfLines = 0
      label.alignment = .center
      addSubview(label)
      NSLayoutConstraint.activate([
        label.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor, constant: 24),
        label.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -24),
        label.centerXAnchor.constraint(equalTo: centerXAnchor),
        label.centerYAnchor.constraint(equalTo: centerYAnchor),
      ])
      errorLabel = label
    }
    errorLabel?.stringValue = String(describing: error)
  }
}
