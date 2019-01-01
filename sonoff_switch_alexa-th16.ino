#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUDP.h>
#include <FS.h>

// Majority of code is from
// https://github.com/aneisch/Sonoff-Wemo-Home-Assistant

char ssid[256];                       // your network SSID (name)
char pass[256];                       // your network password
char friendlyName[32];                // Alexa and/or Home Assistant will use this name to identify your device
char serialNumber[32];                // Serial number of device (it seems alexa uses this to identify the device)
char uuid[64];                        // UUID seems to be unique as well
float targetTemp = 23.50;             // target Temperature to change to

// Multicast declarations for discovery
IPAddress ipMulti(239, 255, 255, 250);
const unsigned int portMulti = 1900;

// TCP port to listen on
const unsigned int webserverPort = 49153;

const int SENSOR_PIN = 14;
const int LED_PIN = 13;
const int RELAY_PIN = 12;
const int SWITCH_PIN = 0;

// initial switch (button) state
int switchState = 0;
// initial switch (button) state
int activated = 0;

DHT dht(SENSOR_PIN, DHT21);

// random declarations
char *alphaNum = "abcdefghijklmnopqrstuvwxyz0123456789";
char *numbers = "0123456789";

// data for global output
float lastTemp = 0;
float lastHum = 0;

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

//int status = WL_IDLE_STATUS;

WiFiUDP Udp;
byte packetBuffer[1500]; //buffer to hold incoming and outgoing packets

//Start TCP server
ESP8266WebServer server(webserverPort);


//-----------------------------------------------------------------------
// Random String Generator
//-----------------------------------------------------------------------

char* generateString(char *base, int length) {
  char res[length + 1];
  int i;

  for (i = 0; i < length; i++) {
    res[i] = base[random(0, strlen(base))];
  }

  res[length] = 0x00;

  return (res);
}

//-----------------------------------------------------------------------
// UDP Multicast Server
//-----------------------------------------------------------------------

char* getDateString()
{
  //Doesn't matter which date & time, will work
  //Optional: replace with NTP Client implementation
  return "Sat, 01 Dec 2018 14:57:46 GMT";
}

