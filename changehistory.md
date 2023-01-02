# History
## 2022-12-19:
* Enabled IPv4 & IPv6 fragmented IP reassembly (finally eliminated http/lwip client sock=54 errors)
* disable: DHCP: Perform ARP check on any offered address

## 2023-01-02:
* disabled gratituous ARP sending on LWIP
* no longer smooth time upates, as that caused problems after restart
* reset UART receive buffer after netif action stops, and wait for TX fifo clearance



