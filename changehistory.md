# History
## 2022-12-19:
* Enabled IPv4 & IPv6 fragmented IP reassembly (finally eliminated http/lwip client sock=54 errors)
* disable: DHCP: Perform ARP check on any offered address

## 2023-01-02:
* disabled gratituous ARP sending on LWIP
* no longer smooth time upates, as that caused problems after restart
* reset UART receive buffer after netif action stops, and wait for TX fifo clearance

## 20230104.1923:
* set UART send buffer to 0 for blocking send calls. to ensure this isnt causing TCP/IP connect issues. at least it frees memory

## 20230105.1306:
* added quick HTTP client connect failure retry without restarting Modem



