// The web page's own light/dark appearance is independent of the display's own
// "Darstellung" setting below — this is just how the page looks in your browser,
// remembered per-browser via localStorage, not written to the device at all.
function initWebUiTheme() {
  const toggle = document.getElementById("webui-theme-toggle");
  const saved = localStorage.getItem("webui-theme");
  const dark = saved ? saved === "dark" : window.matchMedia("(prefers-color-scheme: dark)").matches;
  applyWebUiTheme(dark);
  toggle.checked = dark;
  toggle.addEventListener("change", () => applyWebUiTheme(toggle.checked));
}

function applyWebUiTheme(dark) {
  document.documentElement.setAttribute("data-theme", dark ? "dark" : "light");
  localStorage.setItem("webui-theme", dark ? "dark" : "light");
}

function showToast(message) {
  const toast = document.getElementById("toast");
  toast.textContent = message;
  toast.classList.add("visible");
  setTimeout(() => toast.classList.remove("visible"), 2500);
}

function postJson(url, body) {
  return fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

// A plain fetch() has no timeout of its own — observed, for the device's own polling
// endpoints, sitting far longer than expected before rejecting once the device is no
// longer there to answer (e.g. mid-reboot), which read as the UI getting stuck on
// whatever it last showed. Every poll loop against this device uses this instead of a
// bare fetch().
function fetchWithTimeout(url, timeoutMs = 3000) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
  return fetch(url, { signal: controller.signal }).finally(() => clearTimeout(timeoutId));
}

async function loadConfig() {
  const res = await fetch("/api/config");
  const cfg = await res.json();

  document.getElementById("device-title").textContent = cfg.device_name;
  document.getElementById("ota_auto_check").value = String(cfg.ota_auto_check);
  document.getElementById("ota_auto_install").value = String(cfg.ota_auto_install);
  updateOtaAutoInstallVisibility();
  document.getElementById("mqtt_host").value = cfg.mqtt_host || "";
  document.getElementById("mqtt_port").value = cfg.mqtt_port || 1883;
  document.getElementById("mqtt_username").value = cfg.mqtt_username || "";
  document.getElementById("mqtt_use_tls").value = String(cfg.mqtt_use_tls);
  document.getElementById("chargepoint_ids").value = (cfg.chargepoint_ids || []).join(", ");
  document.getElementById("brightness_pct").value = cfg.brightness_pct;
  document.getElementById("screen_timeout_s").value = cfg.screen_timeout_s;
  document.getElementById("show_flow_chart").value = String(cfg.show_flow_chart);
  document.getElementById("dark_mode").value = String(cfg.dark_mode);
  document.getElementById("dark_mode_schedule").value = String(cfg.dark_mode_schedule);
  document.getElementById("schedule_light_start").value = toTimeInput(
    cfg.schedule_light_start_hour,
    cfg.schedule_light_start_min
  );
  document.getElementById("schedule_dark_start").value = toTimeInput(
    cfg.schedule_dark_start_hour,
    cfg.schedule_dark_start_min
  );
  updateScheduleFieldsVisibility();
  document.getElementById("pin-status").textContent = cfg.has_web_ui_pin ? "Aktiv" : "Deaktiviert";
  loadDiscoveredChargepoints();
}

function toTimeInput(hour, minute) {
  return `${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}`;
}

function updateScheduleFieldsVisibility() {
  const mode = document.getElementById("dark_mode_schedule").value;
  document.getElementById("schedule-time-fields").style.display = mode === "1" ? "block" : "none";
}

function updateOtaAutoInstallVisibility() {
  const checking = document.getElementById("ota_auto_check").value === "true";
  document.getElementById("ota-auto-install-field").style.display = checking ? "block" : "none";
}

const kMaxChargepoints = 4; // matches AppConfig::kMaxChargepoints

// Keeps the checkbox list in sync with chargepoint_ids' *current* value rather than
// overwriting it from scratch each poll — otherwise a box the user just (un)checked
// would flicker back to its pre-click state on the next 5s refresh, before their own
// change actually reaches Speichern & Neustart.
function currentChargepointIds() {
  return document
    .getElementById("chargepoint_ids")
    .value.split(",")
    .map((s) => parseInt(s.trim(), 10))
    .filter((n) => !isNaN(n));
}

