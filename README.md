# Okostemplom ostyamérleg

### Hasznos

#### Ezeken teszteltük

- Arduino UNO, DUE
- ESP32 (ESP32-WROOM-DA)

#### A működéshez szükséges könyvtárak

- [HX711 Arduino Library](https://github.com/bogde/HX711)
- [arduino-mqtt](https://github.com/256dpi/arduino-mqtt)

#### Leírások

- [HX711 modul](docs/HX711-M.pdf)
- [YZC-133-SCL-5](docs/YZC-133-SCL-5.pdf)

### Konfiguráció

1. config.h létrehozása

	```bash
	cp main/config.h.example main/config.h
	```

2. kapcsolódási mód beállítása

	A [forráskód fájl](main/main.ino) első soraiban adható meg, hogy Etherneten vagy WiFin keresztül kapcsolódunk.

	- WiFi

		```c
		#define WITH_ETHERNET 0
		#define WITH_WIFI 1
		#define WITH_MQTT 1
		```

	- Ethernet

		```c
		#define WITH_ETHERNET 1
		#define WITH_WIFI 0
		#define WITH_MQTT 1
		```
	
	- csak soros port konzolon keresztül tesztelés esetén teljesen kikapcsolható az MQTT támogatás:

		```c
		#define WITH_ETHERNET 0
		#define WITH_WIFI 0
		#define WITH_MQTT 0
		```

3. konfigurációs értékek beállítása (ha `WITH_MQTT == 1`)

	- `WIFI_SSID`: a WiFi hálózat neve*
	- `WIFI_PASSWORD`: a WiFi hálózat jelszava*
	- `MQTT_IP`: az IP cím, amin az MQTT broker elérhető (4 szám, vesszővel elválasztva)
	- `MQTT_PORT`: az MQTT broker portja
	- `MQTT_USER`: MQTT felhasználónév
	- `MQTT_USER`: MQTT jelszó

	> \* ha `WITH_WIFI == 0`, akkor üres string maradhat

4. pin-ek módosítása (ezektől eltérő bekötés esetén)

	```c
	#define ETHERNET_CS_PIN 2 // ha WITH_ETHERNET == 1
	...
	#define TARE_BUTTON_PIN 13
	#define LOADCELL_DATA_PIN ESP32 ? 16 : 3
	#define LOADCELL_SCK_PIN 4
	```

5. alapértelmezett értékek módosítása

	```c
	// kezdeti offset, tare-val nullázható a mért érték
	#define LOADCELL_DEFAULT_OFFSET 41155ul
	// ezzel g-ot mér a használt 5 kg-os mérési tartományú mérlegcella
	#define LOADCELL_DEFAULT_DIVIDER 450.f
	...
	// a mérések között legalább ennyi időnek kell eltelnie
	#define DEFAULT_DT 1000ul
	// a mért értékek között legalább ekkora eltérésnek kell lennie MQTT-n publisheléshez
	#define DEFAULT_DM .2f
	```

### MQTT

A topic-ok felépítése: `scale/IP cím/...`

- `.../weight`: a legutóbbi mért érték 2 tizedes kerekítéssel
- `.../divider`: a mérés osztója (pozitív lebegőpontos szám, 0-val visszaállítható, alapértelmezett: `LOADCELL_DEFAULT_DIVIDER`)
- `.../gain`: a mérő ADC erősítése (128 vagy 64 lehet csak, alapértelmezett: 128)
- `.../dt`: a mérések közötti idő (pozitív egész szám, 0-val visszaállítható, alapértelmezett: `DEFAULT_DT`)
- `.../dm`: a publishelt mérések közötti minimális eltérés (pozitív lebegőpontos szám, 0-val visszaállítható, alapértelmezett: `DEFAULT_DM`)
- `.../tare`: nullázza a mért értéket (payloadtól függetlenül)
