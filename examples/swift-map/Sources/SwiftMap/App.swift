import AppKit
import SwiftUI

@main
struct SwiftMapApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

  var body: some Scene {
    WindowGroup {
      MapView()
        .frame(minWidth: 640, minHeight: 420)
    }
    .windowResizability(.contentMinSize)
  }
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
  static let willTerminateMapViews = Notification.Name("SwiftMapWillTerminateMapViews")

  func applicationDidFinishLaunching(_ notification: Notification) {
    NSApp.setActivationPolicy(.regular)
    NSApp.activate(ignoringOtherApps: true)
    installABILogging()
    logControls()
  }

  func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
    NotificationCenter.default.post(name: Self.willTerminateMapViews, object: nil)
    clearABILogging()
    return .terminateNow
  }
}

struct MapView: NSViewRepresentable {
  func makeNSView(context: Context) -> MetalMapView {
    MetalMapView()
  }

  func updateNSView(_ nsView: MetalMapView, context: Context) {}
}
