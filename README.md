#  ESP32 Audio File Downloader and Player via MQTT

This project allows an **ESP32** device to:

- Connect to Wi-Fi
- Subscribe to MQTT for download commands
- Download `.mp3` audio files over HTTP
- Store files on an SD card
- Play audio through the **internal DAC** using I2S
- Publish download status over MQTT

---

## Hardware Requirements

- ESP32 board
- MicroSD card + SD card module
- Audio output (e.g., speaker via I2S DAC)
- Internet/Wi-Fi access
- MQTT Broker (local or remote)

---

## Libraries Used

- `WiFi.h` – Wi-Fi connection
- `PubSubClient.h` – MQTT client
- `SD.h` & `SPI.h` – SD card file system
- `HTTPClient.h` – HTTP GET requests
- `AudioFileSourceSD.h`, `AudioGeneratorMP3.h`, `AudioOutputI2S.h` – Audio playback (ESP32-audioI2S library)

---

## MQTT Topics

| Topic | Description |
|-------|-------------|
| `audio/files/download` | Expects an HTTP URL for the audio file to download |
| `audio/files/status`   | Publishes success/failure status of downloaded files |

---

##  How It Works

1. **Boots** and connects to Wi-Fi.
2. **Mounts SD card** and connects to the MQTT broker.
3. Subscribes to topic `audio/files/download`.
4. When a URL is published on the topic:
   - Downloads the `.mp3` file using HTTP.
   - Stores it on the SD card under `/audio/filename.mp3`.
   - Publishes success/failure status on `audio/files/status`.
   - Plays the downloaded file in a loop.
5. If a new URL is received:
   - Deletes the previous audio file.
   - Repeats the download and playback process.

---

## File Structure (on SD card)

---

## ⚙️ Configuration

In the source code, update the following:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "MQTT_BROKER_IP";
const int mqtt_port = 1883;

