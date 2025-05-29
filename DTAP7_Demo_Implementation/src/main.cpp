// Author: Florian LE RUYET
// Created: 15/05/2025
// Last Update: 27/05/2025
// Version: 1.0

/*
  The following code is the embedded program corresponding to the project of Team 7 from DTAP 2025
  Here the goal is to retrieve data from the EMG sensor and send relevant data to the cloud via HTTPS requests and recieve HTTP requests
*/ 

// Libraries used in the project
#include <Arduino.h>            //Arduino framework
#include <Wifi.h>               //Enables Wi-Fi connection
#include <HTTPClient.h>         //Enables Sending HTTP requests
#include <WifiClientSecure.h>   //Enables Use of HTTPS
#include <arduinoFFT.h>         //Enables FFT on microcontroller
#include <WebServer.h>          //Enables Hosting a Webserver
#include <ESPmDNS.h>            //Enables the local Webserver to be addressed

//Needed for connection to cloud
#define ssid "aalto open"
#define host "lambda.proto.aalto.fi"
#define port 443

//Microcontroller pins attribution
#define PIN_SENSOR 1

#define PIN_BATTERY 8
#define PIN_POWB 9

#define PIN_RED1 4
#define PIN_GREEN1 5
#define PIN_BLUE1 6

#define PIN_RED2 7
#define PIN_GREEN2 15
#define PIN_BLUE2 16

#define PIN_BUZZ 17

//Creation of server and http client
WebServer server(80);
WiFiClientSecure client;

//Global var needed in the program
String deviceID;

bool LED1=false;
bool LED2=false;
bool Measure=false;

u_int8_t Battery=100;
u_int64_t refPB=0;

const double Sampling=2000; //Samlping frequency (Range: 1-1000) *can be improved to support 1MHz(if hardware can handle)
const u_int16_t Samples=8192; //Numbers of Samples

//Creation of FFT item
double vReal[Samples];
double vImag[Samples];

ArduinoFFT<double> FFT=ArduinoFFT<double>(vReal,vImag,Samples,Sampling);

//Https certificate
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_start");

//Function to set color for LED1
void setColor1(int R, int G, int B) {
  analogWrite(PIN_RED1,R*154/255);
  analogWrite(PIN_GREEN1,G);
  analogWrite(PIN_BLUE1,B);
}

//Function to set color for LED2
void setColor2(int R, int G, int B) {
  analogWrite(PIN_RED2,R*154/255);
  analogWrite(PIN_GREEN2,G);
  analogWrite(PIN_BLUE2,B);
}

//Function to make LED1 blink blue
void Blink1() {
  if(LED1) {
    LED1=false;
    setColor1(0,0,0);
  }
  else {
    LED1=true;
    setColor1(0,0,220);
  }
}

//Function to handle http /measure request
void handleMeasure() {
  if(Measure) {
    Serial.printf("Measurement already running!\n");
    server.send(200, "text/plain", "Request received! Measurement already running!");
  }
  else {
    // start measurement
    Measure = true;
    Serial.printf("Measurement started!\n");
    server.send(200, "text/plain", "Request received! Measurement started!");
  }
}

//Function to handle any other request
void handleNotFound() {
  Serial.printf("Wrong request!\n");
  server.send(404, "text/plain", "Request not found!");
}


