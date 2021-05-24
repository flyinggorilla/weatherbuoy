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
| M12 male front-mount | Solar panel power, 4-pins, 2 pins combined to double current of 4A per pin | [7000-13501-9710050](https://shop.murrelektronik.de/en/Connection-Technology/Flange-Connectors/Signal/M12-male-receptacle-A-cod-front-mount-7000-13501-9710050.html?listtype=search&searchparam=7000-13501-9710050) | EUR 10 |
| M12 female with cable | Solar panel power, 4-pins, 2 pins combined to double current of 4A per pin, soldered to solar panel | [7000-12221-6340200](https://shop.murrelektronik.de/en/Connection-Technology/With-open-ended-wires/Signal/M12-female-0-with-cable-7000-12221-6340200.html?listtype=search&searchparam=7000-12221-6340200&src=search&srchPage=1&perPage=10&pos=1) | EUR 10 |
| M8 male connector | soldered to Water temperature sensor cable | [7000-08401-0000000](https://shop.murrelektronik.de/en/Connection-Technology/Field-wireable/Signal/M8-MALE-0-FIELD-WIREABLE-SOLDER-PINS-7000-08401-0000000.html?listtype=search&searchparam=7000-08401-0000000&src=search&srchPage=1&perPage=10&pos=1) | EUR 11 |
| M8 female front-mount 3-pin | Water temperature sensor | [7000-08571-9700050](https://shop.murrelektronik.de/en/Connection-Technology/Flange-Connectors/Signal/M8-FEMALE-FLANGE-PLUG-A-CODED-FRONT-MOUNT-7000-08571-9700050.html?listtype=search&searchparam=7000-08571-9700050&src=search&srchPage=1&perPage=10&pos=1) | EUR 9 |
| M12 female front-mount 5-pin | RS232 data and 5V power connection to Maximet weatherstation | [7000-13561-9720100](https://shop.murrelektronik.de/en/Connection-Technology/Flange-Connectors/Signal/M12-female-receptacle-A-cod-front-mount-7000-13561-9720100.html?listtype=search&searchparam=7000-13561-9720100&src=search&srchPage=1&perPage=10&pos=1)| EUR 12 |
| M12 male connector cable 5-pin | RS232 data and 5V power connection to Maximet weatherstation with Maximet specific IP67 soldered Connector | [7000-12041-6250300](https://shop.murrelektronik.de/en/Connection-Technology/With-open-ended-wires/Signal/M12-male-0-with-cable-7000-12041-6250300.html?listtype=search&searchparam=7000-12041-6250300&src=search&srchPage=1&perPage=10&pos=1) | EUR 11 |
| Pressure compensation unit | Wiska EVPS - Pressure compensation unit, VentPLUG, plastic, metric | [WISKA EVPS 40 10106593](https://www.wiska.com/de/143/pde/10102369/evps-12.html) |
| | | |
| | | |
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