void responseToSearchUdp(IPAddress& senderIP, unsigned int senderPort)
{
  Serial.println("responseToSearchUdp");

  //This is absolutely neccessary as Udp.write cannot handle IPAddress or numbers correctly like Serial.print
  IPAddress myIP = WiFi.localIP();
  char ipChar[20];
  snprintf(ipChar, 20, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
  char portChar[7];
  snprintf(portChar, 7, ":%d", webserverPort);

  Udp.beginPacket(senderIP, senderPort);
  Udp.write("HTTP/1.1 200 OK\r\n");
  Udp.write("CACHE-CONTROL: max-age=86400\r\n");
  Udp.write("DATE: ");
  Udp.write(getDateString());
  Udp.write("\r\n");
  Udp.write("EXT:\r\n");
  Udp.write("LOCATION: ");
  Udp.write("http://");
  Udp.write(ipChar);
  Udp.write(portChar);
  Udp.write("/setup.xml\r\n");
  Udp.write("OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n");
  Udp.write("01-NLS: ");
  Udp.write(uuid);
  Udp.write("\r\n");
  Udp.write("SERVER: Unspecified, UPnP/1.0, Unspecified\r\n");
  Udp.write("X-User-Agent: redsonic\r\n");
  Udp.write("ST: urn:Belkin:device:**\r\n");
  Udp.write("USN: uuid:Socket-1_0-");
  Udp.write(serialNumber);
  Udp.write("::urn:Belkin:device:**\r\n");
  Udp.write("\r\n");
  Udp.endPacket();
}

void UdpMulticastServerLoop()
{
  int numBytes = Udp.parsePacket();
  if (numBytes <= 0) {
    return;
  }

  if (numBytes > 1500) {
    Serial.println("UDP packet too large (1500), skipping...");
    return;
  }

  IPAddress senderIP = Udp.remoteIP();
  unsigned int senderPort = Udp.remotePort();

  // read the packet into the buffer
  Udp.read(packetBuffer, numBytes);

  // print out the received packet
  //Serial.write(packetBuffer, numBytes);

  // check if this is a M-SEARCH for WeMo device
  String request = String((char *)packetBuffer);
  int mSearchIndex = request.indexOf("M-SEARCH");
  if (mSearchIndex >= 0)
    //return;
    responseToSearchUdp(senderIP, senderPort);
}


//-----------------------------------------------------------------------
// HTTP Server
//-----------------------------------------------------------------------

void handleRoot()
{
  Serial.println("handleRoot");

  String serverRoot = "<html><body>";
  serverRoot += "Tell Alexa to discover devices<br><br>";
  serverRoot += "Current temperature: ";
  serverRoot += String(lastTemp);
  serverRoot += " *C<br><br>";
  serverRoot += "Current humidity: ";
  serverRoot += String(lastHum);
  serverRoot += " %<br><br>";
  serverRoot += "Target temperature: ";
  serverRoot += String(targetTemp);
  serverRoot += "<form action='/changetemp' method=POST>Temperature: <input type=text name=targettemp><input type=submit name=change value=change></form>";
  serverRoot += "</body></html>";
  serverRoot += "\r\n";

  server.send(200, "text/html", serverRoot);
}

void handleTempChange() {
  Serial.println("handleTempChange");
  if (server.hasArg("targettemp")) {
    float newtemp = server.arg("targettemp").toFloat();
    server.send(200, "text/plain", "CHANGE TO " + String(newtemp));
    targetTemp = newtemp;
    saveTemp();
  } else {
    server.send(200, "text/plain", "CHANGE FAILED");
  }
}

void handleEventXml()
{
  Serial.println("HandleEventXML");

  String eventservice_xml = "<scpd xmlns=\"urn:Belkin:service-1-0\">"
                            "<actionList>"
                            "<action>"
                            "<name>SetBinaryState</name>"
                            "<argumentList>"
                            "<argument>"
                            "<retval/>"
                            "<name>BinaryState</name>"
                            "<relatedStateVariable>BinaryState</relatedStateVariable>"
                            "<direction>in</direction>"
                            "</argument>"
                            "</argumentList>"
                            "</action>"
                            "<action>"
                            "<name>GetBinaryState</name>"
                            "<argumentList>"
                            "<argument>"
                            "<retval/>"
                            "<name>BinaryState</name>"
                            "<relatedStateVariable>BinaryState</relatedStateVariable>"
                            "<direction>out</direction>"
                            "</argument>"
                            "</argumentList>"
                            "</action>"
                            "</actionList>"
                            "<serviceStateTable>"
                            "<stateVariable sendEvents=\"yes\">"
                            "<name>BinaryState</name>"
                            "<dataType>Boolean</dataType>"
                            "<defaultValue>0</defaultValue>"
                            "</stateVariable>"
                            "<stateVariable sendEvents=\"yes\">"
                            "<name>level</name>"
                            "<dataType>string</dataType>"
                            "<defaultValue>0</defaultValue>"
                            "</stateVariable>"
                            "</serviceStateTable>"
                            "</scpd>\r\n"
                            "\r\n";

  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/xml\r\n";
  header += "Content-Length: ";
  header += eventservice_xml.length();
  header += "\r\n";
  header += "Date: ";
  header += getDateString();
  header += "\r\n";
  header += "LAST-MODIFIED: Sat, 01 Jan 2000 00:00:00 GMT\r\n";
  header += "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n";
  header += "X-User-Agent: redsonic\r\n";
  header += "connection: close\r\n";
  header += "\r\n";
  header += eventservice_xml;

  //  Serial.println(header);

  server.sendContent(header);
}

void handleSetupXml()
{
  Serial.println("handleSetupXml");

  String body =
    "<?xml version=\"1.0\"?>\r\n"
    "<root xmlns=\"urn:Belkin:device-1-0\">\r\n"
    "<specVersion>\r\n"
    "<major>1</major>\r\n"
    "<minor>0</minor>\r\n"
    "</specVersion>\r\n"
    "<device>\r\n"
    "<deviceType>urn:Belkin:device:controllee:1</deviceType>\r\n"
    "<friendlyName>" + String(friendlyName) + "</friendlyName>\r\n"
    "<manufacturer>Belkin International Inc.</manufacturer>\r\n"
    "<manufacturerURL>http://www.belkin.com</manufacturerURL>\r\n"
    "<modelDescription>Belkin Plugin Socket 1.0</modelDescription>\r\n"
    "<modelName>Socket</modelName>\r\n"
    "<modelNumber>1.0</modelNumber>\r\n"
    "<UDN>uuid:Socket-1_0-" + uuid + "</UDN>\r\n"
    "<modelURL>http://www.belkin.com/plugin/</modelURL>\r\n"
    "<serialNumber>" + serialNumber + "</serialNumber>\r\n"
    "<serviceList>\r\n"
    "<service>\r\n"
    "<serviceType>urn:Belkin:service:basicevent:1</serviceType>\r\n"
    "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>\r\n"
    "<controlURL>/upnp/control/basicevent1</controlURL>\r\n"
    "<eventSubURL>/upnp/event/basicevent1</eventSubURL>\r\n"
    "<SCPDURL>/eventservice.xml</SCPDURL>\r\n"
    "</service>\r\n"
    "</serviceList>\r\n"
    "</device>\r\n"
    "</root>\r\n"
    "\r\n";

  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/xml\r\n";
  header += "Content-Length: ";
  header += body.length();
  header += "\r\n";
  header += "Date: ";
  header += getDateString();
  header += "\r\n";
  header += "LAST-MODIFIED: Sat, 01 Jan 2000 00:00:00 GMT\r\n";
  header += "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n";
  header += "X-User-Agent: redsonic\r\n";
  header += "connection: close\r\n";
  header += "\r\n";
  header += body;

  //  Serial.println(header);

  server.sendContent(header);
}

void handleUpnpControl()
{
  Serial.println("handleUpnpControl");

  //Extract raw body
  //Because there is a '=' before "1.0", it will give the following:
  //"1.0" encoding="utf-8"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:SetBinaryState xmlns:u="urn:Belkin:service:basicevent:1"><BinaryState>1</BinaryState></u:SetBinaryState></s:Body></s:Envelope>
  String body = server.arg(0);

  //Check valid request
  boolean isOn = body.indexOf("<BinaryState>1</BinaryState>") >= 0;
  boolean isOff = body.indexOf("<BinaryState>0</BinaryState>") >= 0;
  boolean isQuestion = body.indexOf("GetBinaryState") >= 0;
  Serial.println("body:");
  Serial.println(body);
  boolean isValid = isOn || isOff || isQuestion;
  if (!isValid) {
    Serial.println("Bad request from Amazon Echo");
    //Serial.println(body);
    server.send(400, "text/plain", "Bad request from Amazon Echo");
    return;
  }

  //On/Off Logic

  // handle question first, because otherwise it will switch the device
  if (isQuestion) {
    Serial.println("Alexa is asking for device state");
    String body =
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>" + String(digitalRead(RELAY_PIN)) + "</BinaryState>\r\n"
      "</u:GetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Length: ";
    header += body.length();
    header += "\r\n";
    header += "Content-Type: text/xml\r\n";
    header += "Date: ";
    header += getDateString();
    header += "\r\n";
    header += "EXT:\r\n";
    header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
    header += "X-User-Agent: redsonic\r\n";
    header += "\r\n";
    header += body;
    server.sendContent(header);
  }
  else if (isOn) {
    Serial.println("Alexa is asking to turn ON a device");
    activate();
    String body =
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>1</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Length: ";
    header += body.length();
    header += "\r\n";
    header += "Content-Type: text/xml\r\n";
    header += "Date: ";
    header += getDateString();
    header += "\r\n";
    header += "EXT:\r\n";
    header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
    header += "X-User-Agent: redsonic\r\n";
    header += "\r\n";
    header += body;
    server.sendContent(header);
  }
  else if (isOff) {
    Serial.println("Alexa is asking to turn OFF a device");
    deactivate();
    String body =
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>0</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Length: ";
    header += body.length();
    header += "\r\n";
    header += "Content-Type: text/xml\r\n";
    header += "Date: ";
    header += getDateString();
    header += "\r\n";
    header += "EXT:\r\n";
    header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
    header += "X-User-Agent: redsonic\r\n";
    header += "\r\n";
    header += body;
    server.sendContent(header);
  }

}

void handleNotFound()
{
  Serial.println("handleNotFound()");

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void setupMode() {
  Serial.println("Entering SETUP mode...");

  // setup mode indicator
  digitalWrite(LED_PIN, 0);
  delay(500);
  digitalWrite(LED_PIN, 1);
  delay(500);
  digitalWrite(LED_PIN, 0);
  delay(500);
  digitalWrite(LED_PIN, 1);
  delay(500);

  // format SPIFFS
  SPIFFS.format();

  // start WPS detection
  digitalWrite(LED_PIN, 0);
  Serial.println("Trying to get WPS network...");
  WiFi.mode(WIFI_STA);
  bool wpsSuccess = WiFi.beginWPSConfig();
  Serial.print("WPS configuration ");
  if (wpsSuccess) Serial.println("SUCESSFUL!");
  else Serial.println("FAILED!");

  digitalWrite(LED_PIN, 1);
  if (WiFi.SSID().length() > 0) {
    Serial.println("SUCCESS - writing data to SPIFFS...");

    // write SSID file
    File f = SPIFFS.open("/ssid.txt", "w+");
    f.print(WiFi.SSID().c_str());
    f.close();

    // write password file
    f = SPIFFS.open("/pass.txt", "w+");
    f.print(WiFi.psk().c_str());
    f.close();

    // write friendly name file
    memset(friendlyName, 0, 32);
    strcat(friendlyName, "Sonoff ");
    strcat(friendlyName, generateString(numbers, 3));
    f = SPIFFS.open("/name.txt", "w+");
    f.print(friendlyName);
    f.close();

    // write serial number
    memset(serialNumber, 0, 32);
    strcat(serialNumber, generateString(numbers, 6));
    strcat(serialNumber, "K");
    strcat(serialNumber, generateString(numbers, 7));
    f = SPIFFS.open("/serial.txt", "w+");
    f.print(serialNumber);
    f.close();

    // write uuid
    memset(uuid, 0, 64);
    strcat(uuid, generateString(alphaNum, 8));
    strcat(uuid, "-");
    strcat(uuid, generateString(alphaNum, 4));
    strcat(uuid, "-");
    strcat(uuid, generateString(alphaNum, 4));
    strcat(uuid, "-");
    strcat(uuid, generateString(alphaNum, 4));
    strcat(uuid, "-");
    strcat(uuid, generateString(alphaNum, 12));
    f = SPIFFS.open("/uuid.txt", "w+");
    f.print(uuid);
    f.close();

    // write target temperature file
    saveTemp();

  } else {
    Serial.println("FAILED...");
  }

  // reboot the ESP
  Serial.println("Reseting device...");
  ESP.restart();
}

void saveTemp() {
  // write target temperature file
  File f = SPIFFS.open("/temp.txt", "w+");
  f.print(String(targetTemp));
  f.close();
}

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  // init random number generator
  randomSeed(analogRead(0));

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
  pinMode(SENSOR_PIN, INPUT);

  // init sensor
  dht.begin();

  // Init file system
  SPIFFS.begin();

  // read SSID
  if (SPIFFS.exists("/ssid.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/ssid.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    tmp.toCharArray(ssid, 256);
  } else setupMode();

  // read PASSWORD
  if (SPIFFS.exists("/pass.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/pass.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    tmp.toCharArray(pass, 256);
  } else setupMode();

  // read FRIENDLYNAME
  if (SPIFFS.exists("/name.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/name.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    tmp.toCharArray(friendlyName, 32);
  } else setupMode();

  // read SERIAL NUMBER
  if (SPIFFS.exists("/serial.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/serial.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    tmp.toCharArray(serialNumber, 32);
  } else setupMode();

  // read UUID
  if (SPIFFS.exists("/uuid.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/uuid.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    tmp.toCharArray(uuid, 64);
  } else setupMode();

  // read target Temperature
  if (SPIFFS.exists("/temp.txt")) {
    String tmp = "";
    File f = SPIFFS.open("/temp.txt", "r");
    while (f.available()) tmp += char(f.read());
    f.close();
    targetTemp = tmp.toFloat();
  } else setupMode();

  digitalWrite(LED_PIN, 1);
  digitalWrite(RELAY_PIN, 0);

  // setting up Station AP
  WiFi.mode(WIFI_STA); //Stop wifi broadcast
  WiFi.begin(ssid, pass);

  // Wait for connect to AP
  Serial.print("[Connecting to ");
  Serial.print(ssid);
  Serial.print("]...");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    // enter setup if button is pressed while connecting
    if (digitalRead(SWITCH_PIN) == 0) setupMode();

    // status indicator
    digitalWrite(LED_PIN, 0);
    delay(100);
    digitalWrite(LED_PIN, 1);
    delay(100);
    Serial.print("+");

    // check max connection tries - wait time
    tries++;
    if (tries > 50) {
      digitalWrite(LED_PIN, 0);
      break;
    }
  }

  // print your WiFi info
  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.print("Connected to ");
  Serial.print(WiFi.SSID());
  Serial.print(" with IP: ");
  Serial.println(ip);

  // output config
  Serial.print("Friendly name: ");
  Serial.println(friendlyName);
  Serial.print("Serial number: ");
  Serial.println(serialNumber);
  Serial.print("UUID: ");
  Serial.println(uuid);
  Serial.print("Target temperature: ");
  Serial.println(targetTemp);

  //UDP Server
  Udp.beginMulticast(WiFi.localIP(),  ipMulti, portMulti);
  Serial.print("Udp multicast server started at ");
  Serial.print(ipMulti);
  Serial.print(":");
  Serial.println(portMulti);

  //Web Server
  server.on("/", handleRoot);
  server.on("/changetemp", HTTP_POST, handleTempChange);
  server.on("/setup.xml", handleSetupXml);
  server.on("/eventservice.xml", handleEventXml);
  server.on("/upnp/control/basicevent1", handleUpnpControl);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.print("HTTP server started on port ");
  Serial.println(webserverPort);
}

void switchOn() {
  switchState = 1;
  Serial.println("SWITCHING RELAY ON");
  digitalWrite(RELAY_PIN, 1);
}

void switchOff() {
  switchState = 0;
  Serial.println("SWITCHING RELAY OFF");
  digitalWrite(RELAY_PIN, 0);
}

void activate() {
  activated = 1;
  Serial.println("ACTIVATING TEMP CHECK");
  digitalWrite(LED_PIN, 0);
}

void deactivate() {
  activated = 0;
  Serial.println("DEACTIVATING TEMP CHECK");
  digitalWrite(LED_PIN, 1);
  switchOff();
}

void checkTemp() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(t) && !isnan(h)) {
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print(" °C\tHum: ");
    Serial.print(h);
    Serial.println(" %");

    // remember data for http output
    lastTemp = t;
    lastHum = h;

    // if temp is going to be too low
    if (t < targetTemp - 0.5 && !switchState) {
      Serial.println("Temp to Low AND heater is off!");
      switchOn();
    }

    // switch off, as soon as temp has reached target temp
    if (t >= targetTemp && switchState) {
      Serial.println("Temp to High AND heater is on!");
      switchOff();
    }

  }

}

void loop() {
  ESP.wdtFeed();

  // check temperature
  if (activated) checkTemp();

  // check button presses
  if (digitalRead(SWITCH_PIN)) {
    delay(250);
    if (!digitalRead(SWITCH_PIN)) {
      activated = !activated;
      Serial.println("Switch Pressed!");

      // Show and change Relay State
      if (activated) {
        activate();
      }
      else
      {
        deactivate();
      }
      delay(500);
    }
  }

  UdpMulticastServerLoop();   //UDP multicast receiver

  server.handleClient();      //Webserver
}
