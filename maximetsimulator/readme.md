# Simulate GILL Maximet 501 weather station 

* ESP8266 Huzzah
* DS18B20 temperature sensor

## Preparation

* download micropython 2MB flash latest bin
  * http://micropython.org/download/esp8266/
  * http://docs.micropython.org/en/latest/esp8266/quickref.html
  * https://github.com/goatchurchprime/jupyter_micropython_kernel
* `pip install esptool`
* `esptool.py erase_flash`
* `esptool.py --baud 460800 write_flash --flash_size=detect 0 esp8266-20210202-v1.14.bin`
* `pip install adafruit-ampy`
  * `ampy -p com4 ls`
  * `ampy -p com4 get boot.py boot.py`
  * `ampy -p com4 put main.py`
* `miniterm.py com4 115200`     // comes with pip install pyserial
  * `--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---`
  * to quit in windows CMD prompt (not vscode terminal) use [CTRL +]
  * `python -m serial.tools.miniterm com8 115200` // alternative if miniterm.py is not found

## Wiring

| ESP8266 | DS18B20 cable | DS18B20 | 4.7kOhm |
|-|-|-|-|
| GND | Blue | pin 1 | |
| GPIO 12 | Yellow | pin 2 | to 3.3V |
| 3.3V | Red | pin 3 | to GPIO 12 |

| ESP8266 | connection |
|-|-|
| VBat | battery |
| GND1 | battery |
| GND2 | serial + DS18B20 |
| GPIO 2 | serial TX |
| GPIO 12 | DS18B20/resistor |
| 3.3V | DS18B20/resistor |


| Huzzah32 ESP32 Feature board | connection |
|-|-|
| USB | ESP8266 VBat |
| GND | ESP8266 VBAT |
| RX | ESP8266 serial TX |





## Maximet Serial config

    serialConnection.port = serialport
    serialConnection.baudrate = 19200
    serialConnection.parity = serial.PARITY_NONE
    serialConnection.stopbits = serial.STOPBITS_ONE
    serialConnection.bytesize = serial.EIGHTBITS
    serialConnection.timeout = 2

## maximet data

    NODE, DIR, SPEED, CDIR, CSPEED, PRESS, RH, TEMP, DEWPOINT, SOLARRAD, GPSLOCATION, TIME, VOLT,
    STATUS, CHECK
    ┐Q,021,000.01,090,000.01,1015.3,041,+022.0,+008.5,0000,+50.763004:-001.539898:+3.10,2015-06-
    05T10:19:30.8,+05.1,0004,└ 36
    Where
    ┐ STX
    Q Node Letter
    021 Wind Direction
    000.01 Wind Speed
    090 Corrected Direction
    000.01 GPS Corrected Speed
    1015.3 Pressure
    041 Relative Humidity
    +022.0 Temperature
    +008.5 Dewpoint
    0000 Solar Radiation
    +50.763004 GPS Latitude
    -001.539898 GPS Longitude
    +3.10 GPS Height location
    2015-06-05 Date
    T10:19:30.8 Time
    +05.1 Supply Voltage
    0004 Status
    └ ETX
    36 Checksum
    NOTES:
    <STX> is the Start of String character (ASCII value 2).
    <ETX> is the End of String character (ASCII value 3).
    Checksum, the 2 digit Hex Checksum sum figure is calculated from the Exclusive OR of the bytes between
    (and not including) the STX and ETX characters.

