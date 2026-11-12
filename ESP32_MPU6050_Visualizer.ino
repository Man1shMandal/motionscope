/*
  ESP32 + MPU6050 3D motion visualizer
  Needs only the built-in ESP32 WiFi, WebServer, and Wire headers.
*/
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const char* WIFI_NAME = "Your Wi-Fi Name";
const char* WIFI_PASSWORD = "12345678";
byte mpuAddress = 0;
WebServer server(80);
float ax, ay, az, gx, gy, gz, roll, pitch, yaw, biasX, biasY, biasZ;
unsigned long lastRead, lastDebug;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 Motion</title><style>
*{box-sizing:border-box}body{margin:0;background:#10151c;color:#eef4f7;font-family:Arial;text-align:center}.top{padding:18px;background:#17212b}h1{margin:0;font-size:20px}.live{color:#7de2a8;font-size:13px}.scene{height:410px;display:grid;place-items:center;overflow:hidden;perspective:1000px;background:radial-gradient(circle,#263b49,#10151c 70%)}.mount{width:370px;height:235px;transform:scale(1)}.rig{width:370px;height:235px;position:relative;transform-style:preserve-3d;transition:transform .07s}.breadboard{position:absolute;inset:0;border-radius:13px;background:repeating-linear-gradient(90deg,#ededeb 0 4px,#d5d5d1 4px 5px);box-shadow:inset 0 0 0 12px #fafaf7,0 14px 25px #0008}.rail{position:absolute;left:25px;right:25px;height:4px}.red{top:20px;background:#e84d4d}.blue{bottom:20px;background:#438cdb}.esp{position:absolute;left:53px;top:57px;width:148px;height:120px;border:7px solid #161f26;border-radius:7px;background:#1674a5;color:white;padding-top:44px;font:bold 15px monospace;box-shadow:0 5px 0 #0c4e71}.esp:before,.esp:after,.mpu:before,.mpu:after{content:"";position:absolute;top:5px;bottom:5px;width:7px;background:repeating-linear-gradient(#d7b05c 0 7px,transparent 7px 12px)}.esp:before,.mpu:before{left:-12px}.esp:after,.mpu:after{right:-12px}.mpu{position:absolute;right:65px;top:77px;width:94px;height:77px;border:5px solid #174b3b;border-radius:4px;background:#2cab78;color:#eafff5;padding-top:27px;font:bold 12px monospace;box-shadow:0 4px 0 #166747}.wire{position:absolute;height:4px;border-radius:8px;transform-origin:left}.w1{left:195px;top:86px;width:88px;background:#e34e50;transform:rotate(11deg)}.w2{left:195px;top:111px;width:89px;background:#edce54;transform:rotate(-7deg)}.w3{left:195px;top:137px;width:86px;background:#3a83dd;transform:rotate(8deg)}.angles{display:flex;justify-content:center;gap:18px;padding:20px}.angle{min-width:85px}.label{display:block;color:#a8bbc7;font-size:12px;letter-spacing:1px}.value{font-size:30px;font-variant-numeric:tabular-nums}@media(max-width:500px){.scene{height:320px}.mount{transform:scale(.72)}.angles{gap:4px}.angle{min-width:78px}}@media(max-width:350px){.mount{transform:scale(.62)}.scene{height:285px}.value{font-size:26px}}
</style></head><body><header class="top"><h1>ESP32 + MPU6050 Motion</h1><span id="state" class="live">Connecting…</span></header><main><section class="scene"><div class="mount"><div id="rig" class="rig"><div class="breadboard"><i class="rail red"></i><i class="rail blue"></i><div class="esp">ESP32</div><div class="mpu">MPU6050</div><i class="wire w1"></i><i class="wire w2"></i><i class="wire w3"></i></div></div></div></section><section class="angles"><div class="angle"><span class="label">ROLL</span><span id="roll" class="value">0.0°</span></div><div class="angle"><span class="label">PITCH</span><span id="pitch" class="value">0.0°</span></div><div class="angle"><span class="label">YAW</span><span id="yaw" class="value">0.0°</span></div></section></main><script>
function update(d){document.getElementById('roll').textContent=d.roll.toFixed(1)+'°';document.getElementById('pitch').textContent=d.pitch.toFixed(1)+'°';document.getElementById('yaw').textContent=d.yaw.toFixed(1)+'°';document.getElementById('rig').style.transform='rotateZ('+d.yaw+'deg) rotateX('+(-d.pitch)+'deg) rotateY('+d.roll+'deg)'}async function poll(){try{var r=await fetch('/data');update(await r.json());document.getElementById('state').textContent='● Live'}catch(e){document.getElementById('state').textContent='Reconnecting…'}}setInterval(poll,75);poll();
</script></body></html>
)HTML";

