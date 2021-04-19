#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_wifi.h"

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
const char *base_path = "/spiflash";
const char* settings = "/spiflash/settings.txt";

String SSID;
String PASSWORD;
String URL;
String Button1;
String Button2;
String Button3;
String USER_ID;

int WiFiSet = 0;

const int APPin = 16;
const char* defaultWifiMode = "clientMode";
WebServer server(80);

void loop() {
  server.handleClient();
  delay(100);
}

void sendButton(String value) {
  HTTPClient http;
  http.begin(URL);
  http.addHeader("BABY_USER_ID", USER_ID);
  http.addHeader("BABY_BUTTON_NUM", value.c_str());
  http.addHeader("Content-Length", "0");
  int httpCode = http.POST("");

  Serial.printf("Response: %d\n", httpCode);
  if(httpCode == HTTP_CODE_OK) {
    String body = http.getString();
    Serial.print("Response Body: ");
    Serial.println(body);
  }
  return;
}

void inputSettings() {
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "<form method='post' action='/set'>";
  html += "  <br>";
  html += "  <input type='text' name='ssid' value='" + SSID + "' >SSID<br>";
  html += "  <input type='text' name='pass' value='" + PASSWORD + "' >PASS<br>";
  html += "  <input type='text' name='url' value='" + URL + "' >API URL<br>";
  html += "  <input type='text' name='userid' value='" + USER_ID + "' >USER ID<br>";
  html += "  <input type='text' name='b1' value='" + Button1 + "' >Button No.1<br>";
  html += "  <input type='text' name='b2' value='" + Button2 + "' >Button No.2<br>";
  html += "  <input type='text' name='b3' value='" + Button3 + "' >Button No.3<br>";
  html += "  <br>";
  html += "  <input type='submit'><br>";
  html += "</form>";
  server.send(200, "text/html", html);
  return;
}

void setSettings() {
  Serial.printf("Got request to /set\n");
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String url = server.arg("url");
  String userid = server.arg("userid");
  String button1 = server.arg("b1");
  String button2 = server.arg("b2");
  String button3 = server.arg("b3");
  ssid.trim();
  pass.trim();
  url.trim();
  userid.trim();
  button1.trim();
  button2.trim();
  button3.trim();

  FILE *f = fopen(settings, "wb");
  fprintf(f, "%s\n", ssid.c_str());
  fprintf(f, "%s\n", pass.c_str());
  fprintf(f, "%s\n", url.c_str());
  fprintf(f, "%s\n", userid.c_str());
  fprintf(f, "%s\n", button1.c_str());
  fprintf(f, "%s\n", button2.c_str());
  fprintf(f, "%s\n", button3.c_str());
  fclose(f);

  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "SSID: " + ssid + "<br>";
  html += "PASS: " + pass + "<br>";
  html += "URL: " + url + "<br>";
  html += "USER_ID: " + userid + "<br>";
  html += "Button1: " + button1 + "<br>";
  html += "Button2: " + button2 + "<br>";
  html += "Button3: " + button3 + "<br>";
  html += "<hr>";
  html += "<h1>Please Reset!</h1>";

  server.send(200, "text/html", html);
  return;
}

void readSettings(void) {
  FILE *f = fopen(settings, "rb");
  if (f == NULL) {
    Serial.printf("No file!!\n");
    return;
  }
  Serial.printf("Open file\n");
  const int size = 7;
  char lines[size][128];
  int i;
  for(i = 0; i < size; i++) {
    lines[i][0] = '\0';
  }
  char *p;
  for(i = 0; i < size && fgets(lines[i], sizeof(lines[i]), f) != NULL; i++) {
    Serial.printf("line%d = %s", i, lines[i]);
  }
  fclose(f);

  for(i = 0; i < size; i++) {
    p = strchr(lines[i], '\n');
    if (p) {
      *p = '\0';
    }
  }

  SSID = lines[0];
  PASSWORD = lines[1];
  URL = lines[2];
  USER_ID = lines[3];
  Button1 = lines[4];
  Button2 = lines[5];
  Button3 = lines[6];
  Serial.printf("%s\n%s\n%s\n%s\n", SSID.c_str(), PASSWORD.c_str(), URL.c_str(), USER_ID.c_str());
  Serial.printf("%s\n%s\n%s\n", Button1.c_str(), Button2.c_str(), Button3.c_str());
  return;
}

void connectWiFi() {
  WiFi.begin(SSID.c_str(), PASSWORD.c_str());
  Serial.print("WiFi connecting");

  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }

  Serial.println(" connected");
  return;
}

void mountVFS() {
  esp_vfs_fat_mount_config_t mount_config;
  mount_config.max_files = 4;
  mount_config.format_if_mount_failed = true;
  mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

  esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
  if(err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
    return;
  }
}

void setup(void) {
  Serial.begin(115200);

  while (!Serial);

  esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
  Serial.printf("mount fs\n");
  mountVFS();
  Serial.printf("read settings\n");
  readSettings();

  uint64_t wake_pin = esp_sleep_get_ext1_wakeup_status();
  Serial.printf("%x\n", wake_pin);
  if(wake_pin & 0b10000000000000000000000000000000000) {
    Serial.println("WiFiMode AP Mode");
    Serial.print("Configuring access point...");
    WiFi.softAP("baby_button");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/",inputSettings);
    server.on("/set", HTTP_POST, setSettings);
    server.begin();
    Serial.println("HTTP server started");  
  } else {
    if(wake_pin & 0b00000000000000000000000000000000100) {
      Serial.printf("2\n");
      connectWiFi();
      sendButton(Button1);
    } else if(wake_pin & 0b00000000000000000001000000000000000) {
      Serial.printf("15\n");
      connectWiFi();
      sendButton(Button2);
    } else if(wake_pin & 0b00000000100000000000000000000000000) {
      Serial.printf("26\n");
      connectWiFi();
      sendButton(Button3);
    }
    esp_err_t r = esp_sleep_enable_ext1_wakeup(0b10000000100000000001000000000000100, ESP_EXT1_WAKEUP_ANY_HIGH); // 2, 15, 26, 34
    if (r != ESP_OK) {
      Serial.print("Failed to set ext1 wakeup\n\n");
    } else {
      Serial.print("Success to set ext1 wakeup\n\n");
    }
    esp_wifi_stop();
    esp_deep_sleep_start();
  }
  return;
}

