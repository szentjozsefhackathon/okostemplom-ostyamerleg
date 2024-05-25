#define WITH_ETHERNET 0
#define WITH_WIFI 1
#define WITH_MQTT 1

#if WITH_ETHERNET
#include <Ethernet.h>
#define ETHERNET_CS_PIN 2
#elif WITH_WIFI
#include <WiFi.h>
#endif

#if WITH_MQTT
#include <MQTT.h>
#if !WITH_ETHERNET && !WITH_WIFI
#error "MQTT requires either Ethernet or WiFi"
#endif
#endif

#include <HX711.h>

#define TARE_BUTTON_PIN 13
#define LOADCELL_DATA_PIN ESP32 ? 16 : 3
#define LOADCELL_SCK_PIN 4

#define LOADCELL_DEFAULT_OFFSET 41155ul
#define LOADCELL_DEFAULT_DIVIDER 450.f
#define LOADCELL_GAIN_128 128
#define LOADCELL_GAIN_64 64
#define LOADCELL_DEFAULT_GAIN LOADCELL_GAIN_128
#define DEFAULT_DT 1000ul
#define DEFAULT_DM .2f
#include "config.h"

#if WITH_ETHERNET
EthernetClient netClient;
byte mac[] = {0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed};
#elif WITH_WIFI
WiFiClient netClient;
#endif

#if WITH_ETHERNET || WITH_WIFI
String ipStr;
#endif

#if WITH_MQTT
MQTTClient mqtt;

const String MQTT_TOPIC_ROOT = String("scale/");
const char *MQTT_TOPIC_WEIGHT = "/weight";
const char *MQTT_TOPIC_TARE = "/tare";
const char *MQTT_TOPIC_DIVIDER = "/divider";
const char *MQTT_TOPIC_GAIN = "/gain";
const char *MQTT_TOPIC_DELTA_T = "/dt";
const char *MQTT_TOPIC_DELTA_M = "/dm";

void connect();
void messageReceived(String &topic, String &payload);
String mqttTopic(const char *name);
#endif

void tare();
bool tareFlag = false;

#define NO_SCALE -1.f
void setScale(float scale);
float setScaleTo = NO_SCALE;

#define NO_GAIN 255
void setGain(byte gain);
byte setGainTo = NO_GAIN;

void setDeltaT(unsigned long dt);
unsigned long deltaT = DEFAULT_DT;

void setDeltaM(float dm);
float deltaM = DEFAULT_DM;

HX711 loadCell;
unsigned long lastMillis = 0;
float lastWeight = __FLT_MAX__;
byte loadCellGain = LOADCELL_DEFAULT_GAIN;

void setup()
{
  Serial.begin(115200);

#if WITH_ETHERNET
  Serial.print("Initialize Ethernet with DHCP: ");
  Ethernet.init(ETHERNET_CS_PIN);
  if (Ethernet.begin(mac, 15000) == 0)
  {
    Serial.print("failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.print(" (Ethernet shield not found)");
    }
    else if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.print(" (Ethernet cable is not connected)");
    }
    Serial.println();
    while (true)
    {
      delay(1);
    }
  }
  ipStr = Ethernet.localIP().toString();
#elif WITH_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi (%s)...", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  ipStr = WiFi.localIP().toString();
#endif
#if WITH_ETHERNET || WITH_WIFI
  Serial.printf(" IP address is %s\n", ipStr.c_str());
#endif

  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TARE_BUTTON_PIN), tare, RISING);

  loadCell.begin(LOADCELL_DATA_PIN, LOADCELL_SCK_PIN, LOADCELL_DEFAULT_GAIN);
  loadCell.set_scale(LOADCELL_DEFAULT_DIVIDER);
  loadCell.set_offset(LOADCELL_DEFAULT_OFFSET);

#if WITH_MQTT
#ifdef MQTT_IP
#ifdef MQTT_HOST
#error "Both MQTT_IP and MQTT_HOST is defined"
#endif // MQTT_HOST
  mqtt.begin(IPAddress(MQTT_IP), MQTT_PORT, netClient);
#else // MQTT_IP
#ifdef MQTT_HOST
  mqtt.begin(MQTT_HOST, MQTT_PORT, net);
#else // MQTT_HOST
#error "Either MQTT_IP or MQTT_HOST is required"
#endif // MQTT_HOST
#endif // MQTT_IP
  mqtt.onMessage(messageReceived);

  connect();
#endif // WITH_MQTT
}

