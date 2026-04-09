#include <U8g2lib.h>
#include <Wire.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <LiquidCrystal_I2C.h>

const char WIFI_SSID[] = "ReplaceWithYourSSID";
const char WIFI_PASS[] = "ReplaceWithYourPassword";
const char MQTT_BROKER[] = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char MQTT_TOPIC[] = "uno-r4-topic";
const char MQTT_CLIENT_ID[] = "uno-r4-wifi-notify";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// 128x32 I2C SSD1306 OLED
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);

// Initialize the LCD driver with I2C address and geometry.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// UNO R4 WiFi (RA4M1) SRAM size: 32 KB
constexpr int TOTAL_RAM_BYTES = 32 * 1024;
constexpr uint8_t LINE_HEIGHT = 12;     // 6x10 font line height
constexpr uint8_t BASELINE_Y = 10;      // First line baseline
constexpr uint8_t TOTAL_LINES = 5;      // Number of status lines
constexpr uint16_t SCROLL_STEP_MS = 90; // Scroll step in ms
constexpr unsigned long LCD_SCROLL_MS = 350;
constexpr unsigned long LCD_ALT_MS = 2000;
constexpr unsigned long OLED_MSG_MS = 10000;
constexpr unsigned long WIFI_RETRY_MS = 15000;
constexpr unsigned long MQTT_RETRY_MS = 10000;
constexpr unsigned long LCD_STATUS_REFRESH_MS = 300;
constexpr unsigned long MQTT_POLL_MS = 30;
constexpr unsigned long NET_TASK_MS = 200;
constexpr unsigned long WIFI_STATUS_LOG_MS = 3000;
constexpr unsigned long WIFI_CONNECT_WARN_MS = 15000;
constexpr uint8_t OLED_MSG_LINE_CHARS = 20;
constexpr uint8_t OLED_MSG_MAX_LEN = 80;

String serialLine;
char lcdAlertText[17] = "NEW MESSAGE";
char lcdMsg[65] = {0};
uint8_t lcdMsgLen = 0;
uint8_t lcdMsgOffset = 0;
unsigned long lcdMsgLastScrollMs = 0;
bool lcdMsgActive = false;
unsigned long lcdAltLastMs = 0;
bool lcdAltShowIp = true;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastLcdStatusMs = 0;
unsigned long lastMqttPollMs = 0;
unsigned long lastNetTaskMs = 0;
unsigned long lastWifiStatusLogMs = 0;
unsigned long wifiConnectStartMs = 0;
unsigned long lastMqttStatusLogMs = 0;
char mqttClientId[40] = {0};
char oledMsg[OLED_MSG_MAX_LEN + 1] = {0};
uint8_t oledMsgLen = 0;
bool oledMsgActive = false;
unsigned long oledMsgUntil = 0;
uint8_t oledMsgTopLine = 0;
uint8_t oledMsgPixelOffset = 0;
unsigned long oledMsgLastScrollMs = 0;

extern "C" char *sbrk(int incr);
int freeMemory() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

void buildLine(uint8_t idx, char* out, size_t outLen, int freeMem, int usedMem, unsigned long uptime) {
  switch (idx) {
    case 0:
      snprintf(out, outLen, "Available Memory: %dkB", freeMem / 1024);
      break;
    case 1:
      snprintf(out, outLen, "Used Memory: %dkB", usedMem / 1024);
      break;
    case 2:
      snprintf(out, outLen, "Total Memory: %dkB", TOTAL_RAM_BYTES / 1024);
      break;
    case 3:
      snprintf(out, outLen, "Uptime: %lus", uptime);
      break;
    default:
      snprintf(out, outLen, "UNO R4 WiFi");
      break;
  }
}

void setLcdAlert(const char* text) {
  strncpy(lcdAlertText, text, sizeof(lcdAlertText) - 1);
  lcdAlertText[sizeof(lcdAlertText) - 1] = '\0';
  strncpy(lcdMsg, text, sizeof(lcdMsg) - 1);
  lcdMsg[sizeof(lcdMsg) - 1] = '\0';
  lcdMsgLen = strlen(lcdMsg);
  lcdMsgOffset = 0;
  lcdMsgLastScrollMs = millis();
  lcdMsgActive = (lcdMsgLen > 0);
  lcdAltShowIp = false;
  lcdAltLastMs = millis();
  updateLcdDisplay();
}

void buildIpLine(char* out, size_t outLen) {
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    snprintf(out, outLen, "IP:%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  } else {
    snprintf(out, outLen, "IP:-.-.-.-");
  }
}

