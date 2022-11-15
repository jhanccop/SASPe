/*
  Siren SASpe

  Esp32 - sim800 - RTC - mqtt - LCD Nokia
*/

/* ====================== BLUETOOTH CONFIG ======================== */
#include <NimBLEDevice.h>
static NimBLEServer* pServer;

/* ====================== EEPROM ======================== */
#include "EEPROM.h"
const int ssid_address = 0;
const int pass_address = 8;
const int loc_address = 40;
const int lat_address = 60;
const int long_address = 80;

/* ====================== LCD CONFIG ======================== */
#include <SPI.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 display = Adafruit_PCD8544(18, 23, 4, 15, 2);
const int contrastValue = 50;

#define pin_LCD     14
#define pin_buzzer  25

/* ====================== RTC CONFIG ======================== */

#include "time.h"
#include <ESP32Time.h>
ESP32Time rtc;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = -18000;

/*
  #include "RTClib.h"
*/

/* ====================== MQTT CONFIG ======================== */

#include <WiFi.h>

String ssid = "ssid";
String password = "pass";

WiFiClient client;
#include <PubSubClient.h>

PubSubClient mqtt(client);

/* settings MQTT */

const char *broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "ID_DEVICE";
const char *mqtt_user = "USER";
const char *mqtt_pass = "PASS";

const char* topicSirenStatus_subs = "saspe/sirene/#";

/* ---------------------- Variables de dispositivo ----------------------*/
String idDevice = "saspe16";
String location = "Sullana";
double longitud = -80.6824;
double latitud = -4.8585;

/*---------------------- Variables de evento ----------------------*/
String idE = "";
double longitudE = 0;
double latitudE = 0;
double latencyE = 0;
double distanceE = 0;
double magnitudE = 0;
String dateTimeE = "";
String dateE = "";
String timeE = "";
boolean activateE = false;
String s_idE = "";

unsigned long now = 0;

//const long interval = 5000; // secont wait to mqtt callback


/* ========================== times ========================== */
unsigned long previousMillis = 0; // init millis wait to mqtt callback
const long interval = 5000; // secont wait to mqtt callback

String dateTime = "";   // variable to datetime

long refresh = 30000;
unsigned long time_now = 0;

unsigned long timeStart = 0;
unsigned long timeEnd = 0;
unsigned long deltaTime = 0;

String valor;
int aleatorio;
String alea = "2";

/* ====================== SEPARADOR ======================== */
#include <Separador.h>
Separador s;

/* ====================== JSON ======================== */
#include <ArduinoJson.h>

/* ************************************************************************************************************** */
/* ************************************************* FUNCTIONS ************************************************** */
/* ************************************************************************************************************** */

void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnect();

/* ******************* PRINT MESSAGE  ********************** */
void print_text(String msg, int row) {
  int y = row * 8;

  display.fillRect(0, y, 83, y + 7, WHITE);
  display.display();

  display.setCursor(0, y);
  display.println(msg);
  display.display();
}

/* ******************* PRINT LCD MAINTANCE ********************** */
void print_manintance(String text) {
  display.clearDisplay();
  display.display();
  display.setCursor(0, 10);
  display.println(text);
  display.display();
}

String num2string(double n, int m, String text) {
  String temp = text;
  if (n != 0) {
    temp = String(n, m);
  }
  return temp;
}

