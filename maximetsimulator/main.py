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
    sendline("NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK")
    sendline("-,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-,DEG,-,-,-,DEG,DEG,-,-,-,V,-,-")
    sendline("")
    sendline("<END OF STARTUP MESSAGE>")

def checksum(msg):
    cs = 0
    for c in msg:
        cs ^= ord(c)
        return cs

def readTempAndSend():
    led.off()
    ds.convert_temp()
    time.sleep_ms(750)
    led.on() # note: on/off is reverse!!
    # +004.2
    temp = ds.read_temp(roms[0])
    data = "Q,168,000.02,213,000,000.00,053,000.05,000,0000,0991.1,1046.2,0991.4," \
           "035,{temp:+06.1f},+007.1,07.42,045,0000,00.00,,,1.2,+014.2,04:55,11:11,17:27," \
           "325:-33,04:22,03:45,03:06,-34,+55,+1,,2021-03-27T21:19:31.7,+04.6,0000,".format(temp=temp)
    sendline("\x02{data}\x03{checksum:02x}".format(data=data, checksum=checksum(data)))
    
serialStartupMessage()
while(True):
    readTempAndSend()
    time.sleep_ms(200) # 750ms is gone for reading temp sensor and extra 250ms for flashing the led
