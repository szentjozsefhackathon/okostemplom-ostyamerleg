#define WITH_MQTT 0

#if WITH_MQTT
#include <Ethernet.h>
#include <MQTT.h>
#endif

#include <HX711.h>

#include "mqtt_config.h"

#define TARE_BUTTON_PIN PIN_LED_13
#define ETHERNET_CS_PIN PWM_CH2
#define LOADCELL_DATA_PIN PWM_CH3
#define LOADCELL_SCK_PIN PWM_CH4

// TODO: mi legyen az alapértelmezett osztó
#define LOADCELL_DEFAULT_DIVIDER 1000

#if WITH_MQTT
byte mac[] = {0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed};
EthernetClient net;
MQTTClient client;
String ipStr;

const char* MQTT_TOPIC_TARE = "/tare";
const char* MQTT_TOPIC_DIVIDER = "/divider";
const char* MQTT_TOPIC_WEIGHT = "/weight";

void connect();
void messageReceived(String &topic, String &payload);
String mqttTopic(const char *name);
#endif

void tare();
bool tareFlag = false;

#define NO_SCALE -1.0f
void setScale(float scale);
float setScaleTo = NO_SCALE;

HX711 loadCell;
unsigned long lastMillis = 0;

void setup()
{
  Serial.begin(115200);

#if WITH_MQTT
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
  Serial.print("IP address is ");
  IPAddress localIp = Ethernet.localIP();
  ipStr = String(localIp[0]) + String(".") + String(localIp[1]) + String(".") + String(localIp[2]) + String(".") + String(localIp[3]);
  Serial.println(localIp);
#endif

  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TARE_BUTTON_PIN), tare, RISING);

  loadCell.begin(LOADCELL_DATA_PIN, LOADCELL_SCK_PIN);
  loadCell.set_scale(LOADCELL_DEFAULT_DIVIDER);

#if WITH_MQTT
#ifdef MQTT_IP
#ifdef MQTT_HOST
#error "Both MQTT_IP and MQTT_HOST is defined"
#endif // MQTT_HOST
  client.begin(IPAddress(MQTT_IP), MQTT_PORT, net);
#else // MQTT_IP
#ifdef MQTT_HOST
  client.begin(MQTT_HOST, MQTT_PORT, net);
#else // MQTT_HOST
#error "Either MQTT_IP or MQTT_HOST is required"
#endif // MQTT_HOST
#endif // MQTT_IP
  client.onMessage(messageReceived);

  connect();
#endif // WITH_MQTT
}

void loop()
{
#if WITH_MQTT
  client.loop();
  if (!client.connected())
  {
    connect();
  }
#endif

  if (tareFlag)
  {
    loadCell.tare();
    Serial.println("tare");
    tareFlag = false;
  }
  else if (setScaleTo >= 0.0f)
  {
    loadCell.set_scale(setScaleTo);
    Serial.println("scale");
    setScaleTo = NO_SCALE;
  }
  else if (millis() - lastMillis > 1000)
  {
    lastMillis = millis();
    float weight = loadCell.get_units(10);
    Serial.print("weight: ");
    Serial.println(weight, 2);
#if WITH_MQTT
    client.publish(mqttTopic(MQTT_TOPIC_WEIGHT), String(weight));
#endif
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

#if WITH_MQTT
String mqttTopic(const char *name)
{
  return String("scale/") + ipStr + String(name);
}

void connect()
{
  Serial.print("connecting...");
  while (!client.connect(ipStr.c_str(), MQTT_USER, MQTT_PASSWORD))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  client.publish(mqttTopic(MQTT_TOPIC_DIVIDER), String(loadCell.get_scale()));
  client.subscribe(mqttTopic(MQTT_TOPIC_TARE));
  client.subscribe(mqttTopic(MQTT_TOPIC_DIVIDER));
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.

  if (topic.endsWith(String(MQTT_TOPIC_TARE)))
  {
    tare();
  }
  else if (topic.endsWith(String(MQTT_TOPIC_DIVIDER)))
  {
    setScale(payload.toFloat());
  }
}
#endif