/* ******************* PRINT LCD MAIN  ********************** */
void print_main(boolean lcdSiren, String lcdLocation, String lcdIdEvent, double lcdLatitude, double lcdLongitude, double lcdDistance, double lcdLatency, String lcdDateEvent, String lcdTimeEvent, double lcdMagnitudE) {
  String temp = "";
  display.clearDisplay();
  display.display();

  // ---- Date time ----
  display.setCursor(0, 0);
  display.println(rtc.getTime("%y-%m-%d %H:%M"));
  
  // ---- Lat long device ----
  temp = String(latitud) + ", " + String(longitud);
  display.setCursor(0, 8);
  display.println(temp);

  // ---- Event + type ----
  if (lcdSiren) {
    display.setTextColor(WHITE, BLACK);
  }
  display.setCursor(5, 16);
  display.println(lcdIdEvent);
  display.setTextColor(BLACK);

  // ---- Magnitud y distancia
  temp = num2string(lcdMagnitudE, 2, "mag") + "->" + num2string(lcdDistance, 2, "dist") + "Km";
  display.setCursor(0, 24);
  display.println(temp);

  // ---- Date event
  display.setCursor(0, 32);
  display.println(lcdDateEvent);

  // ---- Time event
  display.setCursor(0, 40);
  display.println(lcdTimeEvent);

  display.display();
}

/* ====================== PROCESS ======================== */

String payload_in = "";

void process(String payload_temp) {
  
  if (payload_temp == "exit") {
    payload_in = payload_in + ",exit,";
    Serial.println(payload_in);
    EEPROM.writeString(0, payload_in);
    EEPROM.commit();
    
    payload_in = "";
    payload_temp = "";

    delay(1000);
    setup_wifi();

    digitalWrite(pin_LCD,LOW);
  }
  payload_in += payload_temp;
}

/* ====================== SEPARA ======================== */
String separa(String text, String sep, int pos ) {
  String temp = "";
  int n = text.length();
  int count = 0;
  for (int i = 0; i < n; i++) {
    String tempChar = String(text[i]);
    if (tempChar == sep) {
      if (count == pos) {
        break;
      } else {
        temp = "";
        count++;
      }
    } else {
      temp = temp + tempChar;
    }
  }
  return temp;
}

/* ====================== SETTING RTC ======================== */

void setting_rtc() {
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    //return;
    print_manintance("Failed to obtain time... reconnect");
    delay(2000);
  }

  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  char th[3];
  strftime(th, 3, "%H", &timeinfo);
  int thour = String(th).toInt();

  char tm[3];
  strftime(tm, 3, "%M", &timeinfo);
  int tminute = String(tm).toInt();

  char ts[3];
  strftime(ts, 3, "%S", &timeinfo);
  int tsecond = String(ts).toInt();

  char td[3];
  strftime(td, 3, "%d", &timeinfo);
  int tday = String(td).toInt();

  char tM[3];
  strftime(tM, 3, "%m", &timeinfo);
  int tmonth = String(tM).toInt();

  char ty[5];
  strftime(ty, 5, "%Y", &timeinfo);
  int tyear = String(ty).toInt();

  rtc.setTime(tsecond, tminute, thour, tday, tmonth, tyear);

  char dt[60];
  strftime(dt, 61, "%A, %B %d %Y %H:%M:%S", &timeinfo);
  print_manintance(String(dt));
  delay(1500);

  display.clearDisplay();
  display.display();

}

