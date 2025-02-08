# MapLibre Native C Bindings Project Requirements Document

## Objective
Create minimal viable C bindings for MapLibre Native to enable cross-platform map integration in various languages (Kotlin, Rust, etc) while maintaining native performance characteristics.

## Core Components to Expose

### 1. Map Context (MLN_MapContext)
- Wraps `mbgl::Map` core
- Handles style loading, layers, sources
- Manages camera state (position, zoom, bearing, pitch)
- Requires platform-specific renderer frontend

### 2. Renderer Frontend Interface (MLN_RendererFrontend)
- Abstracted rendering interface for platform integration
- Must support multiple backends:
  - OpenGL ES (Linux, Windows, Android)
  - Metal (iOS, macOS)
  - Vulkan (Linux, Windows, Android)
- Handles surface creation/destruction
- Manages framebuffer dimensions

### 3. Resource Management
- File source configuration
- Asset path resolution
- Network request handling
- Cache management

### 4. View Integration
- Coordinate system transformations
- Gesture recognition interface
- Viewport padding configuration
- Frame scheduling (continuous vs on-demand)

## Binding Architecture

### Platform Adapter Implementation
1. **Rendering Surface**
   - Platform provides GL/Vulkan/Metal context management
   - Adapter implements frame buffer resize/swapchain handling
   - Coordinate system conversion (DPI/device pixels)

2. **Resource Loading**
   - File source implementation for platform-specific I/O
   - Network request handling with platform HTTP stack
   - Font/glyph management integration

3. **Input Handling**
   - Touch/gesture event translation to camera operations
   - Keyboard input processing for map interaction
   - Platform-to-world coordinate transformation

## Initialization Flow (GLFW Reference)
1. Platform creates native window + graphics context
2. Initialize MapLibre resource subsystem:
   - Configure file sources
   - Set up asset roots
   - Enable caching layers
3. Create renderer backend matching platform capabilities
4. Instantiate map context with platform adapter
5. Set up input event pipeline:
   - GLFW-style callbacks for mouse/keyboard
   - Touch gesture recognizers
   - Frame synchronization mechanism
6. Start main render loop:
   - Continuous vs on-demand rendering
   - VSync integration
   - Frame timing statistics

## Binding Patterns
1. **Object Lifetime**
   - C++ objects created via factory methods
   - Opaque handles prevent direct memory manipulation
   - Reference counting for shared resources

2. **Error Handling**
   - Error code enumeration (MLN_Error)
   - Stack trace capture in debug builds
   - Platform error callback registration

3. **Thread Safety**
   - Main thread affinity for rendering operations
   - Background thread resource loading
   - Message passing for cross-thread communication

## Cross-Platform Requirements
1. **Rendering Backends**
   - OpenGL ES 3.0 (Linux/Windows/Android)
   - Metal (macOS/iOS)
   - Vulkan (Linux/Windows/Android)
   - Headless testing support

2. **Resource Management**
   - Unified asset path resolution
   - Network request cancellation
   - Cache invalidation strategies

3. **Build System**
   - CMake-based cross-compilation
   - Per-platform dependency management
   - CI/CD for binding validation

## MVP Milestones
1. Basic map display with static style
2. Camera manipulation (pan/zoom/rotate)
3. Platform-optimized rendering backend
4. Resource loading pipeline
5. Basic gesture recognition interface

## Testing Strategy

### Testing Requirements
1. **Cross-Platform Validation**
   - Verify bindings on x86_64, arm64 architectures
   - Test Windows/Linux/macOS GL backends
   - Validate Android/iOS through emulation

2. **Binding Integrity Checks**
   - Memory leak detection with Valgrind/ASAN
   - Thread safety verification
   - Resource loading error simulations

3. **CI Pipeline**
   - GitHub Actions matrix builds
   - GL context virtualization for headless testing
   - Automated screenshot comparison

4. **Performance Benchmarks**
   - Frame time consistency metrics
   - Native/JVM memory boundary costs
   - Resource loading throughput
