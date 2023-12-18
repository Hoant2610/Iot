#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Stepper.h>
#include <DHTesp.h>

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define buttonStepPin 15
#define rainSensorDO 18
#define rainSensorAO 36
#define buttonIsAuto 5
#define buttonRelayPin 23
#define relay 21
#define LED_PIN 19 


const int DHT_PIN = 3;
const int IN1 = 17;
const int IN2 = 16;
const int IN3 = 4;
const int IN4 = 2;

const int stepsPerRevolution = 2048;
DHTesp dhtSensor;
int button_state = HIGH;            
int last_button_state = HIGH;       
int button_auto_state = HIGH;       
int last_button_auto_state = HIGH;  
String stepState = "Close";
int button_state1 = HIGH;            
int last_button_state1 = HIGH;       
int button_auto_state1 = HIGH;       
int last_button_auto_state1 = HIGH;  
String relayState = "Close";
Stepper myStepper = Stepper(stepsPerRevolution, IN1, IN3, IN2, IN4);
int isAuto = 0;
int interval = 1000; 
unsigned long previousMillis = 0;


const char* ssid = "THHC";
const char* password = "THHTHH2002";

WiFiClient client;

String jsonString; 
String pin_status = "";

TempAndHumidity data = dhtSensor.getTempAndHumidity();
float t = data.temperature;
float h = data.humidity;
int r = 4095 - analogRead(rainSensorAO);
void sendHtml() {
  String response = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Document</title>
</head>
<body>
    <div>
        Nhiet Do :
        <span id='temperature'>__TEMPERATURE__</span>
    </div>
    <div>
        Do Am :
        <span id='humidity'>__HUMIDITY__</span>
    </div>
    <div>
        Luong Mua :
        <span id='rainfall'>__RAINFALL__</span>
    </div>
  
    <div>
        <button type='button' id='On-Auto'>On-Auto</button>
        <button type='button' id='Off-Auto'>Off-Auto</button>
        <span id='led_state'>Auto-State</span>
    </div>
    <div>
        <button type='button' id='On-ControlStep'>On-ControlStep</button>
        <button type='button' id='Off-ControlStep'>Off-ControlStep</button>
        <span id='stepState'>Step-State</span>
    </div>
    <div>
        <button type='button' id='On-ControlRelay'>On-ControlRelay</button>
        <button type='button' id='Off-ControlRelay'>Off-ControlRelay</button>
        <span id='relayState'>Relay-State</span>
    </div>

</body>
<script>
    var Socket;
    document.getElementById('On-Auto').addEventListener('click', OnAuto_pressed);
    document.getElementById('Off-Auto').addEventListener('click', OffAuto_pressed);
    document.getElementById('On-ControlStep').addEventListener('click', OnControlStep_pressed);
    document.getElementById('Off-ControlStep').addEventListener('click', OffControlStep_pressed);
    document.getElementById('On-ControlRelay').addEventListener('click', OnControlRelay_pressed);
    document.getElementById('Off-ControlRelay').addEventListener('click', OffControlRelay_pressed);

    function init() {
        Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
        Socket.onmessage = function (event) {
            processCommand(event);
        };
    }
    function processCommand(event) {
        var obj = JSON.parse(event.data);
        document.getElementById('led_state').innerHTML = obj.PIN_Status;
        document.getElementById('temperature').innerHTML = obj.Temp;
        document.getElementById('humidity').innerHTML = obj.Hum;
        document.getElementById('rainfall').innerHTML = obj.Rain;

        document.getElementById('stepState').innerHTML = obj.StepState;
        document.getElementById('relayState').innerHTML = obj.RelayState;

        console.log(obj.PIN_Status);
        console.log(obj.Temp);
        console.log(obj.Hum);
        console.log(obj.Rain);
        console.log(obj.StepState);
        console.log(obj.RelayState);

    }
    function OnAuto_pressed() {
        Socket.send('On-Auto');
    }
    function OffAuto_pressed() {
        Socket.send('Off-Auto');
    }
    function OnControlStep_pressed() {
        Socket.send('On-ControlStep');
    }
    function OffControlStep_pressed() {
        Socket.send('Off-ControlStep');
    }
    function OnControlRelay_pressed() {
        Socket.send('On-ControlRelay');
    }
    function OffControlRelay_pressed() {
        Socket.send('Off-ControlRelay');
    }


    window.onload = function (event) {
        init();
    }
</script>
</html>
  )";
  server.send(200, "text/html", response);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  myStepper.setSpeed(5);
  pinMode(buttonStepPin, INPUT_PULLUP);
  pinMode(buttonIsAuto, INPUT_PULLUP);
  pinMode(buttonRelayPin, INPUT_PULLUP);
  pinMode(rainSensorDO, INPUT);
  pinMode(rainSensorAO, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(relay, OUTPUT);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT11);

  server.on("/", sendHtml);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("HTTP server started");

  
}

