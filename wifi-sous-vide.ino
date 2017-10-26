#include <AutoPID.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266SSDP.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>

#include "PersistentWiFiManager.h"

//#define WIFI_CONNECT_TIMEOUT 30

#define RELAY_PIN D7
#define PULSEWIDTH 5000

#define DEVICE_NAME "Sous Vide"

//temperature sensor libraries and variables
#include <OneWire.h>
#include <DallasTemperature.h>
#define TEMP_SENSOR_PIN D4
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensors(&oneWire);
#define TEMP_READ_DELAY 800
double temperature, setTemp;
unsigned long timeAtTemp;
bool relayControl, powerOn;
AutoPIDRelay myPID(&temperature, &setTemp, &relayControl, 5000, .12, .0003, 0);

const char *metaRefreshStr = "<head><meta http-equiv=\"refresh\" content=\"1; url=/\" /></head><body><a href=\"/\">redirecting...</a></body>";

unsigned long lastTempUpdate;
void updateTemperature() {
  if ((millis() - lastTempUpdate) > TEMP_READ_DELAY) {
    temperature = temperatureSensors.getTempFByIndex(0);
    lastTempUpdate = millis();
    temperatureSensors.requestTemperatures();
  }
} //void updateTemperature

ESP8266WebServer server(80);
int scannedNetworks, scanssid;
DNSServer dnsServer;
//IPAddress apIP(192, 168, 1, 1);

//code from fsbrowser example, consolidated.
bool handleFileRead(String path) {
  Serial.println("handlefileread" + path);
  if (path.endsWith("/")) path += "index.htm";
  String contentType;
  if (path.endsWith(".htm") || path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".gif")) contentType = "image/gif";
  else if (path.endsWith(".jpg")) contentType = "image/jpeg";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  else if (path.endsWith(".xml")) contentType = "text/xml";
  else if (path.endsWith(".pdf")) contentType = "application/x-pdf";
  else if (path.endsWith(".zip")) contentType = "application/x-zip";
  else if (path.endsWith(".gz")) contentType = "application/x-gzip";
  else if (path.endsWith(".json")) contentType = "application/json";
  else contentType = "text/plain";
  String pathGz = path + ".gz";
  if (SPIFFS.exists(pathGz) || SPIFFS.exists(path)) {
    Serial.println("sending file " + path);
    File file = SPIFFS.open(SPIFFS.exists(pathGz) ? pathGz : path, "r");
    if (server.hasArg("download"))
      server.sendHeader("Content-Disposition", " attachment;");
    if (server.uri().indexOf("nocache") < 0)
      server.sendHeader("Cache-Control", " max-age=172800");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  } //if SPIFFS.exists
  return false;
} //bool handleFileRead

/*
void networkSetup(String ssid, String pass) {
  //attempt to connect to wifi
  WiFi.mode(WIFI_STA);
  if (ssid.length()) {
    if (pass.length()) WiFi.begin(ssid.c_str(), pass.c_str());
    else WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin();
  }
  Serial.println("WiFi.begin"); /////////////////////////////////////////////////
  unsigned long connectTime = millis();
  //while ((millis() - connectTime) < 1000 * WIFI_CONNECT_TIMEOUT && WiFi.status() != WL_CONNECTED)
  while (WiFi.status() != WL_CONNECT_FAILED && WiFi.status() != WL_CONNECTED && (millis() - connectTime) < 1000 * WIFI_CONNECT_TIMEOUT)
    delay(10);
  if (WiFi.status() != WL_CONNECTED) { //if timed out, switch to AP mode
    Serial.println("WIFI_AP"); ////////////////////////////////////////////////////
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(DEVICE_NAME);
  } //if
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start((byte)53, "*", apIP); //used for captive portal in AP mode

  server.on("/wifi/list", [] () {
    //scan for wifi networks
    int n = WiFi.scanNetworks();
    
    //build array of indices
    int ix[n];
    for (int i = 0; i < n; i++) ix[i] = i;
    
    //sort by signal strength
    for (int i = 0; i < n; i++) for (int j = 1; j < n - i; j++) if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) std::swap(ix[j], ix[j - 1]);
    //remove duplicates
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j]))) ix[j] = -1;
    
    //build plain text string of wifi info
    //format [signal%]:[encrypted 0 or 1]:SSID
    String s = "";
    for (int i = 0; i < n && s.length() < 2000; i++) { //check s.length to limit memory usage
      if (ix[i] != -1) {
        s += String(i?"\n":"") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2)
             + ":" + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1)
             + ":" + WiFi.SSID(ix[i]);
      }
    }

    //send string to client
    server.send(200, "text/plain", s);
  }); //server.on /wifi/list

  server.on("/wifi/wps", []() {
    server.send(200, "text/html", "attempting WPS");
    WiFi.mode(WIFI_STA);
    WiFi.beginWPSConfig();
    delay(100);
    if(WiFi.status() != WL_CONNECTED) {
      networkSetup("","");
    }
  }); //server.on /wifi/wps

  server.on("/wifi/connect", []() {
    server.send(200, "text/html", "attempting to connect...");
    networkSetup(server.arg("n"), server.arg("p"));
  }); //server.on /wifi/connect

} //void networkSetup

*/

void setup() {
  Serial.begin(115200); //for terminal debugging
  Serial.println();

  //allows serving of files from SPIFFS
  SPIFFS.begin();

  //set up temperature sensors and relay output
  temperatureSensors.begin();
  temperatureSensors.requestTemperatures();
  myPID.setBangBang(4);
  myPID.setTimeStep(4000);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  //networkSetup("", "");
  PersistentWiFiManager::setServers(server, dnsServer);
  PersistentWiFiManager::begin();

  //serve files from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) server.send(302, "text/html", metaRefreshStr);
  }); //server.onNotFound

  //handles commands from webpage, sends live data in JSON format
  server.on("/io", []() {
    Serial.println("server.on /io");////////////////////////
    if (server.hasArg("setTemp")) {
      powerOn = true;
      setTemp = server.arg("setTemp").toFloat();
    } //if
    if (server.hasArg("powerOff")) {
      powerOn = false;
    } //if

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["temperature"] = temperature;
    json["setTemp"] = setTemp;
    json["power"] = myPID.getPulseValue();
    json["running"] = powerOn;
    json["upTime"] = ((timeAtTemp) ? (millis() - timeAtTemp) : 0);
    JsonArray& errors = json.createNestedArray("errors");
    if (temperature < -190) errors.add("sensor fault");
    char jsonchar[200];
    json.printTo(jsonchar);
    server.send(200, "application/json", jsonchar);

  }); //server.on io

  server.on("/esp8266-project.json", [] () {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["device_name"] = DEVICE_NAME;
    char jsonchar[200];
    json.printTo(jsonchar);
    server.send(200, "application/json", jsonchar);
  });

  //SSDP makes device visible on windows network
  server.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(server.client());
  });
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(DEVICE_NAME);
  SSDP.setURL("/");
  SSDP.begin();
  SSDP.setDeviceType("upnp:rootdevice");

  server.begin();
  Serial.println("setup complete.");
} //void setup

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateTemperature();
  if (powerOn) {
    myPID.run();
    digitalWrite(RELAY_PIN, !relayControl);
    if (myPID.atSetPoint(2)) {
      if (!timeAtTemp)
        timeAtTemp = millis();
    } else {
      timeAtTemp = 0;
    }
  } else { //!powerOn
    timeAtTemp = 0;
    myPID.stop();
    digitalWrite(RELAY_PIN, HIGH);
  } //endif
  //Serial.println(millis());
} //void loop
