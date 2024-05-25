#include <Ethernet.h>
#include <HX711.h>
#include <MQTT.h>

#include "mqtt_config.h"

#define TARE_BUTTON_PIN PIN_LED_13
#define ETHERNET_CS_PIN PWM_CH2
#define LOADCELL_DATA_PIN PWM_CH3
#define LOADCELL_SCK_PIN PWM_CH4

// TODO: mi legyen az alapértelmezett osztó
#define LOADCELL_DEFAULT_DIVIDER 5895655

byte mac[] = {0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed};

EthernetClient net;
MQTTClient client;
HX711 loadCell;
unsigned long lastMillis = 0;

void connect();
void messageReceived(String &topic, String &payload);

void setup()
{
  Serial.begin(115200);

  Serial.println("Initialize Ethernet with DHCP: ");
  Ethernet.init(ETHERNET_CS_PIN);
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println("Ethernet shield not found");
    }
    else if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println("Ethernet cable is not connected");
    }
    while (true)
    {
      delay(1);
    }
  }
  Serial.print("IP address is ");
  Serial.println(Ethernet.localIP());

  pinMode(TARE_BUTTON_PIN, INPUT_PULLUP);
  loadCell.begin(LOADCELL_DATA_PIN, LOADCELL_SCK_PIN);
  loadCell.set_scale(LOADCELL_DEFAULT_DIVIDER);

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
}

void loop()
{
  client.loop();
  if (!client.connected())
  {
    connect();
  }

  if (digitalRead(TARE_BUTTON_PIN) == LOW)
  {
    loadCell.tare();
  }
  else if (millis() - lastMillis > 1000)
  {
    lastMillis = millis();
    float weight = loadCell.get_units(10);
    Serial.print("weight: ");
    Serial.println(weight, 2);
    client.publish("/weight", String(weight));
  }
}

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

  client.subscribe("/tare");
  client.subscribe("/divider");
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.

  if (topic.compareTo("/tare") == 0)
  {
    loadCell.tare();
    Serial.println("tare");
  }
  else if (topic.compareTo("/divider") == 0)
  {
    loadCell.set_scale(payload.toFloat());
    Serial.println("scale");
  }
  else
  {
    return;
  }
  client.publish("/weight", nullptr);
}