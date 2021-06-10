#include <Arduino_JSON.h>

/*
	Minimal Esp8266 Websockets Client

	This sketch:
        1. Connects to a WiFi network
        2. Connects to a Websockets server
        3. Sends the websockets server a message ("Hello Server")
        4. Sends the websocket server a "ping"
        5. Prints all incoming messages while the connection is open

    NOTE:
    The sketch dosen't check or indicate about errors while connecting to 
    WiFi or to the websockets server. For full example you might want 
    to try the example named "Esp8266-Client".

	Hardware:
        For this sketch you only need an ESP8266 board.

	Created 15/02/2019
	By Gil Maimon
	https://github.com/gilmaimon/ArduinoWebsockets

*/

#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>

const char* ssid = "HomeWifi"; //Enter SSID
const char* password = "88888888"; //Enter Password
const char* websockets_server_host = "172.16.6.25"; //Enter server adress
const uint16_t websockets_server_port = 8080; // Enter server port
long time_prev = 0;
int nid = 20;
char *token = "20";
JSONVar p;
bool open = false;
bool immdyupdate = false;

using namespace websockets;

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());

    JSONVar v = JSON.parse(message.data());
    if (JSON.typeof(v) == "undefined") {
      return;
    }

    int cmd = (int)v["cmd"];
    if (cmd == 1) {
      open = true;
      immdyupdate = true;
    } else if (cmd == 2) {
      open = false;
      immdyupdate = true;
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

WebsocketsClient client;
void setup() {
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(0, INPUT_PULLUP);
    
    // Connect to wifi
    WiFi.begin(ssid, password);

    // Wait some time to connect to wifi
    for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }

    // run callback when messages are received
    client.onMessage(onMessageCallback);
    
    // run callback when events are occuring
    client.onEvent(onEventsCallback);

    // Connect to server
    client.connect(websockets_server_host, websockets_server_port, "/ws");

    // Send a message
    client.send("{\"dst\": 0, \"src\": 20, \"cmd\": 241, \"payload\": {\"token\": \"20\"}}");

    // Send a ping
    client.ping();
}

void loop() {
    client.poll();

    if (millis()-time_prev > 5000) {
      time_prev = millis();
      immdyupdate = true;
    }

    if (digitalRead(0) == LOW) {
      delay(20);
      if (digitalRead(0) == LOW) {
        open = !open;
        immdyupdate = true;

        while(digitalRead(0) == LOW);
      }
    }

    if (immdyupdate) {
      immdyupdate = false;
      Serial.println("update");
      p["dst"] = 1000;
      p["src"] = nid;
      p["cmd"] = 1; // props
      p["payload"] = JSONVar();
      p["payload"]["open"] = open;
      String jsonString = JSON.stringify(p);
      Serial.println(jsonString);
      client.send(jsonString);
      digitalWrite(LED_BUILTIN, open ?  LOW : HIGH);
    }
}
