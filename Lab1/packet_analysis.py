import dpkt
import struct


f = open('test.pcap','rb')
pcap = dpkt.pcap.Reader(f)

flag = False

ip_begin = -1
ip_end = -1

undecode_ip_data = {}

for _, buf in pcap:
     sll = dpkt.sll2.SLL2(buf)
     eth = dpkt.ethernet.Ethernet(buf)
     if isinstance(sll.data, dpkt.ip.IP):
          ip = sll.data
          
          if isinstance(ip.data, dpkt.udp.UDP):
               udp = ip.data
               
               ip_options_length = len(ip.opts)
               udp_payload_length = len(udp.data)
               udp_data = str(udp.data).encode().decode()
               x_header_length = ip_options_length + udp_payload_length
               undecode_ip_data[udp_data] = x_header_length

ASCII_data = {}
for (udp_data, x_len) in undecode_ip_data.items():
     #print(udp_data)
     if "A" not in udp_data: continue
     num = int(udp_data[6:11])
     ASCII_data[num] = x_len
     if "BEGIN" in udp_data:
          ip_begin = num
     if "END" in udp_data:
          ip_end = num
for i in range(ip_begin, ip_end+1):
     if i not in ASCII_data: continue
     print(chr(ASCII_data[i]), end="")