function onChargepointCheckboxChanged(event) {
  const checked = Array.from(document.querySelectorAll(".chargepoint-pick input:checked")).map((el) =>
    parseInt(el.value, 10)
  );
  if (checked.length > kMaxChargepoints) {
    showToast(`Maximal ${kMaxChargepoints} Ladepunkte gleichzeitig.`);
    // Revert: uncheck whichever box just pushed the count over the limit.
    event.target.checked = false;
    return;
  }
  document.getElementById("chargepoint_ids").value = checked.join(", ");
}

let brokerDiscoveryPollTimer = null;

function stopBrokerDiscoveryPoll() {
  if (brokerDiscoveryPollTimer !== null) {
    clearInterval(brokerDiscoveryPollTimer);
    brokerDiscoveryPollTimer = null;
  }
}

function renderBrokerResults(brokers) {
  const status = document.getElementById("broker-discovery-status");
  if (brokers.length === 0) {
    status.textContent = "Keine openWB-Installation gefunden — bitte Host/IP unten manuell eintragen.";
    return;
  }
  status.innerHTML = brokers
    .map(
      (b) =>
        `<button type="button" class="secondary broker-pick" data-host="${escapeHtml(b.host)}" data-port="${b.port}">${escapeHtml(b.host)}:${b.port} verwenden</button>`
    )
    .join(" ");
  status.querySelectorAll(".broker-pick").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.getElementById("mqtt_host").value = btn.dataset.host;
      document.getElementById("mqtt_port").value = btn.dataset.port;
      showToast(`Host übernommen: ${btn.dataset.host}`);
    });
  });
}

async function pollBrokerDiscovery() {
  try {
    const res = await fetchWithTimeout("/api/mqtt/discover/progress");
    const data = await res.json();
    const status = document.getElementById("broker-discovery-status");
    if (data.running) {
      const pct = data.progress_percent >= 0 ? ` ${data.progress_percent} %` : "";
      status.textContent = `Suche läuft…${pct}`;
    } else {
      stopBrokerDiscoveryPoll();
      if (data.error) {
        status.textContent = `Suche nicht möglich: ${data.error}`;
      } else {
        renderBrokerResults(data.brokers || []);
      }
    }
  } catch {
    stopBrokerDiscoveryPoll();
    document.getElementById("broker-discovery-status").textContent =
      "Verbindung zum Display unterbrochen — Suche ggf. erneut starten.";
  }
}

async function discoverBroker() {
  document.getElementById("broker-discovery-status").textContent = "Suche gestartet…";
  await fetch("/api/mqtt/discover", { method: "POST" });
  stopBrokerDiscoveryPoll();
  brokerDiscoveryPollTimer = setInterval(pollBrokerDiscovery, 1000);
}

async function loadDiscoveredChargepoints() {
  const list = document.getElementById("discovered-chargepoints-list");
  let data;
  try {
    const res = await fetchWithTimeout("/api/chargepoints/discovered");
    data = await res.json();
  } catch {
    return; // transient — the next 5s poll tries again, same as loadStatus
  }
  const discovered = data.chargepoints || [];

  if (discovered.length === 0) {
    list.textContent = "Noch keine Ladepunkte erkannt — siehe Hinweis unten.";
    return;
  }

  const selected = new Set(currentChargepointIds());
  list.innerHTML = discovered
    .map((cp) => {
      const checked = selected.has(cp.id) ? "checked" : "";
      const name = cp.name ? escapeHtml(cp.name) : "(Name noch nicht empfangen)";
      return `<label class="chargepoint-pick">
        <input type="checkbox" value="${cp.id}" ${checked} onchange="onChargepointCheckboxChanged(event)" />
        ${name} <span class="cp-id">ID ${cp.id}</span>
      </label>`;
    })
    .join("");
}

