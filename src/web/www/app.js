// Minimal vanilla-JS settings UI — intentionally no build step/framework, this is a
// handful of forms talking to /api/* (api_routes.cpp), not an app that benefits from one.

function showToast(message) {
  const toast = document.getElementById("toast");
  toast.textContent = message;
  toast.classList.add("visible");
  setTimeout(() => toast.classList.remove("visible"), 2500);
}

async function loadConfig() {
  const res = await fetch("/api/config");
  const cfg = await res.json();

  document.getElementById("device-title").textContent = cfg.device_name;
  document.getElementById("mqtt_host").value = cfg.mqtt_host || "";
  document.getElementById("mqtt_port").value = cfg.mqtt_port || 1883;
  document.getElementById("mqtt_username").value = cfg.mqtt_username || "";
  document.getElementById("mqtt_use_tls").value = String(cfg.mqtt_use_tls);
  document.getElementById("brightness_pct").value = cfg.brightness_pct;
  document.getElementById("screen_timeout_s").value = cfg.screen_timeout_s;
  document.getElementById("dark_mode").value = String(cfg.dark_mode);
  document.documentElement.setAttribute("data-theme", cfg.dark_mode ? "dark" : "light");
}

async function loadStatus() {
  const res = await fetch("/api/status");
  const data = await res.json();

  const rows = [];
  if (data.global) {
    rows.push(rowHtml("PV-Leistung", `${Math.round(data.global.pv_power_w)} W`, "tag-pv"));
    rows.push(rowHtml("Netzleistung", `${Math.round(data.global.grid_power_w)} W`, "tag-grid"));
    if (data.global.battery_soc_percent >= 0) {
      rows.push(rowHtml("Speicher-Ladestand", `${Math.round(data.global.battery_soc_percent)}%`, "tag-battery"));
    }
    rows.push(rowHtml("Verbraucher", `${Math.round(data.global.consumer_power_w)} W`, "tag-consumer"));
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

async function saveSettings() {
  const body = {
    mqtt_host: document.getElementById("mqtt_host").value,
    mqtt_port: parseInt(document.getElementById("mqtt_port").value, 10),
    mqtt_username: document.getElementById("mqtt_username").value,
    mqtt_use_tls: document.getElementById("mqtt_use_tls").value === "true",
    brightness_pct: parseInt(document.getElementById("brightness_pct").value, 10),
    screen_timeout_s: parseInt(document.getElementById("screen_timeout_s").value, 10),
    dark_mode: document.getElementById("dark_mode").value === "true",
  };
  const password = document.getElementById("mqtt_password").value;
  if (password) body.mqtt_password = password;

  const res = await fetch("/api/config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  showToast(res.ok ? "Gespeichert." : "Einstellungen konnten nicht gespeichert werden.");
  document.documentElement.setAttribute("data-theme", body.dark_mode ? "dark" : "light");
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

async function resetWifi() {
  if (!confirm("WLAN wirklich zurücksetzen? Das Display startet danach im Einrichtungsmodus neu.")) return;
  try {
    await fetch("/api/wifi/reset", { method: "POST" });
  } catch (e) {
    // The device reboots right after responding, so the connection dropping here is
    // expected, not a real failure.
  }
  showToast("WLAN wird zurückgesetzt — Display startet neu.");
}

document.getElementById("save-btn").addEventListener("click", saveSettings);
document.getElementById("check-update-btn").addEventListener("click", checkForUpdate);
document.getElementById("install-update-btn").addEventListener("click", installUpdate);
document.getElementById("reset-wifi-btn").addEventListener("click", resetWifi);

loadConfig();
loadStatus();
loadInfo();
setInterval(loadStatus, 5000);
