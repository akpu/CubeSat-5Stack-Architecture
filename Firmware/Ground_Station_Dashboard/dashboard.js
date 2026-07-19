console.log("🛰 Ground station initializing...");

// ================= MQTT TOPIC =================
const TOPIC = "ktu/kiran/balloon/telemetry";

// ================= MQTT CONNECTION =================
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt", {
  clientId: "Dashboard_" + Math.random().toString(16).substring(2, 10),
  clean: true,
  connectTimeout: 5000,
  reconnectPeriod: 3000,
  keepalive: 30,
});

client.on("connect", () => {
  console.log("✓ MQTT Connected (HiveMQ)");
  client.subscribe(TOPIC);
  console.log("✓ Subscribed to:", TOPIC);

  const statusEl = document.getElementById("mqtt-status");
  statusEl.textContent = "🟢 Connected";
  statusEl.classList.add("connected");
});

client.on("error", (err) => {
  console.error("MQTT Error:", err.message);

  const statusEl = document.getElementById("mqtt-status");
  statusEl.textContent = "🔴 Connection Error";
  statusEl.classList.remove("connected");
});

client.on("reconnect", () => {
  console.log("Reconnecting...");

  const statusEl = document.getElementById("mqtt-status");
  statusEl.textContent = "🟡 Reconnecting...";
  statusEl.classList.remove("connected");
});

client.on("offline", () => {
  console.log("MQTT Offline");

  const statusEl = document.getElementById("mqtt-status");
  statusEl.textContent = "🔴 Offline";
  statusEl.classList.remove("connected");
});

// ================= MAP INITIALIZATION =================
const map = L.map("map");

L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
  maxZoom: 19,
  attribution: "© OpenStreetMap contributors",
}).addTo(map);

// Default empty world view
map.setView([0, 0], 2);

// ================= CUSTOM MARKER =================
const customIcon = L.divIcon({
  html: `
    <div style="
      background: linear-gradient(135deg, #00d9ff, #9d4edd);
      width: 30px;
      height: 30px;
      border-radius: 50%;
      border: 3px solid white;
      box-shadow: 0 0 12px rgba(0, 217, 255, 0.6);
      display: flex;
      align-items: center;
      justify-content: center;
    ">
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2">
        <circle cx="12" cy="12" r="10"></circle>
        <path d="M12 2v10M12 12v10"></path>
      </svg>
    </div>
  `,
  iconSize: [30, 30],
  className: "custom-marker",
});

// Marker created only after valid GPS received
let marker = null;

// ================= DOM ELEMENTS =================
const GPS = {
  lat: document.getElementById("lat"),
  lng: document.getElementById("lng"),
  alt: document.getElementById("gpsAlt"),
  speed: document.getElementById("speed"),
  sats: document.getElementById("sats"),
};

const IMU = {
  accel: {
    x: document.getElementById("ax"),
    y: document.getElementById("ay"),
    z: document.getElementById("az"),
  },
  gyro: {
    x: document.getElementById("gx"),
    y: document.getElementById("gy"),
    z: document.getElementById("gz"),
  },
  mag: {
    x: document.getElementById("mx"),
    y: document.getElementById("my"),
    z: document.getElementById("mz"),
  },
};

const ENV = {
  temp: document.getElementById("temp"),
  pressure: document.getElementById("pressure"),
  altitude: document.getElementById("altitude"),
};

const ORIENTATION = {
  rollValue: document.getElementById("roll-value"),
  pitchValue: document.getElementById("pitch-value"),
  yawValue: document.getElementById("yaw-value"),
  yaw2: document.getElementById("yaw2"),
};

const BATTERY = {
  voltage: document.getElementById("voltage"),
  current: document.getElementById("current"),
  power: document.getElementById("power"),
  voltageBar: document.getElementById("voltage-bar"),
  currentBar: document.getElementById("current-bar"),
  voltageStatus: document.getElementById("voltage-status"),
};

const cube = document.getElementById("cube");
const needle = document.getElementById("needle");

// ================= ORIENTATION =================
let currentRoll = 0;
let currentPitch = 0;
let currentYaw = 0;

// ================= UTILITY FUNCTIONS =================
function formatNumber(value, decimals = 2) {
  return Number(value).toFixed(decimals);
}

function updateValue(element, value, decimals = 2) {
  const formatted = formatNumber(value, decimals);
  const current = element.innerText;

  if (current !== formatted) {
    element.innerText = formatted;

    element.style.transition = "all 0.3s ease";
    element.style.opacity = "0.6";

    setTimeout(() => {
      element.style.opacity = "1";
    }, 50);
  }
}

function animateValue(element, value, decimals = 2) {
  updateValue(element, value, decimals);
}

// ================= CUBE ROTATION =================
function updateCubeRotation(roll, pitch, yaw) {
  currentRoll = roll;
  currentPitch = pitch;
  currentYaw = yaw;

  cube.style.transform = `
    rotateX(${pitch}deg)
    rotateY(${yaw}deg)
    rotateZ(${roll}deg)
  `;

  ORIENTATION.rollValue.textContent = roll.toFixed(1);
  ORIENTATION.pitchValue.textContent = pitch.toFixed(1);
  ORIENTATION.yawValue.textContent = yaw.toFixed(1);
}

