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
// Stored in flash (program memory) – no SPIFFS upload needed.
// The page polls /api/current and /api/history every 5 s via fetch().

static const char DASHBOARD_HTML[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Sensor Logger</title>
<style>
:root{--accent:#2563eb;--bg:#f1f5f9;--card:#fff;--text:#1e293b;--muted:#64748b;--derived:#d97706;--ok:#16a34a;--warn:#dc2626}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--text);padding:20px}
header{text-align:center;margin-bottom:24px}
header h1{font-size:1.8rem;color:var(--accent)}
#status{font-size:.85rem;color:var(--muted);margin-top:4px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:14px;margin-bottom:28px}
.card{background:var(--card);border-radius:12px;padding:16px;text-align:center;box-shadow:0 1px 4px rgba(0,0,0,.08)}
.card.derived{border-top:3px solid var(--derived)}
.card.rain-yes{border-top:3px solid var(--ok)}
.card.pump-on{border-top:3px solid var(--warn)}
.card .label{font-size:.72rem;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin-bottom:6px}
.card .value{font-size:1.9rem;font-weight:700;color:var(--accent);line-height:1.1}
.card.derived .value{color:var(--derived)}
.card .unit{font-size:.8rem;color:var(--muted);margin-top:4px}
h2{font-size:1.1rem;color:var(--accent);margin-bottom:8px}
.note{font-size:.78rem;color:var(--muted);margin-bottom:10px}
section{margin-bottom:28px}
.pump-controls{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-top:8px}
.diag{font-size:.82rem;color:var(--muted);line-height:1.5;margin-top:8px}
.diag code{font-size:.8rem;background:#e2e8f0;padding:1px 5px;border-radius:4px}
a.btn,button.btn{display:inline-block;padding:8px 18px;border-radius:6px;text-decoration:none;font-size:.85rem;font-weight:600;color:#fff;border:0;cursor:pointer}
a.btn.on,button.btn.on{background:var(--ok)}
a.btn.off,button.btn.off{background:var(--warn)}
a.btn.dl,button.btn.dl{background:var(--accent)}
a.btn.clr,button.btn.clr{background:#64748b}
.wrap{overflow-x:auto}
table{width:100%;border-collapse:collapse;background:var(--card);border-radius:8px;overflow:hidden;box-shadow:0 1px 4px rgba(0,0,0,.08)}
th{background:var(--accent);color:#fff;padding:9px 12px;text-align:left;font-size:.78rem;font-weight:600;white-space:nowrap}
td{padding:7px 12px;font-size:.82rem;border-bottom:1px solid #e2e8f0;white-space:nowrap}
tr:last-child td{border-bottom:none}
tr:nth-child(even) td{background:#f8fafc}
</style>
</head>
<body>
<header>
  <h1>&#127777; Sensor Logger</h1>
  <div id="status">Connecting&#8230;</div>
</header>

<div class="grid">
  <div class="card"><div class="label">Temperature</div><div class="value" id="t_c">&#8212;</div><div class="unit">&#176;C</div></div>
  <div class="card"><div class="label">Temperature</div><div class="value" id="t_f">&#8212;</div><div class="unit">&#176;F</div></div>
  <div class="card"><div class="label">Humidity</div><div class="value" id="hum">&#8212;</div><div class="unit">% RH</div></div>
  <div class="card derived"><div class="label">Pressure</div><div class="value" id="pres">&#8212;</div><div class="unit">hPa</div></div>
  <div class="card derived"><div class="label">Altitude &#9733;</div><div class="value" id="alt">&#8212;</div><div class="unit">m</div></div>
  <div class="card"><div class="label">Water Level</div><div class="value" id="wl_pct">&#8212;</div><div class="unit">%</div></div>
  <div class="card derived"><div class="label">Soil pH &#9733;</div><div class="value" id="ph_val">&#8212;</div><div class="unit">pH (est.)</div></div>
  <div class="card" id="rain_card"><div class="label">Rain / Wet</div><div class="value" id="rain_state">&#8212;</div><div class="unit" id="rain_raw_lbl"></div></div>
  <div class="card" id="pump_card"><div class="label">Pump</div><div class="value" id="pump_state">&#8212;</div><div class="unit">relay</div></div>
</div>

<section>
  <h2>Pump Control</h2>
  <p class="note">The pump will auto-off after the configured safety time even if no OFF command is sent.</p>
  <div class="pump-controls">
    <button class="btn on" id="btn_pump_on" type="button" aria-label="Turn pump on">&#9889; Turn ON</button>
    <button class="btn off" id="btn_pump_off" type="button" aria-label="Turn pump off">&#9632; Turn OFF</button>
  </div>
  <div class="diag" id="pump_diag">Pump diagnostics: waiting for data…</div>
</section>

<section>
  <h2>Recent Readings</h2>
  <p class="note">&#9733; Altitude is derived from barometric pressure (requires accurate sea-level reference). Soil pH is an <strong>estimated</strong> value that requires calibration – see README.</p>
  <div class="wrap">
  <table>
    <thead>
      <tr>
        <th>Uptime</th>
        <th>T &#176;C</th><th>T &#176;F</th>
        <th>Hum %</th><th>Pres hPa</th><th>Alt m &#9733;</th>
        <th>H&#8322;O raw</th><th>H&#8322;O %</th>
        <th>pH raw</th><th>pH est. &#9733;</th>
        <th>Rain raw</th><th>Rain</th>
        <th>BME&#10003;</th>
      </tr>
    </thead>
    <tbody id="tbody">
      <tr><td colspan="13" style="text-align:center;color:#94a3b8">Loading&#8230;</td></tr>
    </tbody>
  </table>
  </div>
</section>

<section>
  <div style="display:flex;gap:10px;flex-wrap:wrap">
    <a class="btn dl" href="/api/log" download="sensor_log.csv">&#11015; Download CSV Log</a>
    <a class="btn clr" href="/api/log/clear" onclick="return confirm('Clear the persistent log file?')">&#128465; Clear Log</a>
  </div>
</section>

<script>
function fmt(ms){var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);return h+'h '+(m%60)+'m '+(s%60)+'s';}
function fv(v,d){return(v===null||v===undefined)?'—':Number(v).toFixed(d!==undefined?d:1);}
function lv(v){return Number(v)===0?'LOW':'HIGH';}

function controlPump(turnOn){
  var msg=turnOn?'Turn pump ON? It will auto-off after the safety timer.':'Turn pump OFF?';
  if(!confirm(msg)) return;
  var path=turnOn?'/api/pump/on':'/api/pump/off';
  fetch(path,{method:'POST'})
    .then(function(r){return r.json();})
    .then(function(){refresh();})
    .catch(function(){document.getElementById('status').textContent='Pump '+(turnOn?'ON':'OFF')+' request failed';});
}

function refresh(){
  fetch('/api/current')
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.valid){
        document.getElementById('t_c').textContent    = d.bme_valid ? fv(d.temperature_c,1) : '—';
        document.getElementById('t_f').textContent    = d.bme_valid ? fv(d.temperature_f,1) : '—';
        document.getElementById('hum').textContent    = d.bme_valid ? fv(d.humidity,1)       : '—';
        document.getElementById('pres').textContent   = d.bme_valid ? fv(d.pressure_hpa,1)   : '—';
        document.getElementById('alt').textContent    = d.bme_valid ? fv(d.altitude_m,1)     : '—';
        document.getElementById('wl_pct').textContent = fv(d.water_level_pct,0);
        document.getElementById('ph_val').textContent = fv(d.ph_value,2);
        var rd=d.rain_detected;
        document.getElementById('rain_state').textContent = rd ? '\uD83C\uDF27 Wet' : '\u2600 Dry';
        document.getElementById('rain_raw_lbl').textContent = 'raw: '+d.rain_raw;
        document.getElementById('rain_card').className='card'+(rd?' rain-yes':'');
        var pon=d.pump_on;
        document.getElementById('pump_state').textContent = pon ? 'ON' : 'OFF';
        document.getElementById('pump_card').className='card'+(pon?' pump-on':'');
        document.getElementById('pump_diag').textContent =
          'GPIO '+d.pump_pin+' configured active='+lv(d.pump_active_level)+
          ', off='+lv(d.pump_off_level)+', current pin='+lv(d.pump_relay_level)+'.';
        document.getElementById('status').textContent='Last updated: uptime '+fmt(d.uptime_ms);
      } else {
        document.getElementById('status').textContent='Awaiting first reading…';
      }
    })
    .catch(function(){document.getElementById('status').textContent='Connection error – retrying…';});

  fetch('/api/history')
    .then(function(r){return r.json();})
    .then(function(data){
      var rows=data.readings||[];
      var tbody=document.getElementById('tbody');
      if(!rows.length){
        tbody.innerHTML='<tr><td colspan="13" style="text-align:center;color:#94a3b8">No readings yet</td></tr>';
        return;
      }
      var html='';
      for(var i=rows.length-1;i>=0;i--){
        var r=rows[i];
        var bv=r.bme_valid;
        html+='<tr>'+
          '<td>'+fmt(r.uptime_ms)+'</td>'+
          '<td>'+(bv?fv(r.temperature_c):'—')+'</td>'+
          '<td>'+(bv?fv(r.temperature_f):'—')+'</td>'+
          '<td>'+(bv?fv(r.humidity):'—')+'</td>'+
          '<td>'+(bv?fv(r.pressure_hpa):'—')+'</td>'+
          '<td>'+(bv?fv(r.altitude_m):'—')+'</td>'+
          '<td>'+r.water_level_raw+'</td>'+
          '<td>'+r.water_level_pct+'%</td>'+
          '<td>'+r.ph_raw+'</td>'+
          '<td>'+fv(r.ph_value,2)+'</td>'+
          '<td>'+r.rain_raw+'</td>'+
          '<td>'+(r.rain_detected?'\uD83C\uDF27':'\u2600')+'</td>'+
          '<td>'+(bv?'\u2713':'\u2717')+'</td>'+
          '</tr>';
      }
      tbody.innerHTML=html;
    });
}
document.getElementById('btn_pump_on').addEventListener('click',function(){controlPump(true);});
document.getElementById('btn_pump_off').addEventListener('click',function(){controlPump(false);});
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

static void handlePumpState();

static void handleCurrentJson() {
  SensorReading r = SampleStore::getLatest();

  // Format BME280 floats as "null" if NaN for valid JSON
  char tC[10], tF[10], hum[10], pHpa[10], alt[10];
  if (r.bme_valid) {
    snprintf(tC,   sizeof(tC),   "%.1f", r.temperature_c);
    snprintf(tF,   sizeof(tF),   "%.1f", r.temperature_f);
    snprintf(hum,  sizeof(hum),  "%.1f", r.humidity);
    snprintf(pHpa, sizeof(pHpa), "%.1f", r.pressure_hpa);
    snprintf(alt,  sizeof(alt),  "%.1f", r.altitude_m);
  } else {
    strcpy(tC, "null");
    strcpy(tF, "null");
    strcpy(hum, "null");
    strcpy(pHpa, "null");
    strcpy(alt, "null");
  }

  char buf[512];
  if (r.valid) {
    snprintf(buf, sizeof(buf),
             "{\"valid\":true,\"uptime_ms\":%lu,\"bme_valid\":%s,"
             "\"temperature_c\":%s,\"temperature_f\":%s,"
             "\"humidity\":%s,\"pressure_hpa\":%s,\"altitude_m\":%s,"
             "\"water_level_raw\":%d,\"water_level_pct\":%d,"
             "\"ph_raw\":%d,\"ph_value\":%.2f,"
             "\"rain_raw\":%d,\"rain_detected\":%s,"
             "\"pump_on\":%s,"
             "\"pump_pin\":%d,\"pump_relay_level\":%d,"
             "\"pump_active_level\":%d,\"pump_off_level\":%d}",
             r.uptime_ms,
             r.bme_valid ? "true" : "false",
             tC, tF, hum, pHpa, alt,
             r.water_level_raw, r.water_level_pct,
             r.ph_raw, r.ph_value,
             r.rain_raw, r.rain_detected ? "true" : "false",
             SampleStore::getPumpState() ? "true" : "false",
             PUMP_RELAY_PIN,
             SampleStore::getPumpRelayLevel(),
             SampleStore::getPumpRelayActiveLevel(),
             SampleStore::getPumpRelayOffLevel());
  } else {
    snprintf(buf, sizeof(buf),
             "{\"valid\":false,\"pump_on\":%s,"
             "\"pump_pin\":%d,\"pump_relay_level\":%d,"
             "\"pump_active_level\":%d,\"pump_off_level\":%d}",
             SampleStore::getPumpState() ? "true" : "false",
             PUMP_RELAY_PIN,
             SampleStore::getPumpRelayLevel(),
             SampleStore::getPumpRelayActiveLevel(),
             SampleStore::getPumpRelayOffLevel());
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

static void sendPumpCommandResponse(bool on) {
  SampleStore::setPump(on);
  handlePumpState();
}

static void handlePumpOn() {
  if (server.method() == HTTP_POST) {
    sendPumpCommandResponse(true);
    return;
  }
  SampleStore::setPump(true);
  server.sendHeader(F("Location"), F("/"));
  server.send(302);
}

static void handlePumpOff() {
  if (server.method() == HTTP_POST) {
    sendPumpCommandResponse(false);
    return;
  }
  SampleStore::setPump(false);
  server.sendHeader(F("Location"), F("/"));
  server.send(302);
}

static void handlePumpState() {
  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"pump_on\":%s,\"pump_pin\":%d,"
           "\"pump_relay_level\":%d,\"pump_active_level\":%d,\"pump_off_level\":%d}",
           SampleStore::getPumpState() ? "true" : "false",
           PUMP_RELAY_PIN,
           SampleStore::getPumpRelayLevel(),
           SampleStore::getPumpRelayActiveLevel(),
           SampleStore::getPumpRelayOffLevel());
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("application/json"), buf);
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
  server.on("/api/pump",                  handlePumpState);
  server.on("/api/pump/on",               handlePumpOn);
  server.on("/api/pump/on", HTTP_POST,    handlePumpOn);
  server.on("/api/pump/off",              handlePumpOff);
  server.on("/api/pump/off", HTTP_POST,   handlePumpOff);

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
