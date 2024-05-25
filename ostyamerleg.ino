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
bool handlingMessage = false;

void connect();
void messageReceived(String &topic, String &payload);
#endif

HX711 loadCell;
unsigned long lastMillis = 0;

void setup()
{
  Serial.begin(115200);

#if WITH_MQTT
  Serial.print("Initialize Ethernet with DHCP: ");
  Ethernet.init(ETHERNET_CS_PIN);
  if (Ethernet.begin(mac, 2000) == 0)
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
  Serial.println(Ethernet.localIP());
#endif

  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);
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

  if (handlingMessage)
  {
    return;
  }
#endif

  if (digitalRead(TARE_BUTTON_PIN) == LOW)
  {
    loadCell.tare();
    Serial.println("tare");
  }
  else if (millis() - lastMillis > 1000)
  {
    lastMillis = millis();
    float weight = loadCell.get_units(10);
    Serial.print("weight: ");
    Serial.println(weight, 2);
#if WITH_MQTT
    client.publish("/weight", String(weight));
#endif
  }
}

#if WITH_MQTT
void connect()
{
  Serial.print("connecting...");
  IPAddress localIp = Ethernet.localIP();
  String ipStr = String(localIp[0]) + String(".") + String(localIp[1]) + String(".") + String(localIp[2]) + String(".") + String(localIp[3]);
  while (!client.connect(ipStr.c_str(), MQTT_USER, MQTT_PASSWORD))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
  handlingMessage = false;

  client.subscribe("/tare");
  client.subscribe("/divider");
}

void messageReceived(String &topic, String &payload)
{
  handlingMessage = true;
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.

  if (topic.equals("/tare"))
  {
    loadCell.tare();
    Serial.println("tare");
  }
  else if (topic.equals("/divider"))
  {
    loadCell.set_scale(payload.toFloat());
    Serial.println("scale");
  }
  handlingMessage = false;
}
#endif
