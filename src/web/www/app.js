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

async function loadConfig() {
  const res = await fetch("/api/config");
  const cfg = await res.json();

  document.getElementById("device-title").textContent = cfg.device_name;
  document.getElementById("mqtt_host").value = cfg.mqtt_host || "";
  document.getElementById("mqtt_port").value = cfg.mqtt_port || 1883;
  document.getElementById("mqtt_username").value = cfg.mqtt_username || "";
  document.getElementById("mqtt_use_tls").value = String(cfg.mqtt_use_tls);
  document.getElementById("chargepoint_ids").value = (cfg.chargepoint_ids || []).join(", ");
  document.getElementById("brightness_pct").value = cfg.brightness_pct;
  document.getElementById("screen_timeout_s").value = cfg.screen_timeout_s;
  document.getElementById("show_flow_chart").value = String(cfg.show_flow_chart);
  document.getElementById("dark_mode").value = String(cfg.dark_mode);
  document.getElementById("pin-status").textContent = cfg.has_web_ui_pin ? "Aktiv" : "Deaktiviert";
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

function rowHtml(label, value, cssClass) {
  return `<div class="status-row"><span>${label}</span><span class="value ${cssClass}">${value}</span></div>`;
}

async function loadInfo() {
  const res = await fetch("/api/info");
  const info = await res.json();
  document.getElementById("fw-version").textContent = info.firmware_version;
}

function readSettingsForm() {
  const chargepointIds = document
    .getElementById("chargepoint_ids")
    .value.split(",")
    .map((s) => parseInt(s.trim(), 10))
    .filter((n) => !isNaN(n))
    .slice(0, 4);

  const body = {
    mqtt_host: document.getElementById("mqtt_host").value,
    mqtt_port: parseInt(document.getElementById("mqtt_port").value, 10),
    mqtt_username: document.getElementById("mqtt_username").value,
    mqtt_use_tls: document.getElementById("mqtt_use_tls").value === "true",
    chargepoint_ids: chargepointIds,
    brightness_pct: parseInt(document.getElementById("brightness_pct").value, 10),
    screen_timeout_s: parseInt(document.getElementById("screen_timeout_s").value, 10),
    show_flow_chart: document.getElementById("show_flow_chart").value === "true",
    dark_mode: document.getElementById("dark_mode").value === "true",
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

async function installUpdate() {
  if (!lastCheckHadUpdate) return;
  const res = await fetch("/api/ota/start", { method: "POST" });
  const result = await res.json();
  const el = document.getElementById("ota-status");
  if (result.started) {
    el.textContent = "Installation läuft — das Display startet in Kürze neu.";
  } else if (result.reason === "charging_active") {
    if (confirm("Ein Ladepunkt lädt gerade. Update trotzdem installieren?")) {
      const forced = await fetch("/api/ota/start", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "force=1",
      });
      const forcedResult = await forced.json();
      el.textContent = forcedResult.started
        ? "Installation läuft…"
        : `Fehlgeschlagen: ${forcedResult.reason}`;
    }
  } else {
    el.textContent = `Installation konnte nicht gestartet werden: ${result.reason}`;
  }
}

async function setWebUiPin() {
  const pin = document.getElementById("web_ui_pin").value;
  const res = await postJson("/api/webui/pin", { pin });
  showToast(res.ok ? (pin ? "PIN gesetzt." : "PIN entfernt.") : "Fehlgeschlagen — falscher aktueller PIN?");
  document.getElementById("web_ui_pin").value = "";
  loadConfig();
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
document.getElementById("check-update-btn").addEventListener("click", checkForUpdate);
document.getElementById("install-update-btn").addEventListener("click", installUpdate);
document.getElementById("reset-wifi-btn").addEventListener("click", resetWifi);
document.getElementById("set-pin-btn").addEventListener("click", setWebUiPin);

initWebUiTheme();
loadConfig();
loadStatus();
loadInfo();
setInterval(loadStatus, 5000);
