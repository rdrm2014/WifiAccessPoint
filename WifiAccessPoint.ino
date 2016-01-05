/*  http://192.168.4.1/ */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <IRremoteESP8266.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* idESP8266= "ESP8266Client1";

/**
 * Flags Pins
 */
const boolean flagIR = false;
const boolean flagIRReader = false;
const boolean flagTEMP = true;
const boolean flagRELAY = false;
const boolean flagPIR = true;

/**
 * Configurações de Pins
 */
const int pinIR = 2;
const int pinIRReader = 4;
const int pinTEMP = 5;
const int pinRELAY = 12;
const int pinPIR = 13;
int pirState = LOW;

/**
 * Configurações de IR
 */
IRrecv irrecv(pinIRReader);
decode_results results;
IRsend irsend(pinIR);

/* 
 * Configurações da Temperatura 
 */
OneWire oneWire(pinTEMP);
DallasTemperature DS18B20(&oneWire);

/**
 * Configurações de Rede
 */
const char* ssid = "ESPap";
const char* password = "password";

//IPAddress local_ip(192, 168, 2, 110);
//IPAddress gateway(192, 168, 2, 1);
//IPAddress subnet(255, 255, 255, 0);

/**
 * Configurações de MQTT
 */
const char* mqtt_server = "192.168.160.122";
WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];
int value = 0;

/**
 * Tempos de Atualização
 */
long lastReadIR = 0;
long lastTemp = 0;
long lastRelay = 0;
long lastPir = 0;
long lastIR = 0;

/**
 * Chars Result
 */
char resultReadIR[200];
char resultTemp[200];
char resultRelay[200];
char resultPir[200];
char resultIR[200];


/**
 * Setup
 */
void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configuring access point...");
  if(flagIR){
    pinMode(pinIR, OUTPUT);  
  }
  if(flagTEMP){
    pinMode(pinTEMP, INPUT);  
  }  
  if(flagRELAY){
    pinMode(pinRELAY, OUTPUT);  
  }
  if(flagPIR){    
    pinMode(pinPIR, INPUT);
  }
  if(flagIRReader){
    irrecv.enableIRIn();
  }
  
  setup_WIFI();
  setup_MQTT();
}

/**
 * Loop
 */
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(flagTEMP){
    mqttTemp(resultTemp);
  }
  if(flagPIR){
    mqttPir(resultPir);
  }
}

/************************************ WIFI ************************************/

/**
 * Setup WIFI
 */
void setup_WIFI() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  
  //WiFi.config(local_ip, gateway, subnet);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  IPAddress myIP = WiFi.localIP();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(myIP);
}

/**
 * ReadIR
 * @param      {char*}   result
 */
void ReadIR(char* result) {
  //irrecv.enableIRIn();
  delay(100);
  Serial.println("RESULT: ");
  //irrecv.enableIRIn(); // Start the receiver
  //while (irrecv.decode(&results) == 0) {}
  if (irrecv.decode(&results)) {
    Serial.println("RESULT: ");
    Serial.println(results.value, HEX);
    //Serial.println(results.bits, DEC);
    int readIR = results.value;
    
    //Serial.println("RAW ");
    //dump(&results);
    
    //snprintf(result, 200, "{\"code\": %x}", results.value);    
    snprintf(result, 200, "{\"code\": %x}", readIR);
    irrecv.resume();
  }
  delay(100);
}

/**
 * Temp
 * @param      {char*}   result
 */
void Temp(char* result) {
  float temperature;
  DS18B20.requestTemperatures();
  temperature = DS18B20.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(temperature);
  
  snprintf(result, 200, "{\"temp\": %d.%d}", (int)temperature, (int)((temperature - (int)temperature) * 100));
}

/**
 * Relay
 * @param      {char*}   result
 */
void Relay(String code, char* result) {
  Serial.println(code);
  if (code =="HIGH") {
    digitalWrite ( pinRELAY, HIGH);
    snprintf(result, 200, "{\"relay\": HIGH}");
  } else if (code == "LOW") {
    digitalWrite ( pinRELAY, LOW);
    snprintf(result, 200, "{\"relay\": LOW}");
  }
}

