#!/usr/bin/python
import time
import random
import socket
import collections
import struct
import argparse
import scapy
from scapy.all import *

DFLT_SRC_IP = "127.0.0.1"
DFLT_DST_IP = "127.0.0.1"
SRC_PORT = 9930
DST_PORT = 9930
MIN_FREQ=779.0
MAX_FREQ=928.0
MIN_RES=58
MAX_RES=812
DFLT_RES=203
DFLT_PKT_DELAY=0.01
DFLT_RAND_DELAY=5

#Parse command-line options
parser = argparse.ArgumentParser(description="Simple simulator of a white space detector board, \
sends fake frequency scanning UDP messages using the same data format as the real embedded platform")
group=parser.add_mutually_exclusive_group()
group.add_argument("-f", "--freqRange", help="Frequency range of the scan in Mhz", type=float, nargs=2, metavar="f1 f2", default=[MIN_FREQ, MAX_FREQ])
group.add_argument("-R", "--randRange", help="Randomize the frequency range of the scan every, "+str(DFLT_RAND_DELAY)+" seconds", action="store_true")
parser.add_argument("-r", "--freqResolution", help="Frequency resolution of the scan in Khz", type=float, default=DFLT_RES)
parser.add_argument("-d", "--dstIP", help="IP address of the UDP frequency scanning server", default=DFLT_DST_IP)
parser.add_argument("-s", "--srcIP", help="Source IP address of the UDP frequency scanning packets", default=DFLT_SRC_IP)
parser.add_argument("-w", "--packetWait", help="Delay between UDP packets in ms", type=int, default=DFLT_PKT_DELAY)
args=parser.parse_args()

#Check syntax of parameters
if args.freqRange:
	startFreq=args.freqRange[0]
	stopFreq=args.freqRange[1]
	if startFreq >= stopFreq:
		parser.error("Syntax error in the freqRange parameter, freqStart<freqStop")
	if startFreq<MIN_FREQ or stopFreq>MAX_FREQ:
		parser.error("Syntax error in the freqRange parameter, acceptable range: [", MIN_FREQ, "-", MAX_FREQ, "]")
	
if args.freqResolution:
	freqResolution=args.freqResolution
	if freqResolution<MIN_RES or freqResolution>MAX_RES:
		parser.error("Syntax error in the freqResolution parameter, acceptable range: [", MIN_RES, "-", MAX_RES, "]")
		
if args.randRange:
	randTimer=time.time()

#Protocol parameters
PROT_IDENTIFIER = "GW"
protHeaderFormat = '<2sH'
optHeaderFormat = '<HHHHHBBBBBxH'
dataPayloadFormat = ''
ProtHeader = collections.namedtuple('protHeader', 'protId protLen')
ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
				    stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
				    agcLnaGain agcLna2Gain agcDvgaGain rssiWait')

testScanOptions = ScanOptions(startFreqMhz=int(startFreq), startFreqKhz=int((startFreq-int(startFreq))*1000), \
				stopFreqMhz=int(stopFreq), stopFreqKhz=int((stopFreq-int(stopFreq))*1000), \
				freqResolution=freqResolution, modFormat=3, activateAGC=1, agcLnaGain=0, \
				agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)

while True:
	print "Generating data"
	#Randomize the frequency range if the --randomizeRange parameter is set
	if args.randRange and time.time()-randTimer>=DFLT_RAND_DELAY:
	    startFreq=random.randrange(int(MIN_FREQ), int((MAX_FREQ+MIN_FREQ)/2) -1)
	    stopFreq=random.randrange(int((MAX_FREQ+MIN_FREQ)/2 +1),int(MAX_FREQ))
	    testScanOptions=ScanOptions(startFreqMhz=startFreq, startFreqKhz=0, stopFreqMhz=stopFreq, stopFreqKhz=0, \
	                freqResolution=203, modFormat=3, activateAGC=1, agcLnaGain=0, \
	                agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)
	    randTimer=time.time()
	amtRssiValues=int((stopFreq-startFreq)/(testScanOptions.freqResolution/1000.0))
	dataPayloadFormat=str(amtRssiValues)+"b"
	#Generate the amount of random RSSI values specified by amtRssiValues
	rssiValuesDbm=[random.randrange(-110,-90) for n in range(amtRssiValues)]
	#Transform the RSSI values into the byte with offset notation
	rssiValuesDec=[(x+74)*2 for x in rssiValuesDbm]
	#Generate the packet header according to the scanner protocol format
	pkgLen=struct.calcsize(dataPayloadFormat)+struct.calcsize(optHeaderFormat)+struct.calcsize(protHeaderFormat)
	protHeader = ProtHeader(protId=PROT_IDENTIFIER, protLen=pkgLen)
	packed_data = struct.pack(protHeaderFormat, protHeader.protId, protHeader.protLen)
	#Add the default scan options
	packed_data += struct.pack(optHeaderFormat, *testScanOptions)
	#Add the data payload
	packed_data += struct.pack(dataPayloadFormat, *rssiValuesDec)
	#Send the data using the scapy library to spoof the source address
	print "Sending data: ", amtRssiValues, " RSSI values"
	conf.L3socket = L3RawSocket
	ipPkt=IP(src=args.srcIP, dst=args.dstIP)
	udpPkt=UDP(sport=SRC_PORT, dport=DST_PORT)
	pkt=ipPkt/udpPkt/packed_data
	send(pkt)
	print "Data sent"
	time.sleep(args.packetWait/1000.0)
    