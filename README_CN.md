# ArduinoSample

## 硬件配置
- 主控: Arduino UNO R4 WiFi
- 小屏: 128x32 OLED (SSD1306, I2C, 0x3C)
- 大屏: 1602/2004 字符 LCD (I2C, 0x27, 失败时改 0x3F)
- I2C: SDA/SCL 共享

## 接线图 (I2C)

```
UNO R4 WiFi     OLED SSD1306     I2C LCD 1602
--------------------------------------------
3.3V or 5V  --> VCC             VCC
GND         --> GND             GND
SDA         --> SDA             SDA
SCL         --> SCL             SCL
```

说明:
- 两块屏共用同一条 I2C 总线
- OLED 一般支持 3.3V/5V, LCD 1602 通常是 5V

## 软件与库
- U8g2lib (OLED)
- Wire (I2C)
- WiFiS3 (UNO R4 WiFi)
- ArduinoMqttClient (MQTT)
- LiquidCrystal_I2C (字符 LCD)

## Wi-Fi / MQTT
- 仅支持 2.4GHz
- MQTT Broker: broker.hivemq.com:1883
- Topic: uno-r4-topic
- Client ID: uno-r4-wifi-notify-<MAC后3字节>

## 程序逻辑
### OLED (小屏)
- 使用 U8G2 绘制滚动信息
- 显示系统状态信息(内存/运行时间等)滚动

### LCD (大屏)
- 第 1 行固定显示连接状态:
  - MQOK 或 MQNo
  - WiFiOK 或 WiFiNo
  - 格式: "MQOK | WiFiOK"
- 第 2 行循环显示:
  - 有 Wi-Fi: 显示 IP
  - 有消息: 在 IP 和消息之间交替显示
  - 无 Wi-Fi: 显示 "IP:-.-.-.-"

### MQTT 收发
- 收到 MQTT 消息后:
  - 大屏显示消息(可滚动)
  - 小屏继续显示系统状态

## 串口
- 波特率: 115200
- 用于日志与调试

## 脚本
### notify_board.sh (串口通知)
- 作用: 向板子串口发送通知
- 用法:
  - ./notify_board.sh
  - ./notify_board.sh "HELLO"
  - ./notify_board.sh --list

### notify_board_mqtt.sh (MQTT 通知)
- 作用: 通过 MQTT 发送消息
- 依赖: mosquitto
- 安装: brew install mosquitto
- 用法:
  - ./notify_board_mqtt.sh "HELLO MQTT"

## 主要配置位置
- Wi-Fi SSID/密码: sketch_apr8b.ino
- MQTT broker/topic: sketch_apr8b.ino