void buildMsgLine(char* out, size_t outLen) {
  if (!lcdMsgActive) {
    out[0] = '\0';
    return;
  }

  if (lcdMsgLen <= 16) {
    strncpy(out, lcdMsg, outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }

  uint8_t start = lcdMsgOffset;
  uint8_t take = 16;
  if (start + take <= lcdMsgLen) {
    memcpy(out, lcdMsg + start, take);
  } else {
    uint8_t first = lcdMsgLen - start;
    memcpy(out, lcdMsg + start, first);
    memcpy(out + first, lcdMsg, take - first);
  }
  out[16] = '\0';
}

void updateLcdDisplay() {
  char line1[17];
  const char* mq = mqttClient.connected() ? "MQOK" : "MQNo";
  const char* wf = (WiFi.status() == WL_CONNECTED) ? "WiFiOK" : "WiFiNo";
  snprintf(line1, sizeof(line1), "%s | %s", mq, wf);
  line1[16] = '\0';

  char line2[17];
  if (lcdMsgActive) {
    if (lcdAltShowIp) {
      buildIpLine(line2, sizeof(line2));
    } else {
      buildMsgLine(line2, sizeof(line2));
      if (line2[0] == '\0') {
        buildIpLine(line2, sizeof(line2));
      }
    }
  } else {
    buildIpLine(line2, sizeof(line2));
  }

  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void setOledMessage(const char* text) {
  strncpy(oledMsg, text, sizeof(oledMsg) - 1);
  oledMsg[sizeof(oledMsg) - 1] = '\0';
  oledMsgLen = strlen(oledMsg);
  oledMsgActive = true;
  oledMsgUntil = millis() + OLED_MSG_MS;
  oledMsgTopLine = 0;
  oledMsgPixelOffset = 0;
  oledMsgLastScrollMs = millis();
}

uint8_t oledMsgTotalLines() {
  if (oledMsgLen == 0) {
    return 1;
  }
  return (oledMsgLen + OLED_MSG_LINE_CHARS - 1) / OLED_MSG_LINE_CHARS;
}

void getOledMsgLine(uint8_t lineIdx, char* out, size_t outLen) {
  uint16_t start = lineIdx * OLED_MSG_LINE_CHARS;
  if (start >= oledMsgLen) {
    out[0] = '\0';
    return;
  }
  uint16_t remain = oledMsgLen - start;
  uint16_t take = remain > OLED_MSG_LINE_CHARS ? OLED_MSG_LINE_CHARS : remain;
  if (take >= outLen) {
    take = outLen - 1;
  }
  memcpy(out, oledMsg + start, take);
  out[take] = '\0';
}

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWifiAttemptMs < WIFI_RETRY_MS) {
    return;
  }
  lastWifiAttemptMs = now;

  if (wifiConnectStartMs == 0) {
    wifiConnectStartMs = millis();
  }

  int status = WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (status != WL_CONNECTED && status != WL_IDLE_STATUS) {
    Serial.print("WiFi begin error: ");
    Serial.println(status);
  }
}

void connectMqttIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) {
    return;
  }
  lastMqttAttemptMs = now;

  if (mqttClientId[0] == '\0') {
    byte mac[6];
    WiFi.macAddress(mac);
    snprintf(mqttClientId, sizeof(mqttClientId), "%s-%02X%02X%02X", MQTT_CLIENT_ID, mac[3], mac[4], mac[5]);
  }
  mqttClient.setId(mqttClientId);
  if (mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
    mqttClient.subscribe(MQTT_TOPIC);
    Serial.println("MQTT connected and subscribed.");
  } else {
    Serial.print("MQTT connect failed, err=");
    Serial.println(mqttClient.connectError());
  }
}

void handleMqttMessages() {
  if (!mqttClient.connected()) {
    return;
  }

  int packetSize = mqttClient.parseMessage();
  if (packetSize <= 0) {
    return;
  }

  char msgBuf[65];
  int n = 0;
  while (mqttClient.available() && n < 64) {
    msgBuf[n++] = (char)mqttClient.read();
  }
  msgBuf[n] = '\0';

  while (mqttClient.available()) {
    mqttClient.read();
  }

  if (n == 0) {
    Serial.println("MQTT msg: <empty>");
    setLcdAlert("NEW MESSAGE");
  } else {
    Serial.print("MQTT msg: ");
    Serial.println(msgBuf);
    setLcdAlert(msgBuf);
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialLine.trim();
      if (serialLine == "NEW_MSG") {
        setLcdAlert("NEW MESSAGE");
      } else if (serialLine.startsWith("MSG ")) {
        String msg = serialLine.substring(4);
        msg.trim();
        if (msg.length() == 0) {
          setLcdAlert("NEW MESSAGE");
        } else {
          char temp[17];
          msg.substring(0, 16).toCharArray(temp, sizeof(temp));
          setLcdAlert(temp);
        }
      }
      serialLine = "";
    } else {
      serialLine += c;
      if (serialLine.length() > 64) {
        serialLine = "";
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.disconnect();

  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();                  // initialize the lcd
  lcd.backlight();             // Turn on backlight
  lcd.print("hello, world!");  // Print a message to the LCD

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  u8g2.setI2CAddress(0x3C * 2); // 0x3C << 1
  u8g2.begin();
  // u8g2.enableUTF8Print();

  // Startup self-check page
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 13);
  u8g2.print("Booting...");
  u8g2.setCursor(0, 28);
  u8g2.print("UNO R4 WiFi");
  u8g2.sendBuffer();

  delay(700);
  digitalWrite(LED_BUILTIN, LOW);

  connectWiFiIfNeeded();
}

bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}

