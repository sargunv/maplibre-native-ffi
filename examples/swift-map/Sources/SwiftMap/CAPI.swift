import CMapLibreNativeC
import Foundation

@_cdecl("swift_map_log_callback")
func swiftMapLogCallback(
  userData: UnsafeMutableRawPointer?,
  severity: UInt32,
  event: UInt32,
  code: Int64,
  message: UnsafePointer<CChar>?
) -> UInt32 {
  let text = message.map { String(cString: $0) } ?? ""
  print("[MapLibre] severity=\(severity) event=\(event) code=\(code): \(text)")
  return 0
}

enum CAPIError: Error, CustomStringConvertible {
  case failure(String)

  var description: String {
    switch self {
    case .failure(let message): message
    }
  }
}

func checkCAPI(_ status: mln_status, _ context: String) throws {
  if status == MLN_STATUS_OK { return }
  let diagnostic = String(cString: mln_thread_last_error_message())
  if diagnostic.isEmpty {
    throw CAPIError.failure("\(context): status \(status.rawValue)")
  }
  throw CAPIError.failure("\(context): \(diagnostic)")
}

func logCAPIError(_ context: String) {
  let diagnostic = String(cString: mln_thread_last_error_message())
  if diagnostic.isEmpty {
    print("\(context): no C API diagnostic")
  } else {
    print("\(context): \(diagnostic)")
  }
}

func installCAPILogging() {
  if mln_log_set_callback(swiftMapLogCallback, nil) != MLN_STATUS_OK {
    logCAPIError("log callback install failed")
  }
}

func clearCAPILogging() {
  if mln_log_clear_callback() != MLN_STATUS_OK {
    logCAPIError("log callback clear failed")
  }
}

func logControls() {
  print(
    """
    Controls:
      left drag: pan
      right drag or Ctrl+left drag: rotate with X, pitch with Y
      scroll: zoom at cursor
      arrows or WASD: pan
      + / -: zoom at center
      Q / E: rotate
      PageUp / PageDown or [ / ]: pitch
      0: reset pitch and bearing

    """
  )
}
