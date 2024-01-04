import socket
import subprocess
import os
import dpkt
import time
import argparse

def packet_analysis(pcap_file):
    ip_begin = -1
    ip_end = -1

    undecode_ip_data = {}

    for _, buf in pcap_file:
        sll = dpkt.sll2.SLL2(buf)
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
    ans = ""
    for i in range(ip_begin+1, ip_end):
        if i not in ASCII_data: 
            print(f'Packet loss! id: {i}')
            continue
        ans += chr(ASCII_data[i]) 
    return ans


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='INP Lab')
    parser.add_argument('--srv', type=int, default=0, help='0: lab | 1: local srv')
    parser.add_argument('--port', type=int, default=10495)
    args = parser.parse_args()
    local_srv = '127.0.0.1'
    lab_srv = 'inp.zoolab.org'
    server_add = (local_srv if args.srv else lab_srv, args.port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    user = input("user: ")
    msg = f'hello {user}'.encode('utf-8')
    sock.sendto(msg, server_add)
    data, addr = sock.recvfrom(1024)
    chals_id = data.decode('utf-8')[3:]
    print(chals_id)

    command = f'sudo tcpdump -ni any -Xxnv udp and port {args.port} -w demo.pcap &'
    os.system(command)
    time.sleep(5)
    print("Send chals_id...")
    sock.sendto(f'chals {chals_id}'.encode('utf-8'), server_add)
    time.sleep(5)
    os.system('sudo pkill tcpdump')
    print("Terminate tcpdump.")
    sock.close()

    print("Analysis packet...")
    f = open('demo.pcap','rb')
    pcap = dpkt.pcap.Reader(f)
    ans = packet_analysis(pcap_file=pcap)
    print(f"verfy {ans}")

    #Send flag again
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect(server_add)
    sock.sendto(f"verfy\t{ans}\n".encode(), server_add)

    data, addr = sock.recvfrom(1024)
    result = data.decode()
    print(result)
    sock.close()
