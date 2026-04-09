# ArduinoSample

## Hardware
- MCU: Arduino UNO R4 WiFi
- Small display: 128x32 OLED (SSD1306, I2C, 0x3C)
- Large display: 1602/2004 I2C character LCD (0x27, fallback 0x3F)
- I2C bus is shared by both displays

## Wiring (I2C)
```
UNO R4 WiFi     OLED SSD1306     I2C LCD 1602
--------------------------------------------
3.3V or 5V  --> VCC             VCC
GND         --> GND             GND
SDA         --> SDA             SDA
SCL         --> SCL             SCL
```
Notes:
- Both displays share the same I2C bus
- OLED usually supports 3.3V/5V, LCD 1602 is typically 5V

## Libraries
- U8g2lib (OLED)
- Wire (I2C)
- WiFiS3 (UNO R4 WiFi)
- ArduinoMqttClient (MQTT)
- LiquidCrystal_I2C (character LCD)

## Wi-Fi / MQTT
- 2.4GHz only
- Broker: broker.hivemq.com:1883
- Topic: andy/uno-r4/notify
- Client ID: uno-r4-wifi-notify-<last 3 bytes of MAC>

## Program Logic
### OLED (small display)
- Uses U8g2 to render scrolling system info
- Shows memory/uptime status

### LCD (large display)
- Line 1 shows connection status:
  - MQOK or MQNo
  - WiFiOK or WiFiNo
  - Format: "MQOK | WiFiOK"
- Line 2 alternates:
  - IP address when Wi-Fi is up
  - Latest message (if received)
  - If no Wi-Fi: "IP:-.-.-.-"

### MQTT
- Incoming MQTT message:
  - Displayed on the large LCD (scrolls if longer than 16 chars)
  - OLED keeps showing system status

## Serial
- Baud rate: 115200
- Used for logs and debugging

## Scripts
### notify_board.sh (serial notify)
- Send notification over serial
- Usage:
  - ./notify_board.sh
  - ./notify_board.sh "HELLO"
  - ./notify_board.sh --list

### notify_board_mqtt.sh (MQTT notify)
- Send notification over MQTT
- Dependency: mosquitto
- Install: brew install mosquitto
- Usage:
  - ./notify_board_mqtt.sh "HELLO MQTT"

## Key Config
- Wi-Fi SSID/password: ArduinoSample.ino
- MQTT broker/topic: ArduinoSample.ino