/* ******************* ACTIVATE SIREN ********************** */
boolean active_siren(double epi_X, double epi_Y, double COORD_X, double COORD_Y, double radius) {
  double R = 6371;
  double pi = 3.1415926;

  double lat1 = COORD_Y * pi / 180;
  double lat2 = epi_Y * pi / 180;

  double varLat = abs(COORD_Y - epi_Y) * pi / 180;
  double varLong = abs(COORD_X - epi_X) * pi / 180;

  double a = pow(sin(varLat / 2), 2) + cos(lat2) * cos(lat1) * pow(sin(varLong / 2), 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  distanceE = R * c;

  return R * c < radius;
}

/* ******************* MQTT CALLBACK  ********************** */
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<300> doc;

  String json = "";
  for (int i = 0; i < length; i++)
  {
    json += String((char)payload[i]);
  }

  Serial.println(json);

  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  Serial.println(json);

  String topic_in = String(topic);

  if (topic_in == "saspe/sirene/set") {
    const char* p_id = doc["id"];
    const char* p_location = doc["location"];
    idDevice = String(p_id);
    location = String(p_location);
    latitud = doc["latitud"];
    longitud = doc["longitud"];

    location = s.separa(location, '/', 1);

    print_main(activateE, location, idE, latitudE, longitudE, distanceE, latencyE, dateE, timeE, magnitudE);

  } else if (topic_in == "saspe/sirene/test" || topic_in == "saspe/sirene/main") {

    double time_arrive = rtc.getSecond() + rtc.getMillis();

    const char* p_idE = doc["id"];
    const char* p_fecha = doc["fecha"];
    dateTimeE = String(p_fecha);

    dateE = s.separa(dateTimeE, ' ', 0);
    timeE = s.separa(dateTimeE, ' ', 1);

    latitudE = doc["latitud"];
    longitudE = doc["longitud"];
    magnitudE = doc["magnitud"];
    double impactoE = doc["impacto"];

    impactoE = 300;
    
    activateE = active_siren(longitudE, latitudE, longitud, latitud, impactoE);

    if (activateE && idE != String(p_idE)) {
      idE = String(p_idE);
      digitalWrite(pin_buzzer, LOW);
      digitalWrite(pin_LCD, HIGH);
    }

    String temp = s.separa(dateTimeE, ':', 2);
    double seconds_send = temp.toDouble();

    latencyE = time_arrive - seconds_send;
    if (latencyE < 0 ) latencyE = latencyE + 60;

    s_idE = String(p_idE) + "-";
    if(s.separa(topic_in, '/', 2) == "main"){
      s_idE += "M";
    }else if (s.separa(topic_in, '/', 2) == "test"){
      s_idE += "T";
    }

    print_main(activateE, location, s_idE, latitudE, longitudE, distanceE, latencyE, dateE, timeE, magnitudE);

    //print_text(".956", 23, 2);

  }

}

/* ******************* RECONNECT  ********************** */
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    print_manintance("MQTT connection...");
    //mqtt.publish(topicInit_pub,"reconnect");
    // Attempt to connect

    if (mqtt.connect(mqtt_id, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      mqtt.subscribe(topicSirenStatus_subs);
      print_manintance("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      print_manintance("failed");
      delay(5000);
    }
  }
}

/* ************************* SETUP BLUETOOTH ************************** */

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      Serial.println("Client connected");
      Serial.println("Multi-connect support: start advertising");
      NimBLEDevice::startAdvertising();
      digitalWrite(pin_LCD,HIGH);
      print_manintance("Bluetooth connected");
    };
    /** Alternative onConnect() method to extract details of the connection.
        See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
    */
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
      Serial.print("Client address: ");
      Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
      /** We can use the connection handle here to ask for different connection parameters.
          Args: connection handle, min connection interval, max connection interval
          latency, supervision timeout.
          Units; Min/Max Intervals: 1.25 millisecond increments.
          Latency: number of intervals allowed to skip.
          Timeout: 10 millisecond increments, try for 5x interval time for best results.
      */
      pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    };
    void onDisconnect(NimBLEServer* pServer) {
      Serial.println("Client disconnected - start advertising");
      NimBLEDevice::startAdvertising();
    };
    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) {
      Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
    };

    /********************* Security handled here **********************
    ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest() {
      Serial.println("Server Passkey Request");
      /** This should return a random 6 digit number for security
          or make your own static passkey as done here.
      */
      return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key) {
      Serial.print("The passkey YES/NO number: "); Serial.println(pass_key);
      /** Return false if passkeys don't match. */
      return true;
    };

    void onAuthenticationComplete(ble_gap_conn_desc* desc) {
      /** Check that encryption was successful, if not we disconnect the client */
      if (!desc->sec_state.encrypted) {
        NimBLEDevice::getServer()->disconnect(desc->conn_handle);
        Serial.println("Encrypt connection failed - disconnecting client");
        return;
      }
      Serial.println("Starting BLE work!");
    };
};

