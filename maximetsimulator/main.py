#%sendtofile main.py
from machine import Pin
from time import sleep
import time
from machine import UART
import ds18x20
import onewire

print("Gill Maximet simulator.")

led = Pin(0, Pin.OUT)
uart = UART(1, baudrate=19200)
ow = onewire.OneWire(Pin(12)) # create a OneWire bus on GPIO12
ds = ds18x20.DS18X20(ow)
roms = ds.scan()

def send(data):
    uart.write(data)
    print("UART: '{}'".format(data))
    
def sendline(data):
    send(data + "\r\n")

def serialStartupMessage():
    sendline("MAXIMET GMX501-ESP8266 Simulator V1.00.00")
    sendline("STARTUP: OK")
    sendline("NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH," \
             "PRECIPT,PRECIPI,PRECIPS,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP," \
             "TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK")
    sendline("-,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,MM,MM/H,-,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-," \
             "DEG,-,-,-,DEG,DEG,-,-,-,V,-,-")
    sendline("")
    sendline("<END OF STARTUP MESSAGE>")


def flashled(): 
    sleep(0.2)
    led.off() # note: on/off is reverse!!
    sleep(0.2)
    led.on()
    sleep(0.2)
    led.off()
    sleep(0.2)
    led.on()

def checksum(msg):
    cs = 0
    for c in msg:
        cs ^= ord(c)
        return cs

def readTempAndSend():
    led.off()
    ds.convert_temp()
    time.sleep_ms(750)
    # +004.2
    temp = ds.read_temp(roms[0])
    data = "Q,,000.38,,,000.00,,000.00,,0100,0981.4,1037.3,0982.0,040,+017.6," \
            "{temp:+06.1f},06.15,00000.000,000.000,N,254,0550,00.00,,,1.2," \
            "+010.5,06:47,11:49,16:51,201:+25,17:23,17:59,18:35,-06,+01,+1," \
            ",2018-10-31T13:07:24.9,+12.1,0000,".format(temp=temp)
    sendline("\x02{data}\x03{checksum:02x}".format(data=data, checksum=checksum(data)))
    led.on()
    flashled()
    
serialStartupMessage()
while(True):
    readTempAndSend()
    sleep(9)
