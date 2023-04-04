# History

## 2022-12-19

* Enabled IPv4 & IPv6 fragmented IP reassembly (finally eliminated http/lwip client sock=54 errors)
* disable: DHCP: Perform ARP check on any offered address

## 2023-01-02

* disabled gratituous ARP sending on LWIP
* no longer smooth time upates, as that caused problems after restart
* reset UART receive buffer after netif action stops, and wait for TX fifo clearance
* updated to ESP-IDF v4.4.3

## 20230104.1923

* set UART send buffer to 0 for blocking send calls. to ensure this isnt causing TCP/IP connect issues. at least it frees memory

## 20230105.1306

* added quick HTTP client connect failure retry without restarting Modem

## 20230325.1259

* added Maximet %% command and print to log for exact model number
* updated to ESP-IDF v4.4.4

## 20230404.0823

* added Modem power cycling after too many HTTP connectivity issues
* added extended Software restart reason, for diagnostics