// ================= POWER UPDATE =================
function updatePowerSupply(voltage, current) {
  BATTERY.voltage.textContent = voltage.toFixed(2);
  BATTERY.current.textContent = current.toFixed(2);

  // POWER = V × I
  const power = voltage * current;
  BATTERY.power.textContent = power.toFixed(2);

  // Voltage Bar
  const voltageMin = 0.0;
  const voltageMax = 30.0;

  const voltagePercent = Math.max(
    0,
    Math.min(
      100,
      ((voltage - voltageMin) / (voltageMax - voltageMin)) * 100
    )
  );

  BATTERY.voltageBar.style.width = voltagePercent + "%";

  // Current Bar
  const currentPercent = (current / 2000) * 100;
  BATTERY.currentBar.style.width = Math.min(currentPercent, 100) + "%";

  // Status
  if (voltage >= 20 && voltage <= 30) {
    BATTERY.voltageStatus.textContent = "☀ Optimal";
    BATTERY.voltageStatus.style.color = "#22c55e";
  } else if (voltage >= 15 && voltage < 20) {
    BATTERY.voltageStatus.textContent = "⛅ Good";
    BATTERY.voltageStatus.style.color = "#fbbf24";
  } else if (voltage >= 5 && voltage < 15) {
    BATTERY.voltageStatus.textContent = "🌙 Low Light";
    BATTERY.voltageStatus.style.color = "#fbbf24";
  } else if (voltage > 0 && voltage < 5) {
    BATTERY.voltageStatus.textContent = "🌑 Night";
    BATTERY.voltageStatus.style.color = "#ef4444";
  } else {
    BATTERY.voltageStatus.textContent = "-- No Data";
    BATTERY.voltageStatus.style.color = "#9ca3af";
  }
}

// ================= DATA RECEIVER =================
client.on("message", (topic, message) => {
  try {
    const data = JSON.parse(message.toString());

    // ================= GPS =================
    const lat = data.lat ?? 0;
    const lng = data.lng ?? 0;
    const gpsAlt = data.galt ?? 0;
    const sats = data.sat ?? 0;
    const speed = data.speed ?? 0;

    // ================= ACCELEROMETER =================
    const ax = data.ax ?? 0;
    const ay = data.ay ?? 0;
    const az = data.az ?? 0;

    // ================= GYROSCOPE =================
    const gx = data.gx ?? 0;
    const gy = data.gy ?? 0;
    const gz = data.gz ?? 0;

    // ================= MAGNETOMETER =================
    const mx = data.mx ?? 0;
    const my = data.my ?? 0;
    const mz = data.mz ?? 0;

    // ================= ENVIRONMENT =================
    const temp = data.t ?? 0;
    const pressure = data.p ?? 0;
    const altitude = data.balt ?? 0;

    // ================= POWER =================
    const voltage = data.v ?? 0;
    const current = data.c ?? 0;

    // ================= ORIENTATION =================
    const roll = data.r ?? 0;
    const pitch = data.pi ?? 0;
    const yaw = data.y ?? 0;

    // ================= UPDATE GPS =================
    animateValue(GPS.lat, lat, 6);
    animateValue(GPS.lng, lng, 6);
    animateValue(GPS.alt, gpsAlt, 2);
    animateValue(GPS.speed, speed, 2);

    GPS.sats.innerText = sats || "--";

    // ================= UPDATE ACCEL =================
    animateValue(IMU.accel.x, ax, 2);
    animateValue(IMU.accel.y, ay, 2);
    animateValue(IMU.accel.z, az, 2);

    // ================= UPDATE GYRO =================
    animateValue(IMU.gyro.x, gx, 2);
    animateValue(IMU.gyro.y, gy, 2);
    animateValue(IMU.gyro.z, gz, 2);

    // ================= UPDATE MAG =================
    animateValue(IMU.mag.x, mx, 2);
    animateValue(IMU.mag.y, my, 2);
    animateValue(IMU.mag.z, mz, 2);

    // ================= UPDATE ENVIRONMENT =================
    animateValue(ENV.temp, temp, 2);
    animateValue(ENV.pressure, pressure, 2);
    animateValue(ENV.altitude, altitude, 2);

    // ================= UPDATE ORIENTATION =================
    updateCubeRotation(roll, pitch, yaw);

    animateValue(ORIENTATION.yaw2, yaw, 1);

    // ================= UPDATE POWER =================
    updatePowerSupply(voltage, current);

    // ================= COMPASS =================
    needle.style.transform = `rotate(${yaw}deg)`;

    // ================= UPDATE MAP =================
    if (lat !== 0 && lng !== 0) {

      if (!marker) {
        marker = L.marker([lat, lng], {
          icon: customIcon,
        }).addTo(map);
      }

      marker.setLatLng([lat, lng]);
      map.setView([lat, lng], 18);
    }

    console.log("✓ Data updated at", new Date().toLocaleTimeString());
    console.log("📡 RSSI:", data.rssi || "N/A", "SNR:", data.snr || "N/A");

  } catch (error) {
    console.error("✗ Error processing message:", error);
  }
});

// ================= INITIALIZATION =================
console.log("✓ Dashboard ready");