#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TZ.h>
#include <time.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include "Secrets.h"

#define FAN1 D7
#define FAN2 D8
#define ONE_WIRE_BUS D6
#define PWM_SPEED 12000

#define INPUT_OUTPUT_COUNT 2

const char* ssid = STASSID;
const char* password = STAPSK;
const char* host = HOSTNAME;
const char* updatePassword = UPDATEPASSWORD;

int led_pin = LED_BUILTIN;

bool otaAvailable = false;
bool timeIsSet = false;
bool fsAvailable = false;
bool online = false;

time_t lastLoggedtime = 0;
time_t lastCheckedTime = 0;

FS* fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature temperatureSensors(&oneWire);

DeviceAddress thermometer[INPUT_OUTPUT_COUNT];
const uint8_t fan[INPUT_OUTPUT_COUNT] = {FAN1, FAN2};

ESP8266WebServer server(80);             // create a web server on port 80


void setup() {
  SetupHardware();

  Serial.begin(115200);

  Serial.println("Booting");

  SetupFan();

  SetupTemperatureSensor();

  SetupMDNS();

  SetupWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    SetupOTA();
  }

  SetupTime();

  SetupFileSystem();

  SetupWebServer();

  /* switch off led */
  digitalWrite(led_pin, HIGH);

  /* configure dimmers, and OTA server events */
  analogWrite(led_pin, 990);
}

void SetupFileSystem(){
  fsAvailable = fileSystem->begin();
  Serial.println(fsAvailable ? F("Filesystem initialized.") : F("Filesystem init failed!"));
  Serial.println("LittleFS started. Contents:");
  Dir dir = fileSystem->openDir("/");
  while (dir.next()) {                      // List the file system contents
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  Serial.printf("\n");
}

void SetupHardware(){
  for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++) {
    pinMode(fan[i], OUTPUT);
    digitalWrite(fan[i], 0);
  }

  /* switch on led */
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  
  analogWriteRange(1000);
  analogWriteFreq(10000);
}

void SetupMDNS() { // Start the mDNS responder
  MDNS.begin(host);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(host);
  Serial.println(".local");
}

void SetupWiFi(){
  Serial.print("Scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  for (int i = 0; i < n; i++)
  {
    Serial.println(WiFi.SSID(i));
  }
  Serial.println();
  WiFi.begin(ssid, password);

  for (int i = 0; i < 2 && WiFi.waitForConnectResult() != WL_CONNECTED; i++){
    WiFi.begin(ssid, password);
    Serial.println("Retrying connection...");
  }
  
  if (WiFi.status() == WL_CONNECTED){
    online = true;
  }

  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}

void timeIsSetCb(bool from_sntp /* <= this parameter is optional */) {
  if (from_sntp) {
    Serial.println(F("Time was set"));
    timeIsSet = true;
  }
}

void SetupTime(){
  settimeofday_cb(timeIsSetCb);
  configTime(TZ_Europe_Berlin, F("0.de.pool.ntp.org"), F("1.de.pool.ntp.org"), F("2.de.pool.ntp.org"));
}

void SetupOTA(){
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(updatePassword);
  ArduinoOTA.onStart([]() {  // switch off all the PWMs during upgrade
    for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++ ){
      analogWrite(fan[i], 0);
    }
    analogWrite(led_pin, 0);
    server.close();
  });

  ArduinoOTA.onEnd([]() {  // do a fancy thing with our board led at end
    for (int i = 0; i < 30; i++) {
      analogWrite(led_pin, (i * 100) % 1001);
      delay(50);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });

  /* setup the OTA server */
  ArduinoOTA.begin();
  otaAvailable = true;
  Serial.println("OTA Ready");
}

void SetupFan(){
  analogWriteFreq(PWM_SPEED);
  for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++){
    analogWrite(fan[i], 300);
  }
  delay(250);
  for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++){
    analogWrite(fan[i], 50);
  }
  delay(5000);
  for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++){
    analogWrite(fan[i], 90);
  }

  analogWriteFreq(100);
  delay(1000);
  analogWriteFreq(500);
  delay(800);
  analogWriteFreq(1000);
  delay(500);
  analogWriteFreq(1500);
  delay(500);
  analogWriteFreq(PWM_SPEED);
  delay(1000);
  analogWriteFreq(1500);
  delay(250);
  analogWriteFreq(PWM_SPEED);
  delay(250);
  analogWriteFreq(1500);
  delay(250);
  analogWriteFreq(PWM_SPEED);

  for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++){
    analogWrite(fan[i], 0);
  }
}

void SetupTemperatureSensor(){
  // Start up the library
  temperatureSensors.begin();

  for (uint8_t i  = 0; i < INPUT_OUTPUT_COUNT; i++){
    if (temperatureSensors.getAddress(thermometer[i], i)){
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" Address: ");
      printAddress(thermometer[i]);
      Serial.println();
    } else {
      Serial.print("Unable to find address for temperaturesensor "); 
      Serial.println(i);
    } 
    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
    temperatureSensors.setResolution(thermometer[i], 11);
    Serial.print("Device");
    Serial.print(i);
    Serial.print("Resolution: ");
    Serial.println(temperatureSensors.getResolution(thermometer[i]), DEC); 
  }
}

void handleClearData(String path){
  if (fileSystem->exists("/temp.csv")){
    fileSystem->remove("/temp.csv");
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "No file available");
  }
}

void SetupWebServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload);                       // go to 'handleFileUpload'

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    analogWrite(led_pin, 990);
  } else {
    analogWrite(led_pin, 0);
  }

  if (otaAvailable){
    ArduinoOTA.handle();
  }

  server.handleClient();                      // run the server

  if (timeIsSet) {
    if (time(nullptr) - lastLoggedtime > 30){
      lastLoggedtime = time(nullptr);
      temperatureSensors.requestTemperatures(); // Send the command to get temperatures
      delay(5);
      // After we got the temperatures, we can print them here.
      // We use the function ByIndex, and as an example get the temperature from the first sensor only.
      FSInfo info;
      fileSystem->info(info);
      if (info.totalBytes - info.usedBytes < 4096){
        fileSystem->remove("/temp.csv");
      }

      auto temp0 = temperatureSensors.getTempC(thermometer[0]);
      auto temp1 = temperatureSensors.getTempC(thermometer[1]);

      if (temp0 < 0) temp0 = 0;
      if (temp1 < 0) temp1 = 0;

      uint32_t logTime = time(nullptr);
      // The actual time is the last NTP time plus the time that has elapsed since the last NTP response
      Serial.printf("Appending temperature to file, time: %lu", logTime);
      File tempLog = fileSystem->open("/temp.csv", "a"); // Write the time and the temperature to the csv file
      tempLog.print(logTime);
      tempLog.print(',');
      tempLog.print(temp0);
      tempLog.print(',');
      tempLog.println(temp1);
      tempLog.close();

      if (WiFi.status() != WL_CONNECTED){
        WiFi.begin(ssid, password);
      }
    }
  }

  if (time(nullptr) - lastCheckedTime >= 30){
    lastCheckedTime = time(nullptr);
    temperatureSensors.requestTemperatures(); // Send the command to get temperatures
    delay(5);
    for (uint8_t i = 0; i < INPUT_OUTPUT_COUNT; i++){
      float tempC = temperatureSensors.getTempC(thermometer[i]);
      int speed = 0;
      switch((int)tempC){
        case -128 ... 0:
          break;
        case 1 ... 34:
          speed = 0;
          break;
        case 35 ... 50:
          analogWrite(fan[i], 300);
          delay(250);
          speed = (tempC - 34) * 50;
          break;
        default:
          speed = 1000;
      }
      analogWrite(fan[i], speed);
    }
  }
  delay(200);
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  return "N/A";
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);

  if (path == "/clear") {
    if (fileSystem->exists("/temp.csv")){
      fileSystem->remove("/temp.csv");
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "No file available");
    }
    return true;
  } else {
    if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
    String contentType = getContentType(path);             // Get the MIME type
    String pathWithGz = path + ".gz";
    if (fileSystem->exists(pathWithGz) ||  fileSystem->exists(path)) { // If the file exists, either as a compressed archive, or normal
      if (fileSystem->exists(pathWithGz))                         // If there's a compressed version available
        path += ".gz";                                         // Use the compressed verion
      File file = fileSystem->open(path, "r");                    // Open the file
      size_t sent = server.streamFile(file, contentType);    // Send it to the client
      file.close();                                          // Close the file again
      Serial.println(String("\tSent file: ") + path);
      return true;
    }
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload() { // upload a new file to the SPIFFS
  File fsUploadFile;
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
      if (fileSystem->exists(pathWithGz))                     // version of that file must be deleted (if it exists)
        fileSystem->remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = fileSystem->open(path, "w");               // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