void writeMPU(byte reg, byte value) {
  Wire.beginTransmission(mpuAddress); Wire.write(reg); Wire.write(value); Wire.endTransmission();
}

bool readMPU(byte reg, byte* values, byte count) {
  Wire.beginTransmission(mpuAddress); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)mpuAddress, (int)count);
  if (Wire.available() != count) return false;
  for (byte index = 0; index < count; index++) values[index] = Wire.read();
  return true;
}

int16_t number(byte high, byte low) { return (int16_t)((high << 8) | low); }

bool findMPU() {
  Serial.println("Scanning I2C bus...");
  bool sawDevice = false;
  for (byte address = 8; address < 120; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) continue;
    sawDevice = true;
    Serial.printf("  Device found at 0x%02X", address);
    Wire.beginTransmission(address);
    Wire.write(0x75);  // MPU6050 WHO_AM_I register
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)address, 1) != 1) {
      Serial.println();
      continue;
    }
    byte whoAmI = Wire.read();
    Serial.printf("  WHO_AM_I: 0x%02X\n", whoAmI);
    if (whoAmI == 0x68 || whoAmI == 0x70 || whoAmI == 0x74) {
      mpuAddress = address;
      return true;
    }
  }
  if (!sawDevice) Serial.println("  No I2C devices detected.");
  return false;
}

bool readSensor() {
  byte data[14];
  if (!readMPU(0x3B, data, 14)) return false;
  ax = number(data[0], data[1]) / 8192.0 * 9.80665;
  ay = number(data[2], data[3]) / 8192.0 * 9.80665;
  az = number(data[4], data[5]) / 8192.0 * 9.80665;
  gx = number(data[8], data[9]) / 65.5 - biasX;
  gy = number(data[10], data[11]) / 65.5 - biasY;
  gz = number(data[12], data[13]) / 65.5 - biasZ;
  return true;
}

void calibrate() {
  Serial.println("Keep the MPU6050 still: calibrating...");
  for (int sample = 0; sample < 300; sample++) {
    digitalWrite(LED_BUILTIN, (sample / 20) % 2);
    byte data[6];
    if (readMPU(0x43, data, 6)) {
      biasX += number(data[0], data[1]) / 65.5;
      biasY += number(data[2], data[3]) / 65.5;
      biasZ += number(data[4], data[5]) / 65.5;
    }
    delay(5);
  }
  biasX /= 300; biasY /= 300; biasZ /= 300;
  digitalWrite(LED_BUILTIN, HIGH);
}

void updateMotion() {
  if (millis() - lastRead < 10) return;
  float dt = (millis() - lastRead) / 1000.0;
  lastRead = millis();
  if (!readSensor()) return;
  float accelRoll = atan2(ay, az) * 180.0 / PI;
  float accelPitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  roll = 0.98 * (roll + gx * dt) + 0.02 * accelRoll;
  pitch = 0.98 * (pitch + gy * dt) + 0.02 * accelPitch;
  yaw += gz * dt;
  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
}

void sendData() {
  char json[160];
  snprintf(json, sizeof(json), "{\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f}", roll, pitch, yaw);
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin(21, 22);
  if (!findMPU()) {
    Serial.println("No MPU6050 detected. Check SDA=21, SCL=22, 3.3V, and GND.");
    while (true) { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); delay(150); }
  }
  Serial.printf("MPU6050 found at I2C address 0x%02X\n", mpuAddress);
  writeMPU(0x6B, 0x00); writeMPU(0x1C, 0x08); writeMPU(0x1B, 0x08);
  delay(100);
  calibrate();
  lastRead = millis();
  WiFi.softAP(WIFI_NAME, WIFI_PASSWORD);
  Serial.print("Own Wi-Fi ready. Open http://"); Serial.println(WiFi.softAPIP());
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", sendData);
  server.begin();
}

void loop() {
  updateMotion();
  server.handleClient();
  if (millis() - lastDebug >= 1000) {
    lastDebug = millis();
    Serial.printf("Roll %.1f  Pitch %.1f  Yaw %.1f\n", roll, pitch, yaw);
  }
}
