const canvas = document.querySelector("#radarCanvas");
const ctx = canvas.getContext("2d");

const els = {
  connectButton: document.querySelector("#connectButton"),
  startButton: document.querySelector("#startButton"),
  stopButton: document.querySelector("#stopButton"),
  demoButton: document.querySelector("#demoButton"),
  connectionState: document.querySelector("#connectionState"),
  radarMode: document.querySelector("#radarMode"),
  heartbeat: document.querySelector("#heartbeat"),
  panValue: document.querySelector("#panValue"),
  tiltValue: document.querySelector("#tiltValue"),
  distanceValue: document.querySelector("#distanceValue"),
  targetValue: document.querySelector("#targetValue"),
  sdValue: document.querySelector("#sdValue"),
  mpuValue: document.querySelector("#mpuValue"),
  accelValue: document.querySelector("#accelValue"),
  gyroValue: document.querySelector("#gyroValue"),
  vccValue: document.querySelector("#vccValue"),
  eventLog: document.querySelector("#eventLog"),
};

const radar = {
  maxDistanceCm: 150,
  minPan: 20,
  maxPan: 160,
  lastPacket: null,
  points: [],
  connected: false,
  demo: false,
  port: null,
  reader: null,
  writer: null,
  readBuffer: "",
  lastHitLogMs: 0,
  lastInvalidLogMs: 0,
};

function hasField(object, key) {
  return Object.prototype.hasOwnProperty.call(object, key);
}

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, Math.floor(rect.width * dpr));
  canvas.height = Math.max(1, Math.floor(rect.height * dpr));
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

function logEvent(text, kind = "") {
  const li = document.createElement("li");
  li.className = kind;
  li.textContent = `${new Date().toLocaleTimeString()}  ${text}`;
  els.eventLog.prepend(li);

  while (els.eventLog.children.length > 10) {
    els.eventLog.lastElementChild.remove();
  }
}

function setConnectionState(connected, text) {
  radar.connected = connected;
  els.connectionState.textContent = text;
  els.connectionState.className = connected ? "value state-on" : "value state-off";
  els.connectButton.textContent = connected ? "Bagli" : "Baglan";
}

function angleToPoint(panDeg, distanceCm, geom) {
  const clampedPan = Math.min(radar.maxPan, Math.max(radar.minPan, panDeg));
  const ratio = (clampedPan - radar.minPan) / (radar.maxPan - radar.minPan);
  const sweepDeg = -70 + ratio * 140;
  const theta = (sweepDeg * Math.PI) / 180;
  const range = Math.min(radar.maxDistanceCm, Math.max(0, distanceCm));
  const radius = (range / radar.maxDistanceCm) * geom.radius;

  return {
    x: geom.cx + Math.sin(theta) * radius,
    y: geom.cy - Math.cos(theta) * radius,
    theta,
    radius,
  };
}

function drawGrid(geom) {
  ctx.save();
  ctx.lineWidth = 1;
  ctx.strokeStyle = "rgba(57, 255, 138, 0.22)";
  ctx.fillStyle = "rgba(57, 255, 138, 0.82)";
  ctx.font = "12px Consolas, monospace";

  for (let ring = 30; ring <= radar.maxDistanceCm; ring += 30) {
    const r = (ring / radar.maxDistanceCm) * geom.radius;
    ctx.beginPath();
    ctx.arc(geom.cx, geom.cy, r, Math.PI * 1.11, Math.PI * 1.89);
    ctx.stroke();
    ctx.fillText(`${ring}cm`, geom.cx + 8, geom.cy - r - 4);
  }

  for (let deg = -70; deg <= 70; deg += 20) {
    const theta = (deg * Math.PI) / 180;
    const x = geom.cx + Math.sin(theta) * geom.radius;
    const y = geom.cy - Math.cos(theta) * geom.radius;
    ctx.beginPath();
    ctx.moveTo(geom.cx, geom.cy);
    ctx.lineTo(x, y);
    ctx.stroke();
  }

  ctx.strokeStyle = "rgba(79, 214, 255, 0.5)";
  ctx.beginPath();
  ctx.moveTo(geom.cx - geom.radius, geom.cy);
  ctx.lineTo(geom.cx + geom.radius, geom.cy);
  ctx.stroke();
  ctx.restore();
}

