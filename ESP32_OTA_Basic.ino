#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ptScheduler.h>
#include "html.h"

const char* host = "esp32";
const char* ssid = "JKE-TP-01";
const char* password = "JKE@2024";

//variabls to blink without delay:
const int led = 23;
bool ledState = LOW;             // ledState used to set the LED

const int arr_inp[] = {34, 35};
const int arr_out[] = {2, led};

int pb1_state, pb1_state_old;

WebServer server(80);
ptScheduler OTA (PT_MODE_ONESHOT, 1000);  //1MS
ptScheduler t1_ledBlink (PT_MODE_ONESHOT, 1000 * 1000); //1000MS
ptScheduler t2_counting (PT_MODE_ONESHOT, 1000 * 2000); //2000MS
ptScheduler t3_readPb (PT_MODE_ONESHOT, 1000 * 50); //50MS

void setup(void) {
  Serial.begin(115200);
  setup_pinmode();
  connect_to_wifi_network();
  setup_webserver();
  t1_ledBlink.setSleepMode (PT_SLEEP_SUSPEND);
  t1_ledBlink.setSequenceRepetition (6);
}

void loop(void) {
  if (OTA.call()) server.handleClient();

  if (t1_ledBlink.call()) {
    // if the LED is off turn it on and vice-versa:
    ledState = not(ledState);
    // set the LED with the ledState of the variable:
    digitalWrite(led, ledState);
    Serial.println(ledState ? "LED ON" : "LED OFF");
  }

  if (t2_counting.call()) {
    static int cnt;
    cnt++;
    Serial.print("Code 2 running:");
    Serial.println(cnt);
  }

  if(t3_readPb.call()){
    pb1_state = digitalRead(34);
    if (pb1_state_old== LOW && pb1_state == HIGH){
      t1_ledBlink.setInterval(1000*100);
      t1_ledBlink.reset();
    }
    pb1_state_old = pb1_state;
  }
}//END loop()

/*
   setup function
*/
void setup_pinmode() {
  for (int i = 0; i < sizeof(arr_inp) / sizeof(arr_inp[0]); i++) pinMode(arr_inp[i], INPUT_PULLUP);  
  for (int i = 0; i < sizeof(arr_out) / sizeof(arr_out[0]); i++) pinMode(arr_out[i], OUTPUT); 
}

void connect_to_wifi_network() {
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_webserver() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}
