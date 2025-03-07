#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

const char* ssid = "wifi";
const char* password = "pasword";

// URL Google Apps Script
const String scriptUrl = "https://script.google.com/macros/s/";

// Inisialisasi ADS1115
Adafruit_ADS1115 ads;
ESP8266WebServer server(80);

// Kalibrasi ACS712
float vOffset[4] = {2.506, 2.502, 2.514, 2.510};  
float sensitivity = 0.066;

// Variabel penyimpanan nilai sensor
float current[4] = {0, 0, 0, 0};
float power[4] = {0, 0, 0, 0};
float energy_kWh[4] = {0, 0, 0, 0};

unsigned long lastSensorRead = 0;
unsigned long lastUpdateTime = 0;
const unsigned long sensorInterval = 1000;
const unsigned long sendInterval = 30000;

struct Timer {
    int duration;
    unsigned long endTime;
    int pin;
};

Timer timers[4] = {
    {0, 0, D4},
    {0, 0, D5},
    {0, 0, D6},
    {0, 0, D7}
};

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    Serial.println(WiFi.localIP());
    
    for (int i = 0; i < 4; i++) {
        pinMode(timers[i].pin, OUTPUT);
        digitalWrite(timers[i].pin, HIGH);
    }
    
    server.on("/", []() {
        server.send(200, "text/html", generateWebPage());
    });
    
    server.on("/setTimer", []() {
        int id = server.arg("id").toInt();
        int duration = server.arg("duration").toInt();
        if (id >= 0 && id < 4 && timers[id].duration == 0) {
            timers[id].duration = duration;
            timers[id].endTime = millis() + (duration * 1000);
            digitalWrite(timers[id].pin, LOW);
            server.send(200, "text/plain", "Timer Set.");
        } else {
            server.send(403, "text/plain", "Invalid request or timer already running.");
        }
    });
    
       server.on("/status", []() {
        int id = server.arg("id").toInt();
        unsigned long currentTime = millis();
        bool active = (timers[id].duration > 0 && currentTime < timers[id].endTime);
        String message = active ? "Running: " + String((timers[id].endTime - currentTime) / 1000) + " sec left" : "Timer is off.";
        String json = "{\"message\": \"" + message + "\"}";
        server.send(200, "application/json", json);
    });
    

    if (!ads.begin()) {
        Serial.println("Gagal mendeteksi ADS1115. Periksa koneksi!");
        while (1);
    }
    ads.setGain(GAIN_ONE);
    
    server.begin();
}

void loop() {
    server.handleClient();
    unsigned long currentTime = millis();
    
        for (int i = 0; i < 4; i++) {
        if (timers[i].duration > 0 && currentTime >= timers[i].endTime) {
            digitalWrite(timers[i].pin, HIGH);
            timers[i].duration = 0;
            timers[i].endTime = 0;
        }
    }

    if (currentTime - lastSensorRead >= sensorInterval) {
        lastSensorRead = currentTime;
        bacaSensor();
    }

    if (currentTime - lastUpdateTime >= sendInterval) {
        lastUpdateTime = currentTime;
        kirimKeSpreadsheet();
    }
}

void bacaSensor() {
    for (int i = 0; i < 4; i++) {
        int16_t adcRaw = ads.readADC_SingleEnded(i);
        float voltage = adcRaw * (4.096 / 32767.0);
        current[i] = abs((voltage - vOffset[i]) / sensitivity);
        if (abs(current[i]) < 0.05) current[i] = 0.0;
        power[i] = current[i] * 220.0;
        energy_kWh[i] += (power[i] * (sensorInterval / 3600000.0));
    }
}

void kirimKeSpreadsheet() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;  // Gunakan WiFiClientSecure untuk HTTPS
        client.setInsecure();  // Abaikan verifikasi SSL/TLS
        
        HTTPClient http;
        String url = scriptUrl;

        // Kirim data ampere & watt (historical)
        url += "?m1_a=" + String(current[0], 3) + "&m1_w=" + String(power[0], 3);
        url += "&m2_a=" + String(current[1], 3) + "&m2_w=" + String(power[1], 3);
        url += "&m3_a=" + String(current[2], 3) + "&m3_w=" + String(power[2], 3);
        url += "&m4_a=" + String(current[3], 3) + "&m4_w=" + String(power[3], 3);

        // Kirim data kWh (realtime)
        url += "&m1_kwh=" + String(energy_kWh[0], 6);
        url += "&m2_kwh=" + String(energy_kWh[1], 6);
        url += "&m3_kwh=" + String(energy_kWh[2], 6);
        url += "&m4_kwh=" + String(energy_kWh[3], 6);

        http.begin(client, url);  // Gunakan client yang telah di-set ke mode insecure
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            Serial.println("Data berhasil dikirim!");
        } else {
            Serial.print("Gagal kirim data, kode: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    }
}

String generateWebPage() {
    String page = "<!DOCTYPE html><html><head><title>Hijau Cerdas - Timer Controller</title>";
    page += "<style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; margin: 0; padding: 0; }";
    page += ".container { margin: 20px auto; width: 80%; display: grid; gap: 20px; grid-template-columns: repeat(4, 1fr); }";
    page += ".box { padding: 15px; background: #fff; border-radius: 8px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); text-align: center; transition: transform 0.3s; }";
    page += ".box:hover { transform: scale(1.05); }";
    page += ".input-group { display: flex; justify-content: space-between; align-items: center; margin: 5px 0; }";
    page += "label { font-weight: bold; }";
    page += "input { width: 50px; padding: 5px; border: 1px solid #ddd; border-radius: 4px; text-align: center; }";
    page += "button { margin-top: 10px; padding: 10px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; width: 100%; transition: background 0.3s; }";
    page += "button:hover { background: #0056b3; }";
    page += "</style></head><body>";
    page += "<h2>Hijau Cerdas - Timer Controller</h2>";
    page += "<a href='https://hijaucerdas.com/p/dashboard.html' target='_blank'>DASHBOARDS</a><div class='container'>";

    for (int i = 0; i < 4; i++) {
        page += "<div class='box'><h3>Mesin " + String(i + 1) + "</h3>";
        page += "<div class='input-group'><label>Hours</label><input type='number' id='hours" + String(i) + "' min='0' value='0'></div>";
        page += "<div class='input-group'><label>Minutes</label><input type='number' id='minutes" + String(i) + "' min='0' max='59' value='0'></div>";
        page += "<div class='input-group'><label>Seconds</label><input type='number' id='seconds" + String(i) + "' min='0' max='59' value='0'></div>";
        page += "<button onclick='setTimer(" + String(i) + ")'>Set Timer</button>";
        page += "<p id='status" + String(i) + "'>Checking...</p></div>";
    }
    
    page += "</div><script>";
    page += "function setTimer(id) {";
    page += "let hours = document.getElementById('hours' + id).value; let minutes = document.getElementById('minutes' + id).value; let seconds = document.getElementById('seconds' + id).value;";
    page += "let totalSeconds = (hours * 3600) + (minutes * 60) + parseInt(seconds);";
    page += "let xhr = new XMLHttpRequest(); xhr.open('GET', '/setTimer?id=' + id + '&duration=' + totalSeconds, true); xhr.send();}";
    page += "function updateStatus() { for (let i = 0; i < 4; i++) { let xhr = new XMLHttpRequest(); xhr.open('GET', '/status?id=' + i, true);";
    page += "xhr.onload = function() { document.getElementById('status' + i).innerHTML = JSON.parse(xhr.responseText).message; }; xhr.send(); } } setInterval(updateStatus, 1000);";
    page += "</script></body></html>";
    return page;
}
