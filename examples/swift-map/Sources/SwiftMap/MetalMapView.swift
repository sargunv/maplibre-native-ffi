import AppKit
import CMapLibreNativeABI
import Metal
import QuartzCore

@MainActor
final class MetalMapView: NSView {
  private let metalLayer = CAMetalLayer()
  private let device: MTLDevice?
  private let commandQueue: MTLCommandQueue?
  private let pipeline: MTLRenderPipelineState?
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
    if let device = MTLCreateSystemDefaultDevice(), let commandQueue = device.makeCommandQueue() {
      self.device = device
      self.commandQueue = commandQueue
      do {
        pipeline = try MetalMapView.makePipeline(device: device)
      } catch {
        pipeline = nil
        setupError = error
      }
    } else {
      device = nil
      commandQueue = nil
      pipeline = nil
      setupError = ABIError.failure("Metal is not available")
    }
    super.init(frame: .zero)

    wantsLayer = true
    layer = metalLayer
    metalLayer.device = device
    metalLayer.pixelFormat = .bgra8Unorm
    metalLayer.framebufferOnly = false
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
    guard setupError == nil, let device else { return }
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
    metalLayer.drawableSize = CGSize(width: Int(physicalWidth), height: Int(physicalHeight))

    guard viewport != currentViewport else { return }
    do {
      if mapState == nil {
        mapState = try MapState(viewport: viewport, device: device)
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
        renderPending = !(try drawFrame(textureSession: mapState.texture))
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

  private func drawFrame(textureSession: OpaquePointer?) throws -> Bool {
    guard let commandQueue, let pipeline else { return false }
    var frame = mln_metal_texture_frame()
    frame.size = UInt32(MemoryLayout<mln_metal_texture_frame>.size)
    let acquireStatus = mln_metal_texture_acquire_frame(textureSession, &frame)
    if acquireStatus == MLN_STATUS_INVALID_STATE { return false }
    try checkABI(acquireStatus, "Metal texture acquire failed")
    defer {
      if mln_metal_texture_release_frame(textureSession, &frame) != MLN_STATUS_OK {
        logABIError("Metal texture release failed")
      }
    }

    guard let texturePointer = frame.texture,
      let drawable = metalLayer.nextDrawable(),
      let commandBuffer = commandQueue.makeCommandBuffer()
    else { return false }

    // The ABI returns a borrowed id<MTLTexture>. Keep it borrowed and release
    // the ABI frame only after the command buffer has completed sampling it.
    let mapTexture = Unmanaged<AnyObject>.fromOpaque(texturePointer).takeUnretainedValue() as! MTLTexture
    let passDescriptor = MTLRenderPassDescriptor()
    passDescriptor.colorAttachments[0].texture = drawable.texture
    passDescriptor.colorAttachments[0].loadAction = .clear
    passDescriptor.colorAttachments[0].storeAction = .store
    passDescriptor.colorAttachments[0].clearColor = MTLClearColor(red: 0.08, green: 0.09, blue: 0.11, alpha: 1.0)

    guard let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor) else { return false }
    encoder.setRenderPipelineState(pipeline)
    encoder.setFragmentTexture(mapTexture, index: 0)
    encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
    encoder.endEncoding()
    commandBuffer.present(drawable)
    commandBuffer.commit()
    commandBuffer.waitUntilCompleted()
    return true
  }

  private static func makePipeline(device: MTLDevice) throws -> MTLRenderPipelineState {
    guard let shaderURL = Bundle.module.url(forResource: "MapShader", withExtension: "metal") else {
      throw ABIError.failure("MapShader.metal resource is missing")
    }
    let source = try String(contentsOf: shaderURL, encoding: .utf8)
    let library = try device.makeLibrary(source: source, options: nil)
    let descriptor = MTLRenderPipelineDescriptor()
    descriptor.vertexFunction = library.makeFunction(name: "vertex_main")
    descriptor.fragmentFunction = library.makeFunction(name: "fragment_main")
    descriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
    return try device.makeRenderPipelineState(descriptor: descriptor)
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
