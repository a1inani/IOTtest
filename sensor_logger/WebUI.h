#pragma once

namespace WebUI {
  /** Start Wi-Fi AP, DNS server (captive portal), and HTTP server. */
  void begin();

  /** Must be called from loop() – drives DNS and HTTP request handling. */
  void handle();
}
