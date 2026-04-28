import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameMillis
import androidx.compose.ui.awt.ComposeWindow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import kotlinx.coroutines.isActive
import org.jetbrains.skia.BackendRenderTarget
import org.jetbrains.skia.ContentChangeMode
import org.jetbrains.skia.DirectContext
import org.jetbrains.skia.Image
import org.jetbrains.skia.Rect
import org.jetbrains.skia.SamplingMode
import org.jetbrains.skia.Surface
import org.jetbrains.skia.SurfaceColorFormat
import org.jetbrains.skia.SurfaceOrigin
import org.jetbrains.skiko.SkiaLayer
import java.awt.Component
import java.awt.Container
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

fun main() {
  application {
    Window(onCloseRequest = ::exitApplication, title = "Compose GPU Texture PoC") {
      App(window)
    }
  }
}

@Composable
private fun App(window: ComposeWindow) {
  var frame by remember { mutableIntStateOf(0) }
  val windowReport = remember(window) { ComposeWindowReport.collect(window) }
  val renderer = remember(window) { ExternalGpuRenderer(windowReport.skiaLayer) }
  var status by remember { mutableStateOf("Waiting for first Compose canvas") }
  var minAlpha by remember { mutableStateOf(0.68f) }
  var rotationAmplitude by remember { mutableStateOf(18f) }
  var scaleAmplitude by remember { mutableStateOf(0.16f) }
  var scaleXAmplitude by remember { mutableStateOf(0.12f) }
  var cornerRadius by remember { mutableStateOf(44f) }

  LaunchedEffect(Unit) {
    while (isActive) {
      withFrameMillis { frame = (frame + 1) % 3600 }
    }
  }

  DisposableEffect(renderer) {
    onDispose { renderer.close() }
  }

  Row(
    modifier = Modifier.fillMaxSize().background(Color(0xFF101010)).padding(24.dp),
    horizontalArrangement = Arrangement.spacedBy(24.dp),
  ) {
    Column(
      modifier = Modifier.weight(1f).fillMaxSize(),
      verticalArrangement = Arrangement.Center,
      horizontalAlignment = Alignment.CenterHorizontally,
    ) {
      val phase = frame * (2.0 * PI / 180.0)
      val animatedAlpha = minAlpha + (1f - minAlpha) * ((sin(phase) + 1.0) / 2.0).toFloat()
      val animatedRotation = (sin(phase * 0.7) * rotationAmplitude).toFloat()
      val animatedScale = 1f + (cos(phase * 0.9) * scaleAmplitude).toFloat()
      val animatedScaleX = animatedScale * (1f + (sin(phase * 1.3) * scaleXAmplitude).toFloat())
      val animatedCornerRadius = (cornerRadius * (0.35f + 0.65f * ((cos(phase * 0.8) + 1.0) / 2.0).toFloat()))
        .coerceAtLeast(0f)
      val shape = RoundedCornerShape(animatedCornerRadius.dp)
      Box(
        modifier = Modifier
          .size(560.dp, 360.dp)
          .graphicsLayer {
            this.alpha = animatedAlpha
            rotationZ = animatedRotation
            scaleX = animatedScaleX
            scaleY = animatedScale
            this.shape = shape
            clip = animatedCornerRadius > 0f
          }
          .background(Color.Black, shape)
          .border(1.dp, Color(0xFF555555), shape),
      ) {
        Canvas(Modifier.fillMaxSize()) {
          drawIntoCanvas { composeCanvas ->
            val result = renderer.renderAndDraw(
              composeCanvas.nativeCanvas,
              targetWidth = size.width.roundToInt().coerceAtLeast(1),
              targetHeight = size.height.roundToInt().coerceAtLeast(1),
              frame = frame,
            )
            status = result
          }
        }
      }
    }

    Column(
      modifier = Modifier.width(320.dp),
      verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
      Text("Native Metal texture in Compose", color = Color.White)
      ControlSlider("min alpha", minAlpha, 0.2f..1f) { minAlpha = it }
      ControlSlider("rotation amplitude", rotationAmplitude, 0f..35f) { rotationAmplitude = it }
      ControlSlider("scale amplitude", scaleAmplitude, 0f..0.35f) { scaleAmplitude = it }
      ControlSlider("scaleX amplitude", scaleXAmplitude, 0f..0.35f) { scaleXAmplitude = it }
      ControlSlider("corner radius", cornerRadius, 0f..96f) { cornerRadius = it }
      Text(status, color = Color(0xFFB8B8B8))
      Text("Skiko: ${windowReport.summary}", color = Color(0xFF777777))
    }
  }
}

