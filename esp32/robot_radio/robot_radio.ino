// Robot radio, ESP32 side (Arduino). One copy runs on each robot's ESP32.
//
// What it does:
//   1. Reads the robot's position lines ("POS x y heading") from the LaunchPad over a serial
//      wire.
//   2. Broadcasts that position to the other robot with ESP-NOW about 5 times a second.
//   3. When the other robot's broadcast arrives, hands it back to the LaunchPad as a line
//      "OTHER id x y heading" (the LaunchPad passes it on to the Buckler display).
//
// ESP-NOW is a Wi-Fi feature that sends small packets straight between ESP32s: no router and
// no pairing. We send to the broadcast address, so any ESP32 running this sketch hears it.
//
// BEFORE YOU UPLOAD to each robot: set ROBOT_ID (1 on one robot, 2 on the other) and the start
// position below.
//
// Wiring (3 wires to the LaunchPad; the ESP32 gets its own power from a USB power bank):
//   LaunchPad pin 2.3 (its TX)  ->  ESP32 GPIO16 (RX2)
//   LaunchPad pin 2.2 (its RX)  <-  ESP32 GPIO17 (TX2)
//   LaunchPad GND               --  ESP32 GND
//
// Bench test with no robots: upload to both ESP32s, open the Serial Monitor on each (115200
// baud, line ending "Newline"), and type   POS 100 200 90   into one. The other one prints
//   OTHER 1 100 200 90   (and sends it on its serial wire too).
//
// Arduino IDE: install "esp32 by Espressif Systems" in the Boards Manager, then pick the board
// "ESP32 Dev Module" (or the SparkFun ESP32 Thing entry).

#include <WiFi.h>
#include <esp_now.h>

// ---- Settings you change per robot -----------------------------------------------------
#define ROBOT_ID 1

// Where this robot starts on the shared map, in mm. Both robots use the SAME map and must
// start facing the same direction. Each robot counts its own position from where it started,
// so this is what lets the two robots agree on where each other is. Example: robot 1 starts at
// (0, 0) and robot 2 starts 600 mm to its right at (600, 0): set robot 1 to 0, 0 and robot 2
// to 600, 0.
#define MY_START_X_MM 0
#define MY_START_Y_MM 0

// ---- Fixed settings ----------------------------------------------------------------------
#define LINK_RX_PIN 16   // ESP32 receives here, from the LaunchPad's pin 2.3
#define LINK_TX_PIN 17   // ESP32 sends here, to the LaunchPad's pin 2.2
#define LINK_BAUD 115200
#define SEND_PERIOD_MS 200

// Status light (the board's own small LED, which you can see from outside the robot):
//   * a short blink once a second  = powered and running this sketch
//   * solid on                     = hearing the other robot right now
// STATUS_LED_PIN is the board's built-in LED when the Arduino board setting defines one; if
// nothing lights, check which pin your board's LED is on (the SparkFun ESP32 Thing's blue LED
// is GPIO 5) and set it here. If it is lit when it should be dark, it is wired active-low:
// swap LED_ON and LED_OFF.
#ifdef LED_BUILTIN
#define STATUS_LED_PIN LED_BUILTIN
#else
#define STATUS_LED_PIN 5
#endif
#define LED_ON HIGH
#define LED_OFF LOW
#define HEARD_HOLD_MS 500   // stay solid this long after each packet from the other robot

// What goes over the air. 8 bytes: small, so it is fast and reliable.
struct __attribute__((packed)) Packet {
  uint8_t id;    // which robot sent it
  int16_t x;     // mm on the shared map
  int16_t y;     // mm on the shared map
  int16_t h;     // heading in degrees
  uint8_t seq;   // counts up by one per packet (to spot lost packets while debugging)
};

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// This robot's latest position, in its own frame (counted from where it started).
static int my_x = 0, my_y = 0, my_h = 0;
static bool have_pos = false;
static uint8_t seq = 0;
static uint32_t last_heard_ms = 0;  // when the other robot's packet last arrived (0 = never)

// The newest packet from the other robot. It is written by the radio callback and read by
// loop(), so access is guarded.
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Packet incoming;
static bool have_incoming = false;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void on_receive(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != (int)sizeof(Packet)) {
    return;  // not one of ours
  }
  Packet p;
  memcpy(&p, data, sizeof p);
  if (p.id == ROBOT_ID) {
    return;  // our own id: ignore (a broadcast should not reach us, but be safe)
  }
  portENTER_CRITICAL(&mux);
  incoming = p;
  have_incoming = true;
  portEXIT_CRITICAL(&mux);
}

// Collects characters into lines and calls handle_line() for each complete one.
static void handle_line(const char *line) {
  int x, y, h;
  if (sscanf(line, "POS %d %d %d", &x, &y, &h) == 3) {
    my_x = x;
    my_y = y;
    my_h = h;
    have_pos = true;
  }
}

struct LineReader {
  char buf[64];
  size_t len = 0;
  void poll(Stream &s) {
    while (s.available()) {
      char c = (char)s.read();
      if (c == '\n') {
        buf[len] = '\0';
        if (len > 0) {
          handle_line(buf);
        }
        len = 0;
      } else if (c != '\r' && len < sizeof buf - 1) {
        buf[len++] = c;
      }
    }
  }
};

static LineReader link_reader;  // lines from the LaunchPad
static LineReader usb_reader;   // lines typed into the Serial Monitor (for bench tests)

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LED_ON);  // lit while starting up
  Serial.begin(115200);
  Serial2.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);

  WiFi.mode(WIFI_STA);  // ESP-NOW needs the Wi-Fi radio on (no network is joined)
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init failed");
    while (true) {
      delay(1000);
    }
  }
  esp_now_register_recv_cb(on_receive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = 0;      // the channel we are already on
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("robot_radio: I am robot %d, start (%d, %d) mm\n", ROBOT_ID, MY_START_X_MM,
                MY_START_Y_MM);
}

void loop() {
  link_reader.poll(Serial2);
  usb_reader.poll(Serial);

  // Broadcast our position, translated onto the shared map.
  static uint32_t last_send = 0;
  if (have_pos && millis() - last_send >= SEND_PERIOD_MS) {
    last_send = millis();
    Packet p;
    p.id = ROBOT_ID;
    p.x = (int16_t)(my_x + MY_START_X_MM);
    p.y = (int16_t)(my_y + MY_START_Y_MM);
    p.h = (int16_t)my_h;
    p.seq = seq++;
    esp_now_send(BROADCAST_MAC, (const uint8_t *)&p, sizeof p);
  }

  // If the other robot's position arrived, translate it back into OUR frame (counted from our
  // own start) and pass it to the LaunchPad.
  Packet in;
  bool got = false;
  portENTER_CRITICAL(&mux);
  if (have_incoming) {
    in = incoming;
    have_incoming = false;
    got = true;
  }
  portEXIT_CRITICAL(&mux);
  if (got) {
    last_heard_ms = millis();
    char line[64];
    snprintf(line, sizeof line, "OTHER %d %d %d %d", in.id, in.x - MY_START_X_MM,
             in.y - MY_START_Y_MM, in.h);
    Serial2.print(line);
    Serial2.print('\n');
    Serial.println(line);
  }

  // Status light: solid while we are hearing the other robot, otherwise a short blink per second.
  bool hearing = last_heard_ms != 0 && millis() - last_heard_ms < HEARD_HOLD_MS;
  bool heartbeat = (millis() % 1000) < 60;
  digitalWrite(STATUS_LED_PIN, (hearing || heartbeat) ? LED_ON : LED_OFF);
}