/**
 * Pir
 * @param      {char*}   result
 */
void Pir(char* result) {
  int val = digitalRead(pinPIR);
  if (val == HIGH) {
    if (pirState == LOW) {
      Serial.println("Motion detected!");      
      pirState = HIGH;
      snprintf(result, 200, "{\"state\": %d}", pirState);            
      client.publish("ESP8266_Pir", result);
    }
  } else {
    if (pirState == HIGH) {
      Serial.println("Motion ended!");
      pirState = LOW;
      snprintf(result, 200, "{\"state\": %d}", pirState);      
      client.publish("ESP8266_Pir", result);
    }
  }  
}

/**
 * IR
 * @param      {char*}   result
 */
void IR(unsigned long code, char* result) {
  irsend.sendNEC(code, 36);
  snprintf(result, 200, "{\"result\": true}");
}

/************************************ MQTT ************************************/
/**
 * Setup MQTT
 */
void setup_MQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/**
 * Callback MQTT
 */
void callback(char* top, byte* payload, unsigned int length) {
  String topic = top;
  String message;
  //char* message;
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    //Serial.print((char)payload[i]);
    message += (char)payload[i];
    //message += (char)payload[i];
  }
  //Serial.println(message);
  //Serial.println(message);

  if (flagIR && topic == "ESP8266_IR_send") {
    Serial.println("topic=='ESP8266_IR_send'");

    unsigned long codeVal=0;
    //const unsigned int len = message.length;
    //unsigned int len = length;

    for (int l = 0; l < length; l++) {
      if(message[l] >= '0' && message[l]<= '9'){
        codeVal += (message[l]-'0')*pow(16,(length-l));
      } else if(message[l] >= 'A' && message[l]<= 'F'){
        codeVal += (message[l]-'0')*pow(16,(length-l));
      } else if(message[l] >= 'a' && message[l]<= 'f'){
        codeVal += (message[l]-'0')*pow(16,(length-l));
      }
    }
    
    //message.toCharArray(buf, len)
    //unsigned int code = strtoul(message, 0, 32);
    
    //unsigned long code = message.toInt();
    
    //unsigned long code = strtoul(message, 0, length);
    //unsigned long code = 0x10EF08F7;
    Serial.println("TESTE1");
    Serial.println(codeVal);
    Serial.println(codeVal,HEX);
    Serial.println("TESTE2");
    mqttIR(codeVal, resultIR);
  } else if (flagRELAY && topic == "ESP8266_Relay_send") {
    Serial.println("topic=='ESP8266_Relay_send'");
    mqttRelay(message, resultRelay);
  } else if (flagIRReader && topic == "ESP8266_ReadIR_send") {
    Serial.println("topic=='ESP8266_ReadIR'");
    mqttReadIR(resultReadIR);
  }
}

/**
 * Reconnect
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(idESP8266)) {
      Serial.println("connected");
      
      if(flagIR){        
        client.subscribe("ESP8266_IR_send");
      }
      if(flagRELAY){
        client.subscribe("ESP8266_Relay_send");
      }
      if(flagIRReader){        
        client.subscribe("ESP8266_ReadIR_send");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * mqttTemp
 */
void mqttTemp(char* result) {
  long now = millis();
  if (now - lastTemp > 10000) {
    lastTemp = now;
    Temp(result); 
    client.publish("ESP8266_Temp", result);
  }
}

/**
 * mqttPir
 */
void mqttPir(char* result) {
  long now = millis();
  if (now - lastPir > 1000) {
    lastPir = now;
    Pir(result);
  }
}

/**
 * mqttRelay (VER) Inicio???
 */
void mqttRelay(String code, char* result) {
  Relay(code, result);
  client.publish("ESP8266_Relay", result);
}

/**
 * mqttIR
 */
void mqttIR(unsigned long code, char* result) {
//void mqttIR(char* result) {
  //unsigned long code = 0x10EF08F7;
  IR(code, result);
  client.publish("ESP8266_IR", result);
}

/**
 * mqttReadIR
 */
void mqttReadIR(char* result) {  
  ReadIR(result);
  Serial.println(result);  
  client.publish("ESP8266_ReadIR", result);
}
