#include <WiFi.h>
#include <PubSubClient.h>
#include <SD.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h> // For audio output (I2S used for internal audio playback)

// Wi-Fi credentials
const char* ssid = "DEXTRIS";
const char* password = "Dextris@789";

// MQTT broker details
const char* mqtt_server = "192.168.1.25";
const int mqtt_port = 1883;
const char* mqtt_topic_download = "audio/files/download";
const char* mqtt_topic_status = "audio/files/status";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Audio objects for playback
AudioFileSourceSD *file;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *out; // Using internal DAC

// Global variables to manage file path
String currentFilePath = "";

// Function to connect to Wi-Fi
void connectWiFi() {
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected to Wi-Fi. IP Address: %s\n", WiFi.localIP().toString().c_str());
}

// Function to connect to MQTT broker
void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT broker...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("connected!");
            mqttClient.subscribe(mqtt_topic_download);
        } else {
            Serial.printf("failed, rc=%d. Retrying in 5 seconds...\n", mqttClient.state());
            delay(5000);
        }
    }
}

// Publish file status
void publishFileStatus(const String& fileName, size_t fileSize, bool success) {
    String message = success ? 
        "Download Success: File=" + fileName + ", Size=" + String(fileSize) + " bytes" :
        "Download Failed: File=" + fileName;

    mqttClient.publish(mqtt_topic_status, message.c_str());
    Serial.printf("Published status: %s\n", message.c_str());
}

// Download file from the provided URL to SD card
bool downloadFile(const String& url, const String& localPath) {
    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    Serial.printf("HTTP GET Code: %d\n", httpCode); 
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }

    File file = SD.open(localPath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing.");
        http.end();
        return false;
    }

    int totalSize = http.getSize();
    Serial.printf("Total file size: %d bytes\n", totalSize);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    size_t bytesWritten = 0;

    while (bytesWritten < totalSize) {
        if (stream->available() > 0) {
            int bytesRead = stream->readBytes(buffer, sizeof(buffer));
            if (bytesRead > 0) {
                file.write(buffer, bytesRead);
                bytesWritten += bytesRead;
                Serial.printf("Written %u bytes\n", bytesWritten);
            } else {
                break;
            }
        } else {
            delay(10);
        }
    }

    file.close();
    http.end();

    if (bytesWritten != totalSize) {
        Serial.printf("File size mismatch. Expected: %d, Written: %u\n", totalSize, bytesWritten);
        return false;
    }

    Serial.printf("File written successfully: %s\n", localPath.c_str());
    return true;
}

// Function to play audio in a loop
void playAudioLoop() {
    while (true) {
        if (currentFilePath != "") {
            Serial.printf("Playing audio file: %s\n", currentFilePath.c_str());

            file = new AudioFileSourceSD(currentFilePath.c_str());
            mp3 = new AudioGeneratorMP3();
            out = new AudioOutputI2S(0, 1); // Using internal DAC
            out->SetOutputModeMono(true);   // Set output to mono (single DAC output)
            out->SetGain(0.9);              // Set gain for volume control

            if (!mp3->begin(file, out)) {
                Serial.println("Failed to start audio playback.");
                delete file;
                delete mp3;
                delete out;
                continue; // retry in next loop
            }

            while (mp3->isRunning()) {
                if (!mp3->loop()) {
                    break;
                }
                delay(10);
            }

            Serial.println("Audio playback finished.");
            mp3->stop();
            delete file;
            delete mp3;
            delete out;
        }

        delay(1000); // Wait for next iteration
    }
}

// Function to wait for updates and delete existing file
void waitForUpdateAndErase() {
    while (true) {
        mqttClient.loop(); // Listen for MQTT messages

        // If a new update arrives, delete the existing file
        if (currentFilePath != "") {
            Serial.printf("Erasing existing file: %s\n", currentFilePath.c_str());
            SD.remove(currentFilePath.c_str());
            currentFilePath = ""; // Reset file path after deletion
        }

        delay(500); // Check for updates every 500ms
    }
}

// MQTT message callback
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) { 
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.printf("Received message on topic %s: %s\n", topic, message.c_str());

    // Handle download requests
    if (String(topic) == mqtt_topic_download) {
        // Download the new file to SD card
        String localPath = "/audio/" + message.substring(message.lastIndexOf('/') + 1);
        bool success = downloadFile(message, localPath);

        // Publish the status of the download
        File file = SD.open(localPath.c_str(), FILE_READ);
        size_t fileSize = file.size();
        file.close();
        publishFileStatus(localPath, fileSize, success);

        // If download is successful, update the current file path
        if (success) {
            currentFilePath = localPath;
        }
    }
}

void setup() {
    Serial.begin(115200); // Debugging UART

    // Connect to Wi-Fi
    connectWiFi();

    // Initialize SD card
    if (!SD.begin()) {
        Serial.println("SD card initialization failed!");
        return;
    }
    Serial.println("SD card mounted successfully");

    // Initialize MQTT
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);

    // Connect to MQTT broker
    connectMQTT();

    // Start tasks (in separate threads)
    xTaskCreate(playAudioLoop, "AudioLoopTask", 4096, NULL, 1, NULL);  // Audio loop task
    xTaskCreate(waitForUpdateAndErase, "WaitForUpdateTask", 4096, NULL, 1, NULL); // Update task
}

void loop() {
    // Main loop just ensures the ESP32 runs normally and listens to MQTT
    mqttClient.loop();
}
