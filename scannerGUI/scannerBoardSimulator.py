#!/usr/bin/python
import time
import random
import socket
import collections
import struct
import binascii
import argparse
import os
from scannerUdpBackend import UdpScanProt

DFLT_SRC_IP = "127.0.0.1"
DFLT_DST_IP = "127.0.0.1"
DFLT_SRC_MAC = "02:00:00:00:00:01"
SRC_PORT = 60000
DST_PORT = 9930
MIN_FREQ=779.0
MAX_FREQ=928.0
MIN_RES=58
MAX_RES=812
DFLT_RES=203
DFLT_PKT_DELAY=0.1
DFLT_RAND_DELAY=5

#Parse command-line options
parser = argparse.ArgumentParser(description="Simple simulator of a white space detector board, \
sends fake frequency scanning UDP messages using the same data format as the real embedded platform")
group=parser.add_mutually_exclusive_group()
group.add_argument("-f", "--freqRange", help="Frequency range of the scan in Mhz", type=float, nargs=2, metavar="Freq", default=[MIN_FREQ, MAX_FREQ])
group.add_argument("-R", "--randRange", help="Randomize the frequency range of the scan every, "+str(DFLT_RAND_DELAY)+" seconds", action="store_true")
parser.add_argument("-r", "--freqResolution", help="Frequency resolution of the scan in Khz", type=float, default=DFLT_RES, metavar="Freq")
parser.add_argument("-d", "--dstIP", help="IP address of the UDP frequency scanning server", default=DFLT_DST_IP, metavar="dstIP")
parser.add_argument("-s", "--srcIP", help="Source IP address of the UDP frequency scanning packets", default=DFLT_SRC_IP, metavar="srcIP")
parser.add_argument("-m", "--srcMAC", help="Source MAC address of the UDP frequency scanning packets", default=DFLT_SRC_MAC, metavar="srcMAC")
parser.add_argument("-w", "--packetWait", help="Delay between UDP packets in ms", type=int, default=DFLT_PKT_DELAY, metavar="delay")
parser.add_argument("-n", "--packetLimit", help="Number of packets to send", type=int, metavar='pktLim')
args=parser.parse_args()

#Check syntax of parameters
#Import scapy only if there's really a need to spoof a source IP, otherwise use a normal socket
if args.srcIP!=DFLT_SRC_IP:
	if os.getuid()!=0:
		parser.error("Root privileges required to spoof the source IP address of a packet, try running the script again with sudo")
	try:
		from scapy.all import *
		#Compute the mac addr based on the IP if there's no mac address specified
		if args.srcMAC==DFLT_SRC_MAC:
			args.srcMAC="%20:%01:%02X:%02X:%02X:%02X" % struct.unpack("BBBB", args.srcIP)
	except ImportError, e:
		parser.error("Scapy package not installed, required for spoofing source IP address. Try sudo apt-get install python-scapy")
else:
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

startFreq=args.freqRange[0]
stopFreq=args.freqRange[1]
if startFreq >= stopFreq:
	parser.error("Syntax error in the freqRange parameter, freqStart<freqStop")
if startFreq<MIN_FREQ or stopFreq>MAX_FREQ:
	parser.error("Syntax error in the freqRange parameter, acceptable range: ["+str(MIN_FREQ)+"-"+str(MAX_FREQ)+"] Mhz")
	
if args.freqResolution:
	freqResolution=args.freqResolution
	if freqResolution<MIN_RES or freqResolution>MAX_RES:
		parser.error("Syntax error in the freqResolution parameter, acceptable range: ["+str(MIN_RES)+"-"+str(MAX_RES)+"] Khz")
		
if args.randRange:
	randTimer=time.time()

#Protocol parameters
dataPayloadFormat = ''
testScanOpt = UdpScanProt.defaultOpt
pktSent=0

while args.packetLimit==None or pktSent<args.packetLimit:
	print "Generating data"
	#Randomize the frequency range if the --randomizeRange parameter is set
	if args.randRange and time.time()-randTimer>=DFLT_RAND_DELAY:
		startFreq=random.randrange(int(MIN_FREQ), int((MAX_FREQ+MIN_FREQ)/2) -1)
		stopFreq=random.randrange(int((MAX_FREQ+MIN_FREQ)/2 +1),int(MAX_FREQ))
		tesScanOpt=UdpScanProt.Opt(freqStartMhz=startFreq, freqStartKhz=0, freqStopMhz=stopFreq, freqStopKhz=0, \
		            freqRes=203, modFormat=2, activateAGC=1, agcLnaGain=0, \
		            agcLna2Gain=7, agcDvgaGain=7, rssiWait=1000)
		randTimer=time.time()
	amtRssiValues=int((stopFreq-startFreq)/(testScanOpt.freqRes/1000.0))
	dataPayloadFormat=str(amtRssiValues)+"b"
	#Generate the amount of random RSSI values specified by amtRssiValues
	rssiValuesDbm=[random.randrange(-110,-90) for n in range(amtRssiValues)]
	#Transform the RSSI values into the byte with offset notation
	rssiValuesDec=[(x+74)*2 for x in rssiValuesDbm]
	#Generate the packet header according to the scanner protocol format
	pkgLen=struct.calcsize(dataPayloadFormat)+struct.calcsize(UdpScanProt.optFormat)+struct.calcsize(UdpScanProt.headerFormat)
	macStr=args.srcMAC.replace(":", "")
	mac=[]
	j=0
	for i in range(0,6):
		mac.append(binascii.hexlify(macStr[j:j+1]))
		j+=2
	print "mac is ", mac
	protHeader = UdpScanProt.Header(protId=UdpScanProt.protId, protLen=pkgLen, macAddr=mac)
	packed_data = struct.pack(UdpScanProt.headerFormat, *protHeader)
	#Add the default scan options
	packed_data += struct.pack(UdpScanProt.optFormat, *testScanOpt)
	#Add the data payload
	packed_data += struct.pack(dataPayloadFormat, *rssiValuesDec)
	#Send the data using the scapy library if IP source spoofing is needed, otherwise use a normal socket
	if args.srcIP==DFLT_SRC_IP:
		bytesToSend=pkgLen
		print "Sending data: ", amtRssiValues, " RSSI values"
		while bytesToSend>0:
			bytesToSend-=sock.sendto(packed_data, (args.dstIP,DST_PORT))
		print "Data sent"
	else:
		conf.L3socket = L3RawSocket
		ipPkt=IP(src=args.srcIP, dst=args.dstIP)
		udpPkt=UDP(sport=SRC_PORT, dport=DST_PORT)
		pkt=ipPkt/udpPkt/packed_data
		send(pkt)
	pktSent+=1

	time.sleep(args.packetWait/1000.0)  