async function loadStatus() {
  const res = await fetch("/api/status");
  const data = await res.json();

  const rows = [];
  if (data.global) {
    rows.push(rowHtml("PV-Leistung", `${Math.round(data.global.pv_power_w)} W`, "tag-pv"));
    rows.push(rowHtml("Netzleistung", `${Math.round(data.global.grid_power_w)} W`, "tag-grid"));
    if (data.global.battery_soc_percent >= 0) {
      rows.push(rowHtml("Speicher-Leistung", `${Math.round(data.global.battery_power_w)} W`, "tag-battery"));
      rows.push(rowHtml("Speicher-Ladestand", `${Math.round(data.global.battery_soc_percent)}%`, "tag-battery"));
    }
    rows.push(rowHtml("Verbraucher", `${Math.round(data.global.consumer_power_w)} W`, "tag-consumer"));
    rows.push(rowHtml("Hausverbrauch", `${Math.round(data.global.home_consumption_w)} W`, "tag-home"));
    if (data.global.meter_power_w) {
      rows.push(rowHtml("Zähler", `${Math.round(data.global.meter_power_w)} W`, "tag-grid"));
    }
  }
  for (const cp of data.chargepoints || []) {
    const label = cp.charging ? "Lädt" : cp.plugged ? "Angesteckt, wartet" : "Nicht angesteckt";
    const name = cp.name || `Ladepunkt ${cp.id}`;
    rows.push(rowHtml(name, `${label} · ${Math.round(cp.power_w)} W`, "tag-chargepoint"));
  }
  document.getElementById("status-rows").innerHTML = rows.join("") || "Noch keine Daten.";
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function rowHtml(label, value, cssClass) {
  return `<div class="status-row"><span>${escapeHtml(label)}</span><span class="value ${cssClass}">${escapeHtml(value)}</span></div>`;
}

async function loadInfo() {
  const res = await fetch("/api/info");
  const info = await res.json();
  document.getElementById("fw-version").textContent = info.firmware_version;
}

function parseTimeInput(elementId, hourKey, minKey) {
  const [hour, minute] = document.getElementById(elementId).value.split(":").map(Number);
  return { [hourKey]: hour, [minKey]: minute };
}

function readSettingsForm() {
  const chargepointIds = document
    .getElementById("chargepoint_ids")
    .value.split(",")
    .map((s) => parseInt(s.trim(), 10))
    .filter((n) => !isNaN(n))
    .slice(0, kMaxChargepoints);

  const body = {
    ota_auto_check: document.getElementById("ota_auto_check").value === "true",
    ota_auto_install: document.getElementById("ota_auto_install").value === "true",
    mqtt_host: document.getElementById("mqtt_host").value,
    mqtt_port: parseInt(document.getElementById("mqtt_port").value, 10),
    mqtt_username: document.getElementById("mqtt_username").value,
    mqtt_use_tls: document.getElementById("mqtt_use_tls").value === "true",
    chargepoint_ids: chargepointIds,
    brightness_pct: parseInt(document.getElementById("brightness_pct").value, 10),
    screen_timeout_s: parseInt(document.getElementById("screen_timeout_s").value, 10),
    show_flow_chart: document.getElementById("show_flow_chart").value === "true",
    dark_mode: document.getElementById("dark_mode").value === "true",
    dark_mode_schedule: parseInt(document.getElementById("dark_mode_schedule").value, 10),
    ...parseTimeInput("schedule_light_start", "schedule_light_start_hour", "schedule_light_start_min"),
    ...parseTimeInput("schedule_dark_start", "schedule_dark_start_hour", "schedule_dark_start_min"),
  };
  const password = document.getElementById("mqtt_password").value;
  if (password) body.mqtt_password = password;
  return body;
}

async function saveAndRestart() {
  const body = readSettingsForm();
  const res = await postJson("/api/config", body);
  if (!res.ok) {
    showToast("Einstellungen konnten nicht gespeichert werden.");
    return;
  }
  showToast("Gespeichert — Neustart...");
  try {
    await fetch("/api/restart", { method: "POST" });
  } catch (e) {
    // Expected: the device reboots right after responding.
  }
}

let lastCheckHadUpdate = false;

async function checkForUpdate() {
  const res = await fetch("/api/ota/check", { method: "POST" });
  const result = await res.json();
  const el = document.getElementById("ota-status");
  if (!result.success) {
    el.textContent = `Prüfung fehlgeschlagen: ${result.message}`;
    return;
  }
  lastCheckHadUpdate = result.update_available;
  el.textContent = result.update_available
    ? `Update verfügbar: ${result.latest_version}`
    : "Aktuell — kein Update verfügbar.";
  document.getElementById("install-update-btn").style.display = result.update_available
    ? "inline-block"
    : "none";
}

// The device hands the actual download+flash off to its own task and responds to
// /api/ota/start immediately (see ota_manager.cpp) — this polls /api/ota/progress to
// show real status instead of a static "please wait" message. The poll itself is
// expected to eventually fail with a network error once the device reboots into the
// new firmware; that's treated as success, not shown as an error.
let otaPollTimer = null;

function stopOtaPoll() {
  if (otaPollTimer !== null) {
    clearInterval(otaPollTimer);
    otaPollTimer = null;
  }
}

function startOtaPoll() {
  stopOtaPoll();
  const el = document.getElementById("ota-status");
  otaPollTimer = setInterval(async () => {
    try {
      const res = await fetchWithTimeout("/api/ota/progress");
      const progress = await res.json();
      if (progress.failure_reason) {
        el.textContent = `Fehlgeschlagen: ${progress.failure_reason}`;
        stopOtaPoll();
      } else if (progress.progress_percent >= 0 && progress.progress_percent < 100) {
        el.textContent = `Installiere… ${progress.progress_percent} %`;
      } else if (progress.progress_percent >= 100) {
        el.textContent = "Installation abgeschlossen — Neustart folgt.";
      }
    } catch {
      // The device likely just rebooted into the new firmware mid-request (or this
      // poll's own timeout fired for the same reason) — treated as success, not an
      // error, since that's the expected end of a working install.
      el.textContent = "Neustart — bitte Seite in Kürze neu laden.";
      stopOtaPoll();
    }
  }, 1000);
}

async function installUpdate() {
  if (!lastCheckHadUpdate) return;
  const res = await fetch("/api/ota/start", { method: "POST" });
  const result = await res.json();
  const el = document.getElementById("ota-status");
  if (result.started) {
    el.textContent = "Installation läuft…";
    startOtaPoll();
  } else if (result.reason === "charging_active") {
    if (confirm("Ein Ladepunkt lädt gerade. Update trotzdem installieren?")) {
      const forced = await fetch("/api/ota/start", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "force=1",
      });
      const forcedResult = await forced.json();
      if (forcedResult.started) {
        el.textContent = "Installation läuft…";
        startOtaPoll();
      } else {
        el.textContent = `Fehlgeschlagen: ${forcedResult.reason}`;
      }
    }
  } else {
    el.textContent = `Installation konnte nicht gestartet werden: ${result.reason}`;
  }
}