void loop()
{
#if WITH_MQTT
  mqtt.loop();
  if (!mqtt.connected())
  {
    Serial.println("MQTT disconnected");
    connect();
  }
#endif

  if (tareFlag)
  {
    loadCell.power_up();
    loadCell.tare();
    loadCell.power_down();
    Serial.printf("tare, offset: %ld\n", loadCell.get_offset());
    tareFlag = false;
  }
  if (setScaleTo != NO_SCALE)
  {
    if (setScaleTo <= 0)
    {
      setScaleTo = LOADCELL_DEFAULT_DIVIDER;
      mqtt.publish(mqttTopic(MQTT_TOPIC_DIVIDER), String(setScaleTo));
    }
    loadCell.power_up();
    loadCell.set_scale(setScaleTo);
    loadCell.power_down();
    Serial.printf("scale: %f\n", setScaleTo);
    setScaleTo = NO_SCALE;
  }
  if (setGainTo != NO_GAIN)
  {
    if (setGainTo == LOADCELL_GAIN_64 || setGainTo == LOADCELL_GAIN_128)
    {
      loadCell.power_up();
      loadCell.set_gain(setGainTo);
      loadCell.power_down();
      loadCellGain = setGainTo;
    }
    else
    {
      setGainTo = loadCellGain;
      mqtt.publish(mqttTopic(MQTT_TOPIC_GAIN), String(setGainTo));
    }
    Serial.printf("gain: %d\n", setGainTo);
    setGainTo = NO_GAIN;
  }
  if (deltaT <= 0)
  {
    deltaT = DEFAULT_DT;
    mqtt.publish(mqttTopic(MQTT_TOPIC_DELTA_T), String(deltaT));
  }
  if (deltaM <= 0)
  {
    deltaM = DEFAULT_DM;
    mqtt.publish(mqttTopic(MQTT_TOPIC_DELTA_M), String(deltaM));
  }
  if (millis() - lastMillis > deltaT)
  {
    loadCell.power_up();
    if (loadCell.wait_ready_timeout())
    {
      float weight = loadCell.get_units(10);
      if (fabs(weight - lastWeight) >= deltaM)
      {
        Serial.print("weight: ");
        Serial.println(weight, 2);
#if WITH_MQTT
        mqtt.publish(mqttTopic(MQTT_TOPIC_WEIGHT), String(weight));
#endif
      }
      lastMillis = millis();
      lastWeight = weight;
    }
    else
    {
      Serial.println("Load cell read timeout");
    }
    loadCell.power_down();
  }
}

void tare()
{
  tareFlag = true;
}

void setScale(float scale)
{
  setScaleTo = scale;
}

void setGain(byte gain)
{
  setGainTo = gain;
}

void setDeltaT(unsigned long dt)
{
  deltaT = dt;
}

void setDeltaM(float dm)
{
  deltaM = dm;
}

#if WITH_MQTT
String mqttTopic(const char *name)
{
  return MQTT_TOPIC_ROOT + ipStr + name;
}

void connect()
{
  Serial.print("Connecting to MQTT broker...");
  while (!mqtt.connect(ipStr.c_str(), MQTT_USER, MQTT_PASSWORD))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.printf(" connected to %d.%d.%d.%d\n", MQTT_IP);

  mqtt.publish(mqttTopic(MQTT_TOPIC_DIVIDER), String(loadCell.get_scale()));
  mqtt.publish(mqttTopic(MQTT_TOPIC_GAIN), String(loadCellGain));
  mqtt.publish(mqttTopic(MQTT_TOPIC_DELTA_T), String(deltaT));
  mqtt.publish(mqttTopic(MQTT_TOPIC_DELTA_M), String(deltaM));
  mqtt.subscribe(mqttTopic(MQTT_TOPIC_TARE));
  mqtt.subscribe(mqttTopic(MQTT_TOPIC_DIVIDER));
  mqtt.subscribe(mqttTopic(MQTT_TOPIC_GAIN));
  mqtt.subscribe(mqttTopic(MQTT_TOPIC_DELTA_T));
  mqtt.subscribe(mqttTopic(MQTT_TOPIC_DELTA_M));
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the mqtt instance in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `mqtt.loop()`.

  if (topic.endsWith(MQTT_TOPIC_TARE))
  {
    tare();
  }
  else if (topic.endsWith(MQTT_TOPIC_DIVIDER))
  {
    setScale(payload.toFloat());
  }
  else if (topic.endsWith(MQTT_TOPIC_GAIN))
  {
    setGain((byte)payload.toInt());
  }
  else if (topic.endsWith(MQTT_TOPIC_DELTA_T))
  {
    long dt = payload.toInt();
    setDeltaT(dt < 0 ? 0 : dt);
  }
  else if (topic.endsWith(MQTT_TOPIC_DELTA_M))
  {
    setDeltaM(payload.toFloat());
  }
}
#endif