void loop() {
  handleSerialCommands();
  unsigned long now = millis();
  if (now - lastNetTaskMs >= NET_TASK_MS) {
    lastNetTaskMs = now;
    connectWiFiIfNeeded();
    connectMqttIfNeeded();
  }

  if (now - lastMqttPollMs >= MQTT_POLL_MS) {
    lastMqttPollMs = now;
    mqttClient.poll();
  }
  handleMqttMessages();

  if (now - lastWifiStatusLogMs >= WIFI_STATUS_LOG_MS) {
    lastWifiStatusLogMs = now;
    int status = WiFi.status();
    Serial.print("WiFi status: ");
    Serial.println(status);
    if (status == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      wifiConnectStartMs = 0;
    } else if (wifiConnectStartMs != 0 && now - wifiConnectStartMs > WIFI_CONNECT_WARN_MS) {
      Serial.println("WiFi connect timeout. Check SSID/password or 2.4GHz support.");
      wifiConnectStartMs = 0;
    }
  }

  if (now - lastMqttStatusLogMs >= 5000) {
    lastMqttStatusLogMs = now;
    Serial.print("MQTT connected: ");
    Serial.println(mqttClient.connected() ? "YES" : "NO");
  }

  static uint8_t topLine = 0;
  static uint8_t pixelOffset = 0;
  static unsigned long lastScrollMs = 0;

  int freeMem = freeMemory();
  int usedMem = TOTAL_RAM_BYTES - freeMem;
  unsigned long uptime = millis() / 1000;

  if (lcdMsgActive && lcdMsgLen > 16 && now - lcdMsgLastScrollMs >= LCD_SCROLL_MS) {
    lcdMsgLastScrollMs = now;
    lcdMsgOffset = (lcdMsgOffset + 1) % lcdMsgLen;
  }
  if (lcdMsgActive && now - lcdAltLastMs >= LCD_ALT_MS) {
    lcdAltLastMs = now;
    lcdAltShowIp = !lcdAltShowIp;
  }
  if (now - lastLcdStatusMs >= LCD_STATUS_REFRESH_MS) {
    lastLcdStatusMs = now;
    updateLcdDisplay();
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  if (oledMsgActive) {
    if (now >= oledMsgUntil) {
      oledMsgActive = false;
      u8g2.clearBuffer();
    } else if (now - oledMsgLastScrollMs >= SCROLL_STEP_MS) {
      oledMsgLastScrollMs = now;
      oledMsgPixelOffset++;
      if (oledMsgPixelOffset >= LINE_HEIGHT) {
        oledMsgPixelOffset = 0;
        oledMsgTopLine = (oledMsgTopLine + 1) % oledMsgTotalLines();
      }
    }

    if (oledMsgActive) {
      for (uint8_t i = 0; i < 3; i++) {
        uint8_t lineIdx = (oledMsgTopLine + i) % oledMsgTotalLines();
        int16_t y = BASELINE_Y + i * LINE_HEIGHT - oledMsgPixelOffset;
        if (y > -2 && y < 40) {
          char lineBuf[OLED_MSG_LINE_CHARS + 1];
          getOledMsgLine(lineIdx, lineBuf, sizeof(lineBuf));
          u8g2.setCursor(0, y);
          u8g2.print(lineBuf);
        }
      }
    }
  } else {
    if (now - lastScrollMs >= SCROLL_STEP_MS) {
      lastScrollMs = now;
      pixelOffset++;
      if (pixelOffset >= LINE_HEIGHT) {
        pixelOffset = 0;
        topLine = (topLine + 1) % TOTAL_LINES;
      }
    }

    // 绘制可见区域内的3行（2行可见 + 1行用于平滑滚动衔接）
    for (uint8_t i = 0; i < 3; i++) {
      uint8_t lineIdx = (topLine + i) % TOTAL_LINES;
      int16_t y = BASELINE_Y + i * LINE_HEIGHT - pixelOffset;
      if (y > -2 && y < 40) {
        char lineBuf[32];
        buildLine(lineIdx, lineBuf, sizeof(lineBuf), freeMem, usedMem, uptime);
        u8g2.setCursor(0, y);
        u8g2.print(lineBuf);
      }
    }
  }

  u8g2.sendBuffer();
  delay(2);
}