@Composable
private fun ControlSlider(label: String, value: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
  Column {
    Row(
      modifier = Modifier.fillMaxWidth(),
      horizontalArrangement = Arrangement.SpaceBetween,
      verticalAlignment = Alignment.CenterVertically,
    ) {
      Text(label, color = Color.White)
      Text("%.2f".format(value), color = Color(0xFFB8B8B8))
    }
    Slider(value = value, onValueChange = onChange, valueRange = range)
  }
}

private class ExternalGpuRenderer(private val skiaLayer: SkiaLayer?) : AutoCloseable {
  private var surface: Surface? = null
  private var renderTarget: BackendRenderTarget? = null
  private var nativeTextureHandle = 0L
  private var width = 0
  private var height = 0
  private var contextIdentity = 0
  private var metalDevicePtr = 0L
  private var lastResult: String? = null
  private val retainedImages = ArrayDeque<Image>()

  fun renderAndDraw(canvas: org.jetbrains.skia.Canvas, targetWidth: Int, targetHeight: Int, frame: Int): String {
    val metalAccess = ComposeMetalAccess.from(skiaLayer)
    val context = metalAccess.context
      ?: return report(
        "Waiting for Skiko Metal context: ${metalAccess.status}",
      )

    val identity = System.identityHashCode(context)
    ensureSurface(context, identity, metalAccess.devicePtr, targetWidth, targetHeight)
    val gpuSurface = surface ?: return report("Failed to create GPU offscreen surface")

    NativeMetalBridge.render(nativeTextureHandle, frame)
    gpuSurface.notifyContentWillChange(ContentChangeMode.DISCARD)
    val image = gpuSurface.makeImageSnapshot()
    retainImageForRecordedFrame(image)
    drawImage(canvas, image, targetWidth, targetHeight)

    return report(
      "GPU path active: renderApi=${skiaLayer?.renderApi}, native Metal texture=${targetWidth}x$targetHeight",
    )
  }

  private fun report(result: String): String {
    if (result != lastResult) {
      lastResult = result
      println(result)
    }
    return result
  }

  private fun ensureSurface(context: DirectContext, identity: Int, devicePtr: Long, targetWidth: Int, targetHeight: Int) {
    if (surface != null &&
      width == targetWidth &&
      height == targetHeight &&
      contextIdentity == identity &&
      metalDevicePtr == devicePtr
    ) return

    closeGpuResources()
    width = targetWidth
    height = targetHeight
    contextIdentity = identity
    metalDevicePtr = devicePtr
    nativeTextureHandle = NativeMetalBridge.create(devicePtr, targetWidth, targetHeight)
    check(nativeTextureHandle != 0L) { "Native Metal texture creation failed" }
    renderTarget = BackendRenderTarget.makeMetal(
      width = targetWidth,
      height = targetHeight,
      texturePtr = NativeMetalBridge.texturePtr(nativeTextureHandle),
    )
    surface = Surface.makeFromBackendRenderTarget(
      context = context,
      rt = renderTarget!!,
      origin = SurfaceOrigin.TOP_LEFT,
      colorFormat = SurfaceColorFormat.BGRA_8888,
      colorSpace = null,
      surfaceProps = null,
    ) ?: error("Skia could not wrap native Metal texture")
  }

  private fun drawImage(canvas: org.jetbrains.skia.Canvas, image: Image, targetWidth: Int, targetHeight: Int) {
    canvas.drawImageRect(
      image = image,
      src = Rect.makeWH(image.width.toFloat(), image.height.toFloat()),
      dst = Rect.makeWH(targetWidth.toFloat(), targetHeight.toFloat()),
      samplingMode = SamplingMode.LINEAR,
      paint = null,
      strict = true,
    )
  }

