function initWebUiTheme() {
  const toggle = document.getElementById("webui-theme-toggle");
  const saved = localStorage.getItem("webui-theme");
  const dark = saved ? saved === "dark" : window.matchMedia("(prefers-color-scheme: dark)").matches;
  document.documentElement.setAttribute("data-theme", dark ? "dark" : "light");
  toggle.checked = dark;
  toggle.addEventListener("change", () => {
    document.documentElement.setAttribute("data-theme", toggle.checked ? "dark" : "light");
    localStorage.setItem("webui-theme", toggle.checked ? "dark" : "light");
  });
}

function formatBytes(bytes) {
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${bytes} B`;
}

function formatUptime(seconds) {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d > 0) return `${d} T ${h} Std.`;
  if (h > 0) return `${h} Std. ${m} Min.`;
  return `${m} Min.`;
}

async function loadInfo() {
  const res = await fetch("/api/info");
  const info = await res.json();

  document.getElementById("fw-version").textContent = info.firmware_version;
  document.getElementById("uptime").textContent = formatUptime(info.uptime_s);
  document.getElementById("chip-model").textContent = info.chip_model;
  document.getElementById("chip-temp").textContent = `${info.chip_temp_c.toFixed(1)} °C`;
  document.getElementById("heap").textContent =
    `${formatBytes(info.free_heap)} / ${formatBytes(info.heap_size)}`;
  if (info.psram_size > 0) {
    document.getElementById("psram-row").style.display = "flex";
    document.getElementById("psram").textContent =
      `${formatBytes(info.free_psram)} / ${formatBytes(info.psram_size)}`;
  }
  document.getElementById("flash-size").textContent = formatBytes(info.flash_size);
  document.getElementById("fs-usage").textContent =
    `${formatBytes(info.fs_used_bytes)} / ${formatBytes(info.fs_total_bytes)}`;
  document.getElementById("wifi-ssid").textContent = info.wifi_ssid || "-";
  document.getElementById("wifi-rssi").textContent = `${info.wifi_rssi_dbm} dBm`;
  document.getElementById("ip-address").textContent = info.ip_address;
  document.getElementById("mqtt-status").textContent = info.mqtt_connected ? "Verbunden" : "Getrennt";
}

initWebUiTheme();
loadInfo();
setInterval(loadInfo, 5000);
