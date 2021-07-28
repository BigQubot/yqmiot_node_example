#include <Arduino_JSON.h>
#include <Adafruit_NeoPixel.h>

#define PIN        5 // On Trinket or Gemma, suggest changing this to 1

#define NUMPIXELS 100 // Popular NeoPixel ring size

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

uint32_t colors[100] = {0xff};

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

const int XNET_CMD_INVALID = 0; // 无效命令 (invalid)
const int XNET_CMD_RESPONSE = 1; // 回复 (response)
const int XNET_CMD_AUTH = 2; // 认证 (auth)
const int XNET_CMD_CLOSE = 3; // 断开 (close)
const int XNET_CMD_PING = 4; // ping
const int XNET_CMD_PONG = 5; // pong
const int XNET_CMD_STATE_REPORT = 6; // 状态上报 (state report)
const int XNET_CMD_EVENT_REPORT = 7; // 事件上报 (event report)
const int XNET_CMD_CALL = 8; // 服务调用 (service call)

const char* GID_ZERO = "0000000000000000";
const char* YQMIOT_GID = "7ffffffffffe0100";

#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>

const char* ssid = "HomeWifi"; //Enter SSID
const char* password = "88888888"; //Enter Password
const char* websockets_server_host = "172.16.6.25"; //Enter server adress
const uint16_t websockets_server_port = 8080; // Enter server port
long time_prev = 0;
char *nid = "0127da13ae000100";
char *token = "faf36ea2e1f87615";
JSONVar p;
bool open = false;
bool immdyupdate = false;
const char *url = "ws://192.168.31.254:8080/ws";

using namespace websockets;

class Fsm {
public:
  Fsm() {
    this->_state = 0;
    this->_needconnect = false;
    this->_connecttime = 0;
    this->_retry_count = 0;
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
      this->_retry_count = 0;
      this->_connecttime = 0;
    }
  }

  void on_close() {
    if (this->_state == 1) {
      int s = min((1<<this->_retry_count++), 30);
      this->_connecttime = millis() + s * 1000;
      Serial.print("retry ");
      Serial.print(s);
      Serial.println();
    } else if (this->_state == 2) {
      this->_state = 1;
      this->_retry_count = 0;
      this->_needconnect = true;
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

      if ((int)v["cmd"] == XNET_CMD_CALL) {
        JSONVar payload = v["payload"];
        if (String((const char*)payload["name"]) == "setpixel") {
          int index = (int)payload["index"];
          uint32_t color = (int)payload["color"];
          colors[index] = color & 0xffffff;
          Serial.print(color);
          Serial.println();
        }
      }
    }
  }

  void on_connect() {
    if (this->_state == 1) {
      JSONVar v;
      v["dst"] = "0000000000000000";
      v["src"] = nid;
      v["cmd"] = 241;
      v["payload"] = JSONVar();
      v["payload"]["nid"] = 0;
      v["payload"]["nid"] = nid;
      v["payload"]["token"] = token;
      String data = JSON.stringify(v);
      Serial.println(data);
      this->_client.send(data);
      this->_state = 2;
    }
  }

  void poll() {
    this->_client.poll();

    // 连接倒计时
    if (this->_connecttime > 0
      && this->_connecttime <= millis()) {
      this->_connecttime = 0;
      this->_needconnect = true;
    }

    // 立即连接
    if (this->_needconnect) {
      this->_needconnect = false;
      bool ret = this->_client.connect(url);
      if (!ret) {
        int s = min((1<<this->_retry_count++), 30);
        this->_connecttime = millis() + s * 1000;
        Serial.print("retry ");
        Serial.print(s);
        Serial.println();
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
  int _retry_count;
};

Fsm fsm;

void setup() {
    Serial.begin(115200);

    pixels.begin();

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

    // 启动网络连接
    fsm.connect();
}

void loop() {
  fsm.poll();

  // 定时触发立即上报
  if (millis()-time_prev > 5000) {
    time_prev = millis();
    immdyupdate = true;
  }

  // 处理按键，切换led，并立即上报
  if (digitalRead(0) == LOW) {
    delay(20);
    if (digitalRead(0) == LOW) {
      open = !open;
      immdyupdate = true;
      while(digitalRead(0) == LOW);
    }
  }

  // 数据上报
  if (immdyupdate) {
    immdyupdate = false;
    if (fsm._state == 2) {
      JSONVar v;
      v["dst"] = "7ffffffffffe0100";
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

  // 同步led
  if (fsm._state != 2) {
    digitalWrite(16, (millis()%500) < 300 ? LOW : HIGH);
  } else {
    digitalWrite(16, HIGH);
  }

  pixels.clear();
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, colors[i]);
  }
  pixels.show();
}