/** Handler class for characteristic actions */
class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic) {
      //Serial.print(pCharacteristic->getUUID().toString().c_str());
      //Serial.print(": onRead(), value: ");
      Serial.println(pCharacteristic->getValue().c_str());
    };

    void onWrite(NimBLECharacteristic* pCharacteristic) {
      //Serial.print(pCharacteristic->getUUID().toString().c_str());
      //Serial.print(": onWrite(), value: ");
      String temp = pCharacteristic->getValue().c_str();
      Serial.print(temp);
      process(temp);
    };
    /** Called before notification or indication is sent,
        the value can be changed here before sending if desired.
    */
    void onNotify(NimBLECharacteristic* pCharacteristic) {
      Serial.println("Sending notification to clients");
    };


    /** The status returned in status is defined in NimBLECharacteristic.h.
        The value returned in code is the NimBLE host return code.
    */
    void onStatus(NimBLECharacteristic* pCharacteristic, Status status, int code) {
      String str = ("Notification/Indication status code: ");
      str += status;
      str += ", return code: ";
      str += code;
      str += ", ";
      str += NimBLEUtils::returnCodeToString(code);
      Serial.println(str);
    };

    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
      String str = "Client ID: ";
      str += desc->conn_handle;
      str += " Address: ";
      str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
      if (subValue == 0) {
        str += " Unsubscribed to ";
      } else if (subValue == 1) {
        str += " Subscribed to notfications for ";
      } else if (subValue == 2) {
        str += " Subscribed to indications for ";
      } else if (subValue == 3) {
        str += " Subscribed to notifications and indications for ";
      }
      str += std::string(pCharacteristic->getUUID()).c_str();

      Serial.println(str);
    };
};

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor) {
      std::string dscVal((char*)pDescriptor->getValue(), pDescriptor->getLength());
      Serial.print("Descriptor witten value:");
      Serial.println(dscVal.c_str());
    };

    void onRead(NimBLEDescriptor* pDescriptor) {
      Serial.print(pDescriptor->getUUID().toString().c_str());
      Serial.println(" Descriptor read");
    };
};

/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
static DescriptorCallbacks dscCallbacks;
static CharacteristicCallbacks chrCallbacks;

/* ************************* SETUP BLUETOOTH ************************** */
void setup_bluetooth() {

  /** sets device name */
  NimBLEDevice::init("SasPe");

  /** Optional: set the transmit power, default is 3db */
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

  //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
  //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison


  //NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pDeadService = pServer->createService("DEAD");
  NimBLECharacteristic* pBeefCharacteristic = pDeadService->createCharacteristic(
        "BEEF",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        /** Require a secure connection for read and write access */
        NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
        NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
      );

  pBeefCharacteristic->setValue("Burger");
  pBeefCharacteristic->setCallbacks(&chrCallbacks);

  NimBLE2904* pBeef2904 = (NimBLE2904*)pBeefCharacteristic->createDescriptor("2904");
  pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
  pBeef2904->setCallbacks(&dscCallbacks);


  NimBLEService* pBaadService = pServer->createService("BAAD");
  NimBLECharacteristic* pFoodCharacteristic = pBaadService->createCharacteristic(
        "F00D",
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
      );

  pFoodCharacteristic->setValue("Fries");
  pFoodCharacteristic->setCallbacks(&chrCallbacks);

  NimBLEDescriptor* pC01Ddsc = pFoodCharacteristic->createDescriptor(
                                 "C01D",
                                 NIMBLE_PROPERTY::READ |
                                 NIMBLE_PROPERTY::WRITE |
                                 NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                 20
                               );
  pC01Ddsc->setValue("Send it back!");
  pC01Ddsc->setCallbacks(&dscCallbacks);

  pDeadService->start();
  pBaadService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  /** Add the services to the advertisment data **/
  pAdvertising->addServiceUUID(pDeadService->getUUID());
  pAdvertising->addServiceUUID(pBaadService->getUUID());

  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("Advertising Started");

}

