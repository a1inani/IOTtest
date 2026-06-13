#include "WebUI.h"
#include "config.h"
#include "SampleStore.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

// ─── module state ─────────────────────────────────────────────────────────────

static WebServer server(HTTP_PORT);
static DNSServer dns;
static const byte DNS_PORT = 53;

// ─── embedded HTML dashboard ──────────────────────────────────────────────────
// Stored in flash (code segment) – no SPIFFS upload needed.
// The page polls /api/current and /api/history every 5 s via fetch().

static const char DASHBOARD_HTML[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Sensor Logger</title>
<style>
:root{--accent:#2563eb;--bg:#f1f5f9;--card:#fff;--text:#1e293b;--muted:#64748b;--derived:#d97706}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--text);padding:20px}
header{text-align:center;margin-bottom:24px}
header h1{font-size:1.8rem;color:var(--accent)}
#status{font-size:.85rem;color:var(--muted);margin-top:4px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:16px;margin-bottom:32px}
.card{background:var(--card);border-radius:12px;padding:20px;text-align:center;box-shadow:0 1px 4px rgba(0,0,0,.08)}
.card.derived{border-top:3px solid var(--derived)}
.card .label{font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin-bottom:8px}
.card .value{font-size:2rem;font-weight:700;color:var(--accent);line-height:1.1}
.card.derived .value{color:var(--derived)}
.card .unit{font-size:.85rem;color:var(--muted);margin-top:4px}
h2{font-size:1.1rem;color:var(--accent);margin-bottom:8px}
.note{font-size:.78rem;color:var(--muted);margin-bottom:12px}
.wrap{overflow-x:auto}
table{width:100%;border-collapse:collapse;background:var(--card);border-radius:8px;overflow:hidden;box-shadow:0 1px 4px rgba(0,0,0,.08)}
th{background:var(--accent);color:#fff;padding:10px 14px;text-align:left;font-size:.82rem;font-weight:600}
td{padding:8px 14px;font-size:.85rem;border-bottom:1px solid #e2e8f0}
tr:last-child td{border-bottom:none}
tr:nth-child(even) td{background:#f8fafc}
.actions{margin-top:20px;display:flex;gap:10px;flex-wrap:wrap}
a.btn{display:inline-block;padding:8px 16px;background:var(--accent);color:#fff;border-radius:6px;text-decoration:none;font-size:.85rem}
a.btn.danger{background:#dc2626}
</style>
</head>
<body>
<header>
  <h1>&#127777; Sensor Logger</h1>
  <div id="status">Connecting&#8230;</div>
</header>

<div class="grid">
  <div class="card">
    <div class="label">Temperature</div>
    <div class="value" id="t_c">&#8212;</div>
    <div class="unit">&#176;C</div>
  </div>
  <div class="card">
    <div class="label">Temperature</div>
    <div class="value" id="t_f">&#8212;</div>
    <div class="unit">&#176;F</div>
  </div>
  <div class="card">
    <div class="label">Humidity</div>
    <div class="value" id="hum">&#8212;</div>
    <div class="unit">% RH</div>
  </div>
  <div class="card derived">
    <div class="label">Heat Index &#9733;</div>
    <div class="value" id="hi_c">&#8212;</div>
    <div class="unit">&#176;C</div>
  </div>
  <div class="card derived">
    <div class="label">Heat Index &#9733;</div>
    <div class="value" id="hi_f">&#8212;</div>
    <div class="unit">&#176;F</div>
  </div>
</div>

<h2>Recent Readings</h2>
<p class="note">&#9733; Heat index is a <strong>derived</strong> value computed from temperature + humidity (Steadman formula). Most meaningful above 27&#176;C / 80&#176;F.</p>
<div class="wrap">
<table>
  <thead>
    <tr>
      <th>Uptime</th>
      <th>Temp &#176;C</th>
      <th>Temp &#176;F</th>
      <th>Humidity %</th>
      <th>HI &#176;C &#9733;</th>
      <th>HI &#176;F &#9733;</th>
    </tr>
  </thead>
  <tbody id="tbody">
    <tr><td colspan="6" style="text-align:center;color:#94a3b8">Loading&#8230;</td></tr>
  </tbody>
</table>
</div>

<div class="actions">
  <a class="btn" href="/api/log" download="sensor_log.csv">&#11015; Download CSV Log</a>
  <a class="btn danger" href="/api/log/clear" onclick="return confirm('Clear the persistent log file?')">&#128465; Clear Log</a>
</div>

<script>
function fmt(ms){
  var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  return h+'h '+(m%60)+'m '+(s%60)+'s';
}
function refresh(){
  fetch('/api/current')
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.valid){
        document.getElementById('t_c').textContent=d.temperature_c;
        document.getElementById('t_f').textContent=d.temperature_f;
        document.getElementById('hum').textContent=d.humidity;
        document.getElementById('hi_c').textContent=d.heat_index_c;
        document.getElementById('hi_f').textContent=d.heat_index_f;
        document.getElementById('status').textContent='Last updated: uptime '+fmt(d.uptime_ms);
      } else {
        document.getElementById('status').textContent='Sensor error \u2013 retrying\u2026';
      }
    })
    .catch(function(){
      document.getElementById('status').textContent='Connection error \u2013 retrying\u2026';
    });

  fetch('/api/history')
    .then(function(r){return r.json();})
    .then(function(data){
      var rows=data.readings||[];
      var tbody=document.getElementById('tbody');
      if(!rows.length){
        tbody.innerHTML='<tr><td colspan="6" style="text-align:center;color:#94a3b8">No readings yet</td></tr>';
        return;
      }
      var html='';
      for(var i=rows.length-1;i>=0;i--){
        var r=rows[i];
        html+='<tr><td>'+fmt(r.uptime_ms)+'</td>'+
              '<td>'+r.temperature_c+'</td>'+
              '<td>'+r.temperature_f+'</td>'+
              '<td>'+r.humidity+'</td>'+
              '<td>'+r.heat_index_c+'</td>'+
              '<td>'+r.heat_index_f+'</td></tr>';
      }
      tbody.innerHTML=html;
    });
}
refresh();
setInterval(refresh,5000);
</script>
</body>
</html>
)rawhtml";

// ─── route handlers ───────────────────────────────────────────────────────────

static void handleRoot() {
  server.send(200, F("text/html"), DASHBOARD_HTML);
}

static void handleCurrentJson() {
  SensorReading r = SampleStore::getLatest();
  char buf[256];
  if (r.valid) {
    snprintf(buf, sizeof(buf),
             "{\"valid\":true,\"uptime_ms\":%lu,"
             "\"temperature_c\":%.1f,\"temperature_f\":%.1f,"
             "\"humidity\":%.1f,"
             "\"heat_index_c\":%.1f,\"heat_index_f\":%.1f}",
             r.uptime_ms,
             r.temperature_c, r.temperature_f,
             r.humidity,
             r.heat_index_c, r.heat_index_f);
  } else {
    snprintf(buf, sizeof(buf), "{\"valid\":false}");
  }
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("application/json"), buf);
}

static void handleHistoryJson() {
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("application/json"), SampleStore::historyJson());
}

static void handleLog() {
  if (!LittleFS.exists(LOG_FILE_PATH)) {
    server.send(200, F("text/csv"), F("# No log entries yet\n"));
    return;
  }
  File f = LittleFS.open(LOG_FILE_PATH, "r");
  if (!f) {
    server.send(500, F("text/plain"), F("Failed to open log file"));
    return;
  }
  server.sendHeader(F("Content-Disposition"),
                    F("attachment; filename=\"sensor_log.csv\""));
  server.streamFile(f, F("text/csv"));
  f.close();
}

static void handleLogClear() {
  SampleStore::clearLog();
  server.sendHeader(F("Location"), F("/"));
  server.send(302);
}

// Captive-portal redirect: send all unrecognised requests to the dashboard
static void handleCaptive() {
  server.sendHeader(F("Location"), F("http://192.168.4.1/"));
  server.send(302);
}

// ─── public API ───────────────────────────────────────────────────────────────

void WebUI::begin() {
  // ── Wi-Fi access point ──────────────────────────────────────────────────────
  WiFi.mode(WIFI_AP);
  if (strlen(AP_PASSWORD) >= 8) {
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  } else {
    WiFi.softAP(AP_SSID);  // open network
  }

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WebUI] AP started  SSID: \"%s\"  IP: %s\n",
                AP_SSID, ip.toString().c_str());

  // ── DNS server (captive portal) ─────────────────────────────────────────────
  // Resolves every hostname to the AP IP so connecting clients are redirected.
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(DNS_PORT, "*", ip);
  Serial.println("[WebUI] DNS captive-portal server started.");

  // ── HTTP routes ─────────────────────────────────────────────────────────────
  server.on("/",                          handleRoot);
  server.on("/index.html",                handleRoot);
  server.on("/api/current",               handleCurrentJson);
  server.on("/api/history",               handleHistoryJson);
  server.on("/api/log",                   handleLog);
  server.on("/api/log/clear",             handleLogClear);

  // Known captive-portal detection endpoints (Android, iOS, Windows, Firefox)
  server.on("/generate_204",              handleCaptive);  // Android
  server.on("/hotspot-detect.html",       handleCaptive);  // iOS (legacy)
  server.on("/library/test/success.html", handleCaptive);  // iOS
  server.on("/connecttest.txt",           handleCaptive);  // Windows
  server.on("/redirect",                  handleCaptive);  // Windows
  server.on("/canonical.html",            handleCaptive);  // Firefox

  // Catch-all – redirect anything else to the dashboard
  server.onNotFound(handleCaptive);

  server.begin();
  Serial.printf("[WebUI] HTTP server started on port %d.\n", HTTP_PORT);
}

void WebUI::handle() {
  dns.processNextRequest();
  server.handleClient();
}
