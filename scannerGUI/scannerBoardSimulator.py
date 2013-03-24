#!/usr/bin/python
import time
import random
import socket
import collections
import struct

HOST = "127.0.0.1"
PORT = 9930

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


#Protocol parameters
PROT_IDENTIFIER = "GW"
protHeaderFormat = '<2sH'
optHeaderFormat = '<HHHHHBBBBBxH'
dataPayloadFormat = ''
ProtHeader = collections.namedtuple('protHeader', 'protId protLen')
ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
				    stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
				    agcLnaGain agcLna2Gain agcDvgaGain rssiWait')
defaultScanOptions = ScanOptions(startFreqMhz=779, startFreqKhz=0, stopFreqMhz=928, stopFreqKhz=0, \
				freqResolution=203, modFormat=3, activateAGC=1, agcLnaGain=0, \
				agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)
testScanOptions = defaultScanOptions
changeFreqRange=True
changeTimer=time.time()

while True:
    print "Generating data"
    #Randomize the amount of RSSI values that will be generated
    #amtRssiValues = random.randrange(200,2569)
    if changeFreqRange:
        changeFreqRange=False
        startFreq=random.randrange(779,850)
        stopFreq=random.randrange(860,928)
        testScanOptions=ScanOptions(startFreqMhz=startFreq, startFreqKhz=0, stopFreqMhz=stopFreq, stopFreqKhz=0, \
                    freqResolution=203, modFormat=3, activateAGC=1, agcLnaGain=0, \
                    agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)
        amtRssiValues=int((stopFreq-startFreq)/(testScanOptions.freqResolution/1000.0))
        changeTimer=time.time()
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
    #Send the data
    bytesToSend=pkgLen
    print "Sending data: ", amtRssiValues, " RSSI values"
    while bytesToSend>0:
        bytesToSend-=sock.sendto(packed_data, (HOST,PORT))
    print "Data sent"
    time.sleep(0.01)
    if time.time()-changeTimer>5:
        changeFreqRange=True
    