void step_manual() {
  last_button_state = button_state;           
  button_state = digitalRead(buttonStepPin); 
  int spc = 512;
  if (last_button_state == HIGH && button_state == LOW) {
    if (stepState == "Close") {
      myStepper.step(spc);
      stepState = "Open";
    } else if (stepState == "Open") {
      myStepper.step(-spc);
      stepState = "Close";
    }
  }
}
void step_auto() {
  int spc = 512;
  int rainState = digitalRead(rainSensorDO);  
  int rainAmount = analogRead(rainSensorAO);  
  if (stepState == "Close") {
    if (rainState == LOW) {
      myStepper.step(spc);  //Mở
      stepState = "Open";
    }

  } else if (stepState = "Open") {
    if (rainState == HIGH) {
      myStepper.step(-spc);  //Đóng
      stepState = "Close";
    }
  }
}
void relay_manual() {
  last_button_state1 = button_state1;           
  button_state1 = digitalRead(buttonRelayPin); 
  if (last_button_state1 == HIGH && button_state1 == LOW) {
    if (relayState == "Close") {
      digitalWrite(relay, HIGH);
      relayState = "Open";
    } else if (relayState == "Open") {
      digitalWrite(relay, LOW);
      relayState = "Close";
    }
  }
}
void relay_auto() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float t = data.temperature;
  float h = data.humidity;
  if(relayState == "Close"){
    if(t > 35 || h < 65){
      digitalWrite(relay, HIGH);
      relayState = "Open";
    }
  } else if(relayState == "Open"){
    if(t<= 35 && h >= 65){
      digitalWrite(relay, LOW);
      relayState = "Close";
    }
  }
}
void viewtandh(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float t = data.temperature;
  float h = data.humidity;
  Serial.print("Nhiệt độ: ");
  Serial.print(t);
  Serial.println(" °C");
  Serial.print("Độ ẩm: ");
  Serial.print(h);
  Serial.println(" %");

  delay(1000);
}
void update_data(){
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();
  h = data.humidity;
  t = data.temperature;
  r = 4095 - analogRead(rainSensorAO);

  if (digitalRead(LED_PIN) == HIGH) {  //check if pin LED_PIN is high or low
    pin_status = "ON";
  }
  else {                          
    pin_status = "OFF"; //check if pin LED_PIN is high or low
    }
  stepState = stepState;
  relayState = relayState;

}
void update_webpage(){
  StaticJsonDocument<1000> doc;
  // create an object
  JsonObject object = doc.to<JsonObject>();
  object["RelayState"] = relayState ;
  object["StepState"] = stepState ;
  object["PIN_Status"] = pin_status ;
  object["Temp"] = t ;
  object["Hum"] = h ;
  object["Rain"] = r ;
  // object["Led1State"] = led1State ;
  // object["Led2State"] = led2State ;
  // object["FireAlarm"] = flameAlarm ;
  serializeJson(doc, jsonString); // serialize the object and save teh result to teh string variable.
  // Serial.println( jsonString ); // print the string for debugging.
  webSocket.broadcastTXT(jsonString); // send the JSON object through the websocket
  jsonString = ""; // clear the String.
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  myStepper = Stepper(stepsPerRevolution, 17, 4, 16, 2);
  myStepper.setSpeed(5);
  int spc = 512;
  String receivedData = String((char *)payload);
  switch (type) {
    case WStype_DISCONNECTED: // enum that read status this is used for debugging.
      Serial.print("WS Type ");
      Serial.print(type);
      Serial.println(": DISCONNECTED");
      break;
    case WStype_CONNECTED:  // Check if a WebSocket client is connected or not
      Serial.print("WS Type ");
      Serial.print(type);
      Serial.println(": CONNECTED");
      if (digitalRead(LED_PIN) == HIGH) {  //check if pin LED_PIN is high or low
        pin_status = "ON";
        update_webpage(); // update the webpage accordingly
      }
      else {                          
        pin_status = "OFF"; //check if pin LED_PIN is high or low
        update_webpage();// update the webpage accordingly
      }
      break;
    case WStype_TEXT: // check responce from client
      Serial.println(receivedData); // the payload variable stores teh status internally
          String receivedData = String((char *)payload);
      if (receivedData == "On-Auto") { 
        update_webpage();
        digitalWrite(LED_PIN, HIGH);  // Bật đèn
        isAuto = 1;                
      }
      if (receivedData == "Off-Auto") {
        update_webpage();
        digitalWrite(LED_PIN, LOW);  // Tắt đèn
        isAuto = 0;           
      }
      if (receivedData == "On-ControlStep") { 
        update_webpage();
        if(stepState == "Close"){
            stepState = "Open";
            myStepper.step(spc);
        }
      }
      if (receivedData == "Off-ControlStep") {
        update_webpage();
        if(stepState == "Open"){
            stepState = "Close";
            myStepper.step(-spc);
        }
      }
      if (receivedData == "On-ControlRelay") { 
        update_webpage();
        if(relayState == "Close"){
            relayState = "Open";
            digitalWrite(relay, HIGH);
        }
      }
      if (receivedData == "Off-ControlRelay") {
        update_webpage();
        if(relayState == "Open"){
            relayState = "Close";
            digitalWrite(relay, LOW);
        }
      }
      // if (receivedData == "On-FireAlarm") { 
      //   update_webpage();
      //   if(flameAlarm == "Close"){
      //       flameAlarm = "Open";
      //         digitalWrite(FLAMBELL_PIN, HIGH);
      //         digitalWrite(FLAMLIGHT_PIN, HIGH);
      //   }
      // }
      // if (receivedData == "Off-FireAlarm") {
      //   update_webpage();
      //   if(flameAlarm == "Open"){
      //       flameAlarm = "Close";
      //         digitalWrite(FLAMBELL_PIN, LOW);
      //         digitalWrite(FLAMLIGHT_PIN, LOW);
      //   }
      // }
      break;
  }
}

void loop() {
  viewtandh();
  server.handleClient();
  webSocket.loop();
  unsigned long currentMillis = millis(); // call millis  and Get snapshot of time
  if ((unsigned long)(currentMillis - previousMillis) >= interval) { // How much time has passed, accounting for rollover with subtraction!
    update_data(); // update temperature data.
    update_webpage(); // Update Humidity Data
    previousMillis = currentMillis;   // Use the snapshot to set track time until next event
  }


  last_button_auto_state = button_auto_state;
  button_auto_state = digitalRead(buttonIsAuto);
  if (last_button_auto_state == HIGH && button_auto_state == LOW) {
    if (isAuto == 0) {
      digitalWrite(LED_PIN, HIGH);  // Bật đèn
      isAuto = 1;              // Đặt isAuto thành 1
    } else if (isAuto == 1) {
      digitalWrite(LED_PIN, LOW);  // Tắt đèn
      isAuto = 0;             // Đặt isAuto thành 0
    }
  }
  if (isAuto == 0) {
    step_manual();
    relay_manual();
  } else if (isAuto == 1) {
    step_auto();
    relay_auto();
  }


}