  private fun retainImageForRecordedFrame(image: Image) {
    retainedImages.addLast(image)
    while (retainedImages.size > 8) {
      retainedImages.removeFirst().close()
    }
  }

  override fun close() {
    closeGpuResources()
    width = 0
    height = 0
    contextIdentity = 0
    metalDevicePtr = 0L
  }

  private fun closeGpuResources() {
    while (retainedImages.isNotEmpty()) {
      retainedImages.removeFirst().close()
    }
    surface?.close()
    surface = null
    renderTarget?.close()
    renderTarget = null
    if (nativeTextureHandle != 0L) {
      NativeMetalBridge.dispose(nativeTextureHandle)
      nativeTextureHandle = 0L
    }
  }
}

private object NativeMetalBridge {
  init {
    System.loadLibrary("compose_map_metal")
  }

  external fun create(skikoMetalDevicePtr: Long, width: Int, height: Int): Long
  external fun dispose(handle: Long)
  external fun texturePtr(handle: Long): Long
  external fun render(handle: Long, frame: Int)
}

private data class ComposeWindowReport(val skiaLayer: SkiaLayer?, val summary: String) {
  companion object {
    fun collect(window: ComposeWindow): ComposeWindowReport {
      val skiaLayer = window.findSkiaLayer()
        ?: return ComposeWindowReport(null, "no SkiaLayer")
      return ComposeWindowReport(skiaLayer, "renderApi=${skiaLayer.renderApi}")
    }
  }
}

// This is the demo's only intentional Skiko gap: public access to the live Metal
// DirectContext and device backing the current Compose scene.
private data class ComposeMetalAccess(val context: DirectContext?, val devicePtr: Long, val status: String) {
  companion object {
    fun from(skiaLayer: SkiaLayer?): ComposeMetalAccess {
      if (skiaLayer == null) return ComposeMetalAccess(null, 0L, "no SkiaLayer")
      return runCatching {
        val redrawer = skiaLayer.javaClass.getMethod("getRedrawer\$skiko").invoke(skiaLayer)
          ?: return ComposeMetalAccess(null, 0L, "SkiaLayer has no redrawer yet")
        val contextHandler = redrawer.findFieldValue("contextHandler")
          ?: return ComposeMetalAccess(null, 0L, "${redrawer.javaClass.name} has no contextHandler")
        val context = contextHandler.findFieldValue("context") as? DirectContext
          ?: return ComposeMetalAccess(null, 0L, "${contextHandler.javaClass.name} has no DirectContext yet")
        val device = contextHandler.findFieldValue("device")
          ?: return ComposeMetalAccess(null, 0L, "${contextHandler.javaClass.name} has no Metal device")
        val devicePtr = when (device) {
          is Long -> device
          else -> device.findFieldValue("ptr") as? Long
        } ?: return ComposeMetalAccess(null, 0L, "${device.javaClass.name} has no native pointer")
        ComposeMetalAccess(context, devicePtr, "found ${contextHandler.javaClass.simpleName}")
      }.getOrElse { error ->
        ComposeMetalAccess(null, 0L, "reflection failed: ${error.javaClass.simpleName}: ${error.message}")
      }
    }
  }
}

private fun Any.findFieldValue(name: String): Any? {
  var type: Class<*>? = javaClass
  while (type != null) {
    val field = type.declaredFields.firstOrNull { it.name == name }
    if (field != null) {
      field.isAccessible = true
      return field.get(this)
    }
    type = type.superclass
  }
  return null
}

private fun ComposeWindow.findSkiaLayer(): SkiaLayer? {
  return findSkiaLayerIn(this)
}

private fun findSkiaLayerIn(component: Component): SkiaLayer? {
  if (component is SkiaLayer) return component
  if (component is Container) {
    component.components.forEach { child ->
      findSkiaLayerIn(child)?.let { return it }
    }
  }
  return null
}
