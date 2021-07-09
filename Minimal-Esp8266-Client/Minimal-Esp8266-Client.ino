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
int nid = 11264;
char *token = "5dd03d9f486669f7912ff46f96bdf7b62ffb06e9c764f9b971ddc98801b288a5";
JSONVar p;
bool open = false;
bool immdyupdate = false;
const char *url = "ws://api.yqmiot.com/ws";

using namespace websockets;

class Fsm {
public:
  Fsm() {
    this->_state = 0;
    this->_needconnect = false;
    this->_connecttime = 0;
    this->_client.onMessage([this](WebsocketsClient&, WebsocketsMessage msg) {
      this->on_message(msg.data());
    });
    this->_client.onEvent([this](WebsocketsClient&, WebsocketsEvent event, WSInterfaceString data) {
      if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.print(this->_state);
          Serial.println("Connnection Opened");
          this->on_connect();
      } else if(event == WebsocketsEvent::ConnectionClosed) {
          Serial.println("Connnection Closed");
          this->on_close();
      } else if(event == WebsocketsEvent::GotPing) {
          Serial.println("Got a Ping!");
      } else if(event == WebsocketsEvent::GotPong) {
          Serial.println("Got a Pong!");
      }
    });
  }

  void connect() {
    if (this->_state == 0) {
      this->_state = 1;
      this->_needconnect = true;
    }
  }

  void on_close() {
    if (this->_state == 1) {
      this->_connecttime = millis();
    } else if (this->_state == 2) {
      this->_state = 1;
      this->_connecttime = millis();
    }
  }

  void on_message(WSInterfaceString data) {
    Serial.print(data);
    Serial.println();

    JSONVar v = JSON.parse(data);

    if (this->_state == 1) {
      // 等待登陆回包
    } else if (this->_state == 2) {
      if (JSON.typeof(v) == "undefined") {
        return;
      }

      switch ((int)v["cmd"]) {
      case 1:
        open = true;
        immdyupdate = true;
        break;

      case 2:
        open = false;
        immdyupdate = true;
        break;
      }
    }
  }

  void on_connect() {
    if (this->_state == 1) {
      JSONVar v;
      v["dst"] = 0;
      v["src"] = nid;
      v["cmd"] = 241;
      v["payload"] = JSONVar();
      v["payload"]["token"] = token;
      String data = JSON.stringify(v);
      Serial.println(data);
      this->_client.send(data);
      this->_state = 2;
    }
  }

  void poll() {
    this->_client.poll();

    if (this->_connecttime > 0
      && (millis()-this->_connecttime) > 5000) {
      this->_connecttime = 0;
      this->_needconnect = true;
    }

    if (this->_needconnect) {
      this->_needconnect = false;
      bool ret = this->_client.connect(url);
      if (!ret) {
        this->_connecttime = millis();
      }
      Serial.print("connect ");
      Serial.print(ret);
      Serial.println();
    }
  }

  int _state;
  WebsocketsClient _client;
  bool _needconnect;
  long _connecttime;
};

Fsm fsm;

void setup() {
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(0, INPUT_PULLUP);
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Connect to wifi
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);

    // Wait some time to connect to wifi
    for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }

    fsm.connect();
}

void loop() {
  fsm.poll();

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
    if (fsm._state == 2) {
      JSONVar v;
      v["dst"] = 0x40000100;
      v["src"] = nid;
      v["cmd"] = 1;
      v["payload"] = JSONVar();
      v["payload"]["open"] = open;
      String vv = JSON.stringify(v);
      fsm._client.send(vv);
      Serial.println(vv);
    }
    digitalWrite(LED_BUILTIN, open ? LOW : HIGH);
  }

  if (fsm._state != 2) {
    digitalWrite(16, (millis()%500) < 300 ? LOW : HIGH);
  } else {
    digitalWrite(16, HIGH);
  }
}
