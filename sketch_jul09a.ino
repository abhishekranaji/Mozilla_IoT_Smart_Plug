//D2  ---SLP ---GPIO2
//D4  ---STP ---GPIO4
//D5  ---DIR ---GPIO5
//D18 ---ENB ---GPIO18
TaskHandle_t Task1;

#include "FS.h"
#include "SPIFFS.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
bool messageReceivedComplete = false;
std::string message;
long lastReconnectAttempt = 0;
int outVal = 0;
bool k = false;
int counter;
String clientId = "weo-"; // Add different clientID for each MCU
char c;
// Initialise the WiFi and MQTT Client objects
const char* mqtt_server = "35.193.200.242";
char mqtt_topic[34] = "windowcurtain";
char mqtt_publish[34] = "";
char ssid[30] = "We Are Heros";
char password[30] = "Homies@123";
char houseName[20] = "hyperoffice0";
char roomName[20] = "Hyper";
String ssID;
WiFiClient wifiClient;
PubSubClient client(mqtt_server , 1883, wifiClient); // 1883 is the listener port for the Broker
// First Time config parameters
const char *ap_ssid = "WEO-SwitchBoard";
const char *ap_password = "WEOPASSWORD";
// TCP SERVER PORT DECLARATION....
int port = 80;  //Port number
WiFiServer server(port);
const IPAddress weo_ip(35, 193, 200, 242);
const uint16_t weo_port = 1881;
// Function Setup
int timer = 0;
bool check = false;
void setup() {
  if (SPIFFS.begin()) {
    Serial.begin(115200);
    Serial.println("mounting FS...");
    Serial.println("mounted file system");
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(18, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(12, INPUT);
    pinMode(13, INPUT);

    digitalWrite(5, LOW); // dir
    digitalWrite(4, LOW); // step
    digitalWrite(18, HIGH); // en
    digitalWrite(2, HIGH); // slp
    digitalWrite(12, HIGH);
    digitalWrite(13, LOW);

    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      //      SPIFFS.remove("/config.json");

      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!error) {
          Serial.println("\nparsed json");
          strcpy(mqtt_topic, json["mqtt_topic"]);
          strcpy(ssid, json["ssid"]);
          strcpy(password, json["password"]);
          strcpy(houseName, json["url"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  Serial.println(ssid);
  Serial.println(sizeof(ssid));
  String ssID(ssid);
  Serial.print(ssID.length());
  Serial.println(password);
  counter = 0;
  //SSID CHECK
  if (ssID.length() != 0) {
    Serial.println("Everything going smooth..... Hows that happen");
    setup_wifi1();
    Serial.println("tested");
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    char bufferpub[40];
    char pub[10] = "/pub";
    strcpy(mqtt_publish, mqtt_topic);
    strcpy(mqtt_publish + strlen(mqtt_publish), pub);
    check = true;
    Serial.println(check);
  } else {
    Serial.println("should start BLE server");
    digitalWrite(16, LOW);
    setup_wifi();
  }
  xTaskCreatePinnedToCore(
    Task1code,   /* Task function. */
    "Task1",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */
  delay(500);
}

void Task1code( void * pvParameters ) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    client.loop();
    delay(1);
  }
}

void loop() {
  if (check == true) {
    if (!client.connected()) {
      reconnect();
    } else {
      // Client connected

    }
  }
}
void setup_wifi() {
  class ServerReadCallbacks: public BLECharacteristicCallbacks {
      void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        Serial.println();
        Serial.println(messageReceivedComplete);
        String inMsg;
        char ch;
        inMsg = rxValue.c_str();
        int Strlength = inMsg.length() + 1;
        char buf[Strlength];
        inMsg.toCharArray(buf, Strlength);
        StaticJsonDocument<200> json;
        deserializeJson(json, buf);
        strcpy(mqtt_topic, json["mqtt_topic"]);
        strcpy(ssid, json["ssid"]);
        strcpy(password, json["password"]);
        File order = SPIFFS.open("/config.json", "w+");
        if (order) {
          serializeJson(json, Serial);
          serializeJson(json, order);
        }
        order.close();
        //add to message (as of now just one packet)
        message = rxValue;
        Serial.print("Weo connecting to ");
        Serial.println(ssid);
        String hostname = "WEO-" + String(mqtt_topic);
        int hostnameLen = hostname.length();
        char hostbuf[20] = "";
        hostname.toCharArray(hostbuf, hostnameLen);
        WiFi.setHostname(hostbuf);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          yield();
          delay(500);
          Serial.print(".");
          timer = timer + 1;
          if (timer == 100) {
            SPIFFS.remove("/config.json");
            ESP.restart();
          }
        }
        randomSeed(micros());
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        client.setServer(mqtt_server, 1883);
        client.setCallback(callback);
        char bufferpub[40];
        char pub[10] = "/pub";
        strcpy(mqtt_publish, mqtt_topic);
        strcpy(mqtt_publish + strlen(mqtt_publish), pub);
        char buffer[512];
        while (!client.connected()) {
          reconnect();
          Serial.println("waiting");
          delay(400);
        }
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);
          configFile.readBytes(buf.get(), size);
          DynamicJsonDocument json(1024);
          DeserializationError error = deserializeJson(json, buf.get());
          size_t n = serializeJson(json, buffer);
          client.publish("setup", buffer, false);
          if (!error) {
            Serial.println("\nparsed json");
          } else {
            Serial.println("failed to load json config");
          }
          configFile.close();
        }
        //once you think all packets are received. As of now one packet
        messageReceivedComplete = true;
        check = true;
      }
  };
  Serial.println("Starting BLE work!");
  BLEDevice::init("WEO-Switchboard");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setValue("Hello From WEO");
  pCharacteristic->setCallbacks(new ServerReadCallbacks());
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
  //  delay(10);
  //  // We start by connecting to a WiFi network
  Serial.println();
}
void reconnect() {
  // Loop until reconnect
  while (!client.connected()) {
    yield();
    Serial.print("*WEO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect((clientId + mqtt_topic).c_str())) {
      Serial.println("*WEO: connected");
      client.subscribe("Weo");
      client.subscribe(mqtt_topic);
      File file = SPIFFS.open("/config.json", "r");
      if (file) {
        char buffer[1024];
        size_t size = file.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        json["IP"] =  WiFi.localIP().toString();
        size_t n = serializeJson(json, buffer);
        serializeJson(json, Serial);
        boolean rc = client.publish("PinState", buffer, false);
        client.subscribe("Weo");
      } else {
        Serial.println("*WEO: failed to load json config");
      }
      file.close();
      // Once connected, publish an announcement...
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("*WEO: try again in 5 seconds");
      if (client.state() == 0) {
        reconnect();
      }
      counter = counter + 1;
      if (counter == 600) {
        Serial.println("*WEO: Wait, Restarting MCU...... in 5 Seconds");
        delay(5000);
        setup();
      }
      delay(5000);
    }
  }
}
void setup_wifi1() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  String hostname = "WEO-" + String(mqtt_topic);
  int len = hostname.length();
  char buf[20] = "";
  hostname.toCharArray(buf, len);
  WiFi.setHostname(buf);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password, 3);
  while (WiFi.status() != WL_CONNECTED) {
    yield();
    delay(500);
    Serial.print(".");
    timer = timer + 1;
    if (timer == 100) {
      ESP.restart();
    }
  }
  Serial.println("Wifi_conected");
  class ServerReadCallbacks: public BLECharacteristicCallbacks {
      void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        Serial.println();
        Serial.println(messageReceivedComplete);
        String inMsg;
        char ch;
        inMsg = rxValue.c_str();
        int Strlength = inMsg.length() + 1;
        char buf[Strlength];
        inMsg.toCharArray(buf, Strlength);
        StaticJsonDocument<200> json;
        deserializeJson(json, buf);
        serializeJson(json, Serial);
        if (json["cmd"] == "reboot") {
          ESP.restart();
        }
        if (json["cmd"] == "reset") {
          bool f = SPIFFS.remove("/config.json");
          if (f) {
            ESP.restart();
          }
        }
        //add to message (as of now just one packet)
        message = rxValue;
        messageReceivedComplete = true;
        check = true;
      }
  };
  Serial.println("Starting BLE work!");
  //  String deviceName = "WEO-SB-" + String(mqtt_topic);
  String deviceName = "WEO-SB-";
  Serial.println(deviceName);
  int dnLen = deviceName.length() + 1;
  deviceName.toCharArray(buf, dnLen);
  std::string devicename = buf;
  //  deviceName.append(mqtt_topic);
  Serial.println("hello");
  Serial.println(devicename.c_str());
  BLEDevice::init(devicename);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setValue("Hello From WEO");
  pCharacteristic->setCallbacks(new ServerReadCallbacks());
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
  randomSeed(micros());
}

void callback(char* topic, byte * payload, unsigned int length) {
  //  StaticJsonDocument<512> json;
  DynamicJsonDocument json(512);
  deserializeJson(json, payload, length);
  Serial.println("");
  serializeJson(json, Serial);
  Serial.println("");
  Serial.print(String(topic));
  if (json.size() != 0) {
    String command = json["command"]; String RPMS = json["RPMS"]; String STEPS_PER_REV = json["STEPS_PER_REV"]; String MICROSTEPS_PER_STEP = json["MICROSTEPS_PER_STEP"];
    String room = json["room"];

    if (command == "reset") {
      bool f = SPIFFS.remove("/config.json");
      if (f) {
        ESP.restart();
      }
    }
    if (command == "Hello"){
      Serial.println("command is " + command);
    }
    float R = RPMS.toFloat();
    float Steps  = STEPS_PER_REV.toFloat();
    float Micro = MICROSTEPS_PER_STEP.toFloat();
    float  MICROSECONDS_PER_MICROSTEP =  (1000000 / (Steps * Micro) / (R / 60));
    if (room == roomName && command == "stop") {
      digitalWrite(2, LOW);
      delay(100);
      digitalWrite(4, HIGH);
      Serial.println("Stopped");
    }
    if (room == roomName && command != "null" && String(topic) == String(mqtt_topic)) {
      DynamicJsonDocument doc(512);
      char buffer[512];
      delay(100);
      if (command == "open" ) {
        digitalWrite(4, LOW); // ON MOTOR
        digitalWrite(2, HIGH);  // slp off
        digitalWrite(5, HIGH);
        delay(200);
        while (1) {
          Serial.println(digitalRead(12));
          if (digitalRead(12) == HIGH && digitalRead(14) == LOW) {
            delay(100);
            doc["deviceType"] = "curtains";
            doc["mqtt_topic"] = mqtt_topic;
            doc["houseName"] = houseName;
            doc["message"] = "opened";
            doc["roomName"] = roomName;
            size_t n = serializeJson(doc, buffer);
            client.publish(mqtt_publish, buffer, false);
            Serial.println("breakk_on");
            break;
          }
          digitalWrite(16, HIGH);
          delayMicroseconds((MICROSECONDS_PER_MICROSTEP * 0.9) / 2);
          digitalWrite(16, LOW);
          delayMicroseconds((MICROSECONDS_PER_MICROSTEP * 0.9) / 2);
        }
      }
      if (room == roomName && command == "close" ) {
        digitalWrite(4, LOW); // ON MOTOR
        digitalWrite(2, HIGH);  // slp off
        digitalWrite(5, LOW);
        delay(200);
        while (1) {
          if (digitalRead(14) == HIGH && digitalRead(12) == LOW) {
            delay(100);
            doc["deviceType"] = "curtains";
            doc["mqtt_topic"] = mqtt_topic;
            doc["houseName"] = houseName;
            doc["message"] = "closed";
            doc["roomName"] = roomName;
            size_t n = serializeJson(doc, buffer);
            client.publish(mqtt_publish, buffer, false);
            Serial.println("breakk_off");
            break;
          }
          digitalWrite(16, HIGH);
          delayMicroseconds((MICROSECONDS_PER_MICROSTEP * 0.9) / 2);
          digitalWrite(16, LOW);
          delayMicroseconds((MICROSECONDS_PER_MICROSTEP * 0.9) / 2);
        }
      }
      digitalWrite(4, HIGH);
    }
  }
}