function drawSweep(geom) {
  const packet = radar.lastPacket;
  if (!packet) return;

  const tip = angleToPoint(packet.pan, radar.maxDistanceCm, geom);
  ctx.save();
  const gradient = ctx.createLinearGradient(geom.cx, geom.cy, tip.x, tip.y);
  gradient.addColorStop(0, "rgba(57, 255, 138, 0)");
  gradient.addColorStop(1, "rgba(57, 255, 138, 0.78)");
  ctx.strokeStyle = gradient;
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.moveTo(geom.cx, geom.cy);
  ctx.lineTo(tip.x, tip.y);
  ctx.stroke();
  ctx.restore();
}

function drawPoints(geom) {
  const now = performance.now();
  radar.points = radar.points.filter((point) => now - point.time < 6500);

  for (const point of radar.points) {
    const age = (now - point.time) / 6500;
    const alpha = Math.max(0.08, 1 - age);
    const pos = angleToPoint(point.pan, point.distance_cm, geom);
    ctx.save();
    ctx.fillStyle = point.target
      ? `rgba(255, 93, 93, ${alpha})`
      : `rgba(57, 255, 138, ${alpha * 0.55})`;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, point.target ? 7 : 3.5, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }
}

function drawHud(geom) {
  ctx.save();
  ctx.fillStyle = "rgba(238, 246, 239, 0.86)";
  ctx.font = "13px Consolas, monospace";
  ctx.fillText("USB SERIAL TELEMETRY / 115200", geom.cx - 118, geom.cy + 34);
  ctx.restore();
}

function render() {
  const rect = canvas.getBoundingClientRect();
  ctx.clearRect(0, 0, rect.width, rect.height);

  const geom = {
    cx: rect.width / 2,
    cy: rect.height * 0.9,
    radius: Math.min(rect.width * 0.47, rect.height * 0.78),
  };

  drawGrid(geom);
  drawPoints(geom);
  drawSweep(geom);
  drawHud(geom);

  requestAnimationFrame(render);
}

function applyPacket(packet) {
  if (packet.type === "boot") {
    logEvent(`Boot: ${packet.name || "Arduino"}`);
    return;
  }

  if (packet.type !== "scan") return;

  radar.lastPacket = packet;
  els.heartbeat.classList.toggle("live", packet.enabled);
  els.radarMode.textContent = packet.enabled ? "Tarama" : "Kapali";
  els.panValue.textContent = `${packet.pan} deg`;
  els.tiltValue.textContent = `${packet.tilt} deg`;
  els.distanceValue.textContent = packet.distance_cm == null ? "-- cm" : `${packet.distance_cm.toFixed(1)} cm`;
  els.targetValue.textContent = packet.target ? "Var" : "Yok";
  els.sdValue.textContent = hasField(packet, "sd") ? (packet.sd ? "Kayit" : "Yok") : "Eski kod";
  els.mpuValue.textContent = hasField(packet, "mpu") ? (packet.mpu ? "Aktif" : "Yok") : "Eski kod";

  if (hasField(packet, "vcc_mv")) {
    const vcc = packet.vcc_mv / 1000;
    els.vccValue.textContent = `${vcc.toFixed(2)} V`;
    els.vccValue.style.color = packet.vcc_mv < 4650 ? "var(--red)" : "var(--text)";
  } else {
    els.vccValue.textContent = "Eski kod";
  }

  if (packet.imu) {
    const axG = packet.imu.ax / 16384;
    const ayG = packet.imu.ay / 16384;
    const azG = packet.imu.az / 16384;
    const gxDps = packet.imu.gx / 131;
    const gyDps = packet.imu.gy / 131;
    const gzDps = packet.imu.gz / 131;
    const accelMag = Math.sqrt(axG * axG + ayG * ayG + azG * azG);
    const gyroMag = Math.sqrt(gxDps * gxDps + gyDps * gyDps + gzDps * gzDps);

    els.accelValue.textContent = `${accelMag.toFixed(2)} g`;
    els.gyroValue.textContent = `${gyroMag.toFixed(1)} dps`;
  } else {
    els.accelValue.textContent = "Eski kod";
    els.gyroValue.textContent = "Eski kod";
  }

  if (packet.distance_cm != null) {
    radar.points.push({
      pan: packet.pan,
      distance_cm: packet.distance_cm,
      target: packet.target,
      time: performance.now(),
    });
  }

  if (packet.target && packet.distance_cm != null && performance.now() - radar.lastHitLogMs > 1200) {
    logEvent(`Hedef: pan ${packet.pan} deg, ${packet.distance_cm.toFixed(1)} cm`, "hit");
    radar.lastHitLogMs = performance.now();
  }
}

