from machine import Pin
from time import sleep
import time
from machine import UART
import ds18x20
import onewire

print("Gill Maximet simulator.")

led = Pin(0, Pin.OUT)
uart = UART(2, baudrate=19200)
ow = onewire.OneWire(Pin(15)) # create a OneWire bus on GPIO12
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
    data = "Q,,000.38,,,000.00,,000.00,,0100,0981.4,1037.3,0982.0,040,+017.6," \
            "{temp:+06.1f},06.15,00000.000,000.000,N,254,0550,00.00,,,1.2," \
            "+010.5,06:47,11:49,16:51,201:+25,17:23,17:59,18:35,-06,+01,+1," \
            ",2018-10-31T13:07:24.9,+12.1,0000,".format(temp=temp)
    sendline("\x02{data}\x03{checksum:02x}".format(data=data, checksum=checksum(data)))
    
    
    
STATE_NONE = 0
STATE_DATA = 1
STATE_COMMAND = 2
state = STATE_DATA
#for i in range(0,20):

while(True):
    rx = uart.readline() ## None if no data
    if rx:
        if "\r\n" in rx:
            rx = rx[:-2].decode()        
        elif "\r" in rx:
            rx = rx[:-1].decode()
        print("<<<{}>>>".format(rx))
    
    if state == STATE_DATA:
        if rx == "*":
            state = STATE_COMMAND
            sendline("SETUP MODE")
            print("ENTERING COMMAND STATE")
        else:
            readTempAndSend()
            sleep(0.1)
    
    elif state == STATE_COMMAND:
        if rx:
            if rx.lower() == "exit":
                print("EXITING COMMAND STATE")
                state = STATE_DATA
            elif rx.lower() == "report":
                sendline("REPORT = NODE,DIR,SPEED,CDIR,AVGDIR,AVGSPEED,GDIR,GSPEED,AVGCDIR,WINDSTAT,PRESS,PASL,PSTN,RH,TEMP,DEWPOINT,AH,COMPASSH,SOLARRAD,SOLARHOURS,WCHILL,HEATIDX,AIRDENS,WBTEMP,SUNR,SNOON,SUNS,SUNP,TWIC,TWIN,TWIA,XTILT,YTILT,ZORIENT,USERINF,TIME,VOLT,STATUS,CHECK")
            elif rx.lower() == "units":
                sendline("UNITS = -,DEG,MS,DEG,DEG,MS,DEG,MS,DEG,-,HPA,HPA,HPA,%,C,C,G/M3,DEG,WM2,HRS,C,C,KG/M3,C,-,-,-,DEG,-,-,-,DEG,DEG,-,-,-,V,-,-")
            elif rx.lower() == "stop":
                sendline("Simulator command STOP")
                break
            else:
                sendline("unknown command: {} \r\n use EXIT, REPORT, UNITS (simulator also STOP)".format(rx))
        else:
            sleep(1)