//Function to send the deactivation https request to cloud
void SendDeac() {
  String data="{\"deviceId\": "+deviceID+"}";   //Dynamic data String creation
  String CL="Content-Length: "+String(data.length());     //Dynamic Content Length String creation

  Serial.printf("Connecting to %s...\n", host);
  
  //If connection with cloud fails
  if (!client.connect(host, port)) {
    Serial.println("Connection failed!");
    return;
  }
  //Actual HTTPS request
  Serial.printf("Connection suceeded!\n");
  client.println("POST /api/devices/turn-off HTTP/1.1");
  client.println("Host: lambda.proto.aalto.fi");
  client.println("Content-Type: application/json");
  client.println(CL);
  client.println();
  client.println(data);

  //Receiving Cloud's response
  while (client.connected()) {
  String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  //Response is printed one character at a time
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  Serial.println();
  client.stop();
}


//Function to send the battery level https request to cloud
void SendBat() {
  Battery=100*(analogRead(PIN_BATTERY)-3300)/770;
  if(Battery>100) {
    Battery=100;
  }
  String data="{\"deviceId\": "+deviceID+",\"batteryLevel\": "+String(Battery)+"}";
  String CL="Content-Length: "+String(data.length());

  Serial.printf("Connecting to %s...\n", host);
  
  if (!client.connect(host, port)) {
    Serial.println("Connection failed!");
    return;
  }
  Serial.printf("Connection suceeded!\n");
  client.println("POST /api/devices/battery HTTP/1.1");
  client.println("Host: lambda.proto.aalto.fi");
  client.println("Content-Type: application/json");
  client.println(CL);
  client.println();
  client.println(data);

  while (client.connected()) {
  String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  Serial.println();
  client.stop();
}

//Function to send the macAddress and ipAddress https request to cloud
void SendMac() {

  String IP=WiFi.localIP().toString();
  String data="{\"macAddress\": \"00-B0-D0-63-C2-52\",\"ipAddress\": \""+IP+"\"}";
  String CL="Content-Length: "+String(data.length());

  Serial.printf("Connecting to %s...\n", host);
  
  if (!client.connect(host, port)) {
    Serial.println("Connection failed!");
    return;
  }
  Serial.printf("Connection suceeded!\n");
  client.println("POST /api/devices HTTP/1.1");
  client.println("Host: lambda.proto.aalto.fi");
  client.println("Content-Type: application/json");
  client.println(CL);
  client.println();
  client.println(data);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  char check=12;
  char charnum=0;
  bool wait=true;
  // The next part of the code prints the response
  // and reads what was sent by the server one character at a time:
  while (client.available()) {
    charnum++;
    char c = client.read();
    Serial.write(c);
    //Retrieving the ID given by the cloud
    if(charnum>check&&wait) {
      if(c==':'&&check!=133) {
        check=133;
      }
      else {
        if(c!=',') {
          deviceID=deviceID+c;
        }
        else {
          wait=false;
        }
      }
    }
  }
  Serial.println(deviceID);
  Serial.println();
  client.stop();
}

//Function to send the measurement result https request to cloud
void SendVal() {
  // double x = 423.094830;
  double x = FFT.majorPeak();
  String data="{\"deviceId\": "+deviceID+",\"value\": "+String(x,3)+"}";
  String CL="Content-Length: "+String(data.length());
  Serial.printf("Connecting to %s...\n", host);
  
  if (!client.connect(host, port)) {
    Serial.println("Connection failed!");
    return;
  }
  Serial.printf("Connection suceeded!\n");
  client.println("PUT /api/datapoints HTTP/1.1");
  client.println("Host: lambda.proto.aalto.fi");
  client.println("Content-Type: application/json");
  client.println(CL);
  client.println();
  client.println(data);

  while (client.connected()) {
  String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  Serial.println();
  client.stop();
}


//Function to sample data from the sensor and then maje FFT computations
void Measurement() {
  u_int16_t T_step=1000000/Sampling;
  u_int16_t ref=micros();
  for (uint16_t Sample=1;Sample<Samples;Sample++) {
    vReal[Sample]=analogRead(4);
    vImag[Sample]=0.0;
    while(micros()-ref<Sample*T_step);
  }
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);	/* Weigh data */
  FFT.compute(FFTDirection::Forward); /* Compute FFT */
}


//Function to enter deep sleep after button is released
void GoToSleep() {
  setColor1(0,0,0);
  setColor2(0,0,0);
  SendDeac();
  while(digitalRead(PIN_POWB)==HIGH);
  esp_deep_sleep_start();
}


//Main loop setup
void setup() {
  Serial.begin(115200);
  //Wi-Fi connection booting
  WiFi.mode(WIFI_STA);
  char timeout=0;
  WiFi.begin(ssid);

  Serial.printf("Connecting.");

  while(WiFi.status()!=WL_CONNECTED) {
    if(timeout>=25) {
      timeout=0;
      WiFi.begin(ssid);
    }
    Serial.printf(".");
    delay(500);
    Blink1();
    WiFi.reconnect();
    timeout++;
  }
  Serial.printf("\n");
  setColor1(0,0,220);
  LED1=true;

  Serial.printf("\nConnected to WiFi with Address ");
  Serial.print(WiFi.localIP());
  Serial.printf("\n");

  analogSetAttenuation(ADC_11db);

  //Pins setup
  pinMode(PIN_SENSOR,INPUT);
  pinMode(PIN_BATTERY,INPUT);
  pinMode(PIN_POWB,INPUT);

  pinMode(PIN_RED1,OUTPUT);
  pinMode(PIN_GREEN1,OUTPUT);
  pinMode(PIN_BLUE1,OUTPUT);

  pinMode(PIN_RED2,OUTPUT);
  pinMode(PIN_GREEN2,OUTPUT);
  pinMode(PIN_BLUE2,OUTPUT);

  pinMode(PIN_BUZZ,OUTPUT);

  //Deep sleep wake up sources
  // esp_sleep_enable_timer_wakeup(300000000);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1);

  //Set HTTPS certificate
  client.setCACertBundle(rootca_crt_bundle_start);

  //Set local Webserver handling rules
  server.on("/measure", handleMeasure);
  server.onNotFound(handleNotFound);
  server.begin();

  //Device ID retrieval and sending battery level
  delay(5000);
  SendMac();
  setColor1(0,220,0);
  delay(5000);
  SendBat();
}


//Main loop responsible for the device's functions
void loop() {
  //Sending the battery level whenever it changes
  if(Battery!=100*(analogRead(PIN_BATTERY)-3300)/770) {
    SendBat();
  }
  //Check if sleep has been triggered
  if(digitalRead(PIN_POWB)==HIGH) {
    if(refPB==0) {
      refPB=millis();
    }
    if(millis()-refPB>5000) {
      GoToSleep();
    }
  }
  else {
    refPB=0;
  }
  //Lights LED2 when sensor signal is strong enough
  if(analogRead(PIN_SENSOR)>2000) {
    setColor2(0,220,0);
    LED2=true;
  }
  //Shutting it down otherwise
  else {
    setColor2(0,0,0);
    LED2=false;
  }
  //When measurement asked, get data, compute, and send it to cloud
  if(Measure) {
    Measurement();
    SendVal();
    Measure=false;
  }
  server.handleClient();
  delay(1);
}



