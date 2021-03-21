# Weather Buoy

* ESP32, SIM7600L
* GILL MaxiMet 501GX

## Weather Station GILL Maximet Simulator
* [ESP8266 emulates Maximet weather data over serial](maximetsumulator)



## detailed part list

| Part | Description | Links | Cost (ex. delivery) |
|-|-|-|-|
| Waterproof housing | Fibox PCM 200/100 T 6016927 Universal-Gehäuse 255 x 180 x 100 Polycarbonat Lichtgrau (RAL 7035) | [conrad](https://www.conrad.at/de/p/fibox-pcm-200-100-t-6016927-universal-gehaeuse-255-x-180-x-100-polycarbonat-lichtgrau-ral-7035-1-st-521203.html) [fibox](https://www.fibox.de//catalog/64/product/183/6016927_GER1.html) | EUR 46 |
| Weather station | GILL MaxiMet 501GX |  |  |
| Microcontroller |  ESP32 |   |   |
| LTE/GSM Modem  | LTE/GSM Modem SIM7600L |   |   |
| Solar panel | 18V, 20W | |
| Solar charging controller 12V | LiFePo4 capable, 5V power for microcontroller | |
| RS232 to TLL converter | | |
| Voltage/Current sensor | | |
| Temperature sensor | DS18B20 module | |
| Battery | LiFePo4 Jutec...... | |
| | | |
| | | |
| Weather station simulator | ESP8266 | |
| | | |
| | | |
| | | |
| | | |
| | | |


## wiring

| ESP32 / SimCom board | connection |
|-|-|
| 5V USB | ESP8266 VBat, RS232 to TTL |
| GND | ESP8266 VBAT |
| RX | RS232 to TTL -> Maximet  |
| GPIO 34 | MAX471 voltage 12v / 5x divider = 0..2.4V |
| GPIO 35 | MAX471 current (provided as analog voltage) |

## todo
* change Maximet serial to 

* handle disconnect!!! 
I (86576) esp-netif_lwip-ppp: Connection lost
I (86576) Cellular: IP event! 6
I (86576) Cellular: Cellular Disconnect from PPP Server

## troubleshooting

### cannot flash via serial/USB anymore
* remove serial connection to Maximet (or turn off/reset Maximet/Simulator device)
  https://esp32.com/viewtopic.php?t=1205
### observing weird crashes, especially after changing git branches
* completely delete the build folder and perform a 100% clean build
 https://esp32.com/viewtopic.php?t=1205

 UART0: RX: GPIO3, TX: GPIO1
UART1: RX: GPIO9, TX: GPIO10
UART2: RX: GPIO16, TX: GPIO17