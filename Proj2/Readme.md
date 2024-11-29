Part2 
notes:restart network :systemctl restart networking
ex1-

ifconfig eth1 up -create network
ifconfig eth1 <ip> -add ip
                172.16.60.1/24 tux63
                172.16.60.254/24 tux64
ifconfig eth1 - can see mac addr "ether XX:XX:XX:XX:XX:XX "
--
then ping tux64 IP when using tux63 or
          tux63 IP when using tux64
with command: ping <IP> 
--
route -n gives us : 172.16.60.0(Destination) 255.255.255.0(Gen/net mask(interface)everything else 0 or 0.0.0.0

doing arp -a shows : ? (172.16.60.1) at 00:c0:df:25:40:66 [ether] on eth1
which is the address of the tux63 and the arp -a was done on tux64
--
deleting arp entry : arp -d <ip>
--
wireshark 
when we delete the arp table entry 
when i ping the other machine it will ask it's mac address 
since the ip can be changed but the mac is a fixed address

the arp table is a table where i can see other ip's and their mac's and in which interface

so  receiving Ethernet frame is ARP when i dont know the mac address of an ip im trying to ping and it lets me know which is the mac that i should ping
 receiving Ethernet frame is ICMP when its a ping


EX2