/* ************************* SETUP WIFI ************************** */
void setup_wifi() {

  //char locationC[150];
  String payload_eeprom = String(EEPROM.readString(0));
  //locationS.toCharArray(locationC, (locationS.length() + 1));
  //location = locationS;
  Serial.println(payload_eeprom);

  ssid = s.separa(payload_eeprom, ',', 0);
  password = s.separa(payload_eeprom, ',', 1);
  location = s.separa(payload_eeprom, ',', 2);
  latitud = s.separa(payload_eeprom, ',', 3).toDouble();
  longitud = s.separa(payload_eeprom, ',', 4).toDouble();
  
  char ssidC[150];
  //ssid = String(EEPROM.readString(ssid_address));
  ssid.toCharArray(ssidC, (ssid.length() + 1));

  char passwordC[150];
  //password = EEPROM.readString(pass_address);
  password.toCharArray(passwordC, (password.length() + 1));

  WiFi.begin(ssidC, passwordC);
  delay(100);
  while (WiFi.status() != WL_CONNECTED) {
    //char ssidC[150];
    //ssid = String(EEPROM.readString(ssid_address));
    //ssid.toCharArray(ssidC, (ssid.length() + 1));

    //char passwordC[150];
    //password = EEPROM.readString(pass_address);
    //password.toCharArray(passwordC, (password.length() + 1));
    
    Serial.print(ssid);
    Serial.println(password);
    print_manintance("connect wifi");
    delay(1500);
  }

  delay(2000);
}

/* ******************* SETUP  ********************** */
void setup() {
  Serial.begin(115200);
  pinMode(pin_LCD, OUTPUT);
  pinMode(pin_buzzer, OUTPUT);
  digitalWrite(pin_buzzer, HIGH);

  if (!EEPROM.begin(64)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  digitalWrite(pin_LCD, HIGH);
  display.begin();
  display.setContrast(contrastValue);

  display.clearDisplay();
  display.display();

  //display.setTextColor(WHITE, BLACK); 
  display.invertDisplay(true);
  display.setCursor(0, 15);
  display.setTextSize(2);
  display.println(" SASPe ");
  display.display();

  display.setTextSize(0);
  display.invertDisplay(false);
  display.display();

  digitalWrite(pin_buzzer, LOW);
  delay(100);
  digitalWrite(pin_buzzer, HIGH);
  delay(100);

  setup_bluetooth();

  String input_payload = "";

  setup_wifi();

  display.invertDisplay(false);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.display();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  setting_rtc();

  digitalWrite(pin_LCD, LOW);

  mqtt.setServer(broker, mqtt_port);
  mqtt.setCallback(mqttCallback);


  /* **************** INIT AND SET RTC ********************* */
  if (mqtt.connect(mqtt_id, mqtt_user, mqtt_pass)) {
    //mqtt.publish(topicInit_pub,"start");
    mqtt.subscribe(topicSirenStatus_subs);
    print_manintance(topicSirenStatus_subs);

  }
}

void loop()
{
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop();

  if (millis() > now + 60000)
  {

    //print_datetime(true);
    char data[150];
    String payload = String(analogRead(34));
    payload.toCharArray(data, (payload.length() + 1));

    //mqtt.publish(topicData_pub, data);
    now = millis();
    digitalWrite(pin_LCD, LOW);
    digitalWrite(pin_buzzer, HIGH);

  } else if (millis() > time_now + 5000) {
    activateE = false;
    time_now = millis();
    digitalWrite(pin_LCD, LOW);
    print_main(activateE, location, s_idE, latitudE, longitudE, distanceE, latencyE, dateE, timeE, magnitudE);
    //print_datetime(true);
  }

}
