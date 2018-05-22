$Id: README,v 1.10 2006-01-19 23:34:44 cmn Exp $

iwsniff is a TCP/UDP password sniffer based on decode routines in dsniff.
Credits to Dug Song <dugsong@monkey.org>, author of dsniff.

I wrote this to be able to scan through some old Kismet dump files
for passwords as dsniff fails when there are packets missing
for reassembly of a TCP stream, which is quite frequent when
sniffing wireless traffic.

In case of wireless traffic, the BSSID is printed by default.
This is convinient when scanning through Kismet dump files since 
most access points provides DHCP with 192.168.0.x addresses.

Traffic detection (-t option) makes is easier to detect
if insecure protocols are used since it alerts the user
even if a packet going to a known destination port
is not successfully decoded.


TODO
decode_portmap.c and counterparts is not yet intergrated


Claes M Nyberg <pocpon@fuzzpoint.com>