async function setWebUiPin() {
  const pin = document.getElementById("web_ui_pin").value;
  const res = await postJson("/api/webui/pin", { pin });
  if (res.ok) {
    showToast(pin ? "PIN gesetzt." : "PIN entfernt.");
  } else if (res.status === 400) {
    const body = await res.json().catch(() => ({}));
    showToast(body.error || "Ungültiger PIN.");
  } else {
    showToast("Fehlgeschlagen — falscher aktueller PIN?");
  }
  document.getElementById("web_ui_pin").value = "";
  loadConfig();
}

async function resetSettingsToDefaults() {
  if (!confirm("MQTT-Broker-, Ladepunkt- und Display-Einstellungen wirklich auf die Standardwerte " +
      "zurücksetzen? WLAN-Zugangsdaten und PIN bleiben erhalten. Das Display startet danach neu.")) {
    return;
  }
  const res = await postJson("/api/config/reset", {});
  if (!res.ok) {
    showToast("Zurücksetzen fehlgeschlagen.");
    return;
  }
  showToast("Zurückgesetzt — Neustart...");
  try {
    await fetch("/api/restart", { method: "POST" });
  } catch (e) {
    // Expected: the device reboots right after responding.
  }
}

async function resetWifi() {
  if (!confirm("WLAN wirklich zurücksetzen? Das Display startet danach im Einrichtungsmodus neu.")) return;
  try {
    await fetch("/api/wifi/reset", { method: "POST" });
  } catch (e) {
    // Expected: the device reboots right after responding.
  }
  showToast("WLAN wird zurückgesetzt — Display startet neu.");
}

document.getElementById("save-restart-btn").addEventListener("click", saveAndRestart);
document.getElementById("reset-defaults-btn").addEventListener("click", resetSettingsToDefaults);
document.getElementById("check-update-btn").addEventListener("click", checkForUpdate);
document.getElementById("install-update-btn").addEventListener("click", installUpdate);
document.getElementById("reset-wifi-btn").addEventListener("click", resetWifi);
document.getElementById("set-pin-btn").addEventListener("click", setWebUiPin);
document.getElementById("dark_mode_schedule").addEventListener("change", updateScheduleFieldsVisibility);
document.getElementById("ota_auto_check").addEventListener("change", updateOtaAutoInstallVisibility);
document.getElementById("discover-broker-btn").addEventListener("click", discoverBroker);

initWebUiTheme();
loadConfig();
loadStatus();
loadInfo();
setInterval(loadStatus, 5000);
setInterval(loadDiscoveredChargepoints, 5000);