function parseLine(line) {
  const trimmed = line.trim();
  if (!trimmed) return;

  // Arduino reset atarsa veya seri akista yarim satir gelirse JSON eksik kalir.
  // Eksik paketleri sessizce atiyoruz; bir sonraki tam paket arayuzu gunceller.
  if (trimmed.startsWith("{") && !trimmed.endsWith("}")) {
    if (performance.now() - radar.lastInvalidLogMs > 3000) {
      logEvent("Eksik seri paket atlandi", "warn");
      radar.lastInvalidLogMs = performance.now();
    }
    return;
  }

  try {
    applyPacket(JSON.parse(trimmed));
  } catch (error) {
    if (performance.now() - radar.lastInvalidLogMs > 3000) {
      logEvent(`Gecersiz veri: ${trimmed.slice(0, 48)}`, "warn");
      radar.lastInvalidLogMs = performance.now();
    }
  }
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    logEvent("Web Serial desteklenmiyor. Chrome veya Edge kullan.", "warn");
    return;
  }

  try {
    radar.port = await navigator.serial.requestPort();
    await radar.port.open({ baudRate: 115200 });
    radar.writer = radar.port.writable.getWriter();
    setConnectionState(true, "Bagli");
    logEvent("Seri port acildi");
    readSerialLoop();
    await sendCommand("STATUS");
  } catch (error) {
    setConnectionState(false, "Kapali");
    logEvent(`Baglanti hatasi: ${error.message}`, "warn");
  }
}

async function readSerialLoop() {
  const decoder = new TextDecoder();
  radar.reader = radar.port.readable.getReader();

  try {
    while (true) {
      const { value, done } = await radar.reader.read();
      if (done) break;

      radar.readBuffer += decoder.decode(value, { stream: true });
      const lines = radar.readBuffer.split(/\r?\n/);
      radar.readBuffer = lines.pop() || "";
      lines.forEach(parseLine);
    }
  } catch (error) {
    logEvent(`Okuma durdu: ${error.message}`, "warn");
  } finally {
    setConnectionState(false, "Kapali");
  }
}

async function sendCommand(command) {
  if (radar.demo) {
    radar.demoEnabled = command !== "STOP";
    logEvent(`Komut: ${command}`);
    return;
  }

  if (!radar.writer) {
    logEvent("Seri port bagli degil", "warn");
    return;
  }

  const encoder = new TextEncoder();
  await radar.writer.write(encoder.encode(`${command}\n`));
  logEvent(`Komut: ${command}`);
}

function startDemo() {
  radar.demo = !radar.demo;
  radar.demoEnabled = true;
  els.demoButton.textContent = radar.demo ? "Simulasyon Acik" : "Simulasyon";

  if (!radar.demo) return;

  setConnectionState(true, "Demo");
  let pan = radar.minPan;
  let direction = 1;
  let tilt = 90;

  const timer = setInterval(() => {
    if (!radar.demo) {
      clearInterval(timer);
      setConnectionState(false, "Kapali");
      return;
    }

    if (radar.demoEnabled) {
      pan += direction;
      if (pan >= radar.maxPan || pan <= radar.minPan) direction *= -1;
      tilt = 90 + Math.round(Math.sin(Date.now() / 1200) * 6);
    }

    const target = Math.random() > 0.88;
    applyPacket({
      type: "scan",
      enabled: radar.demoEnabled,
      pan,
      tilt,
      target,
      distance_cm: target ? 45 + Math.random() * 50 : 80 + Math.random() * 65,
      threshold_cm: 120,
      vcc_mv: 4970 + Math.round(40 * Math.sin(Date.now() / 1300)),
      sd: true,
      mpu: true,
      imu: {
        ax: Math.round(1000 * Math.sin(Date.now() / 700)),
        ay: Math.round(900 * Math.cos(Date.now() / 900)),
        az: 16384 + Math.round(500 * Math.sin(Date.now() / 1100)),
        gx: Math.round(100 * Math.sin(Date.now() / 800)),
        gy: Math.round(80 * Math.cos(Date.now() / 1000)),
        gz: Math.round(60 * Math.sin(Date.now() / 1200)),
      },
      uptime_ms: Date.now(),
    });
  }, 90);
}

els.connectButton.addEventListener("click", connectSerial);
els.startButton.addEventListener("click", () => sendCommand("START"));
els.stopButton.addEventListener("click", () => sendCommand("STOP"));
els.demoButton.addEventListener("click", startDemo);

window.addEventListener("resize", resizeCanvas);
resizeCanvas();
render();
logEvent("Panel hazir");
