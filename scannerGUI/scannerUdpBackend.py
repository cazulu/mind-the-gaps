#!/usr/bin/python

import socket
import select
import struct
import collections
import threading
import Queue
import wx
import time
import datetime
import array
import numpy as np

class ResultEvent(wx.PyEvent):
	"""Simple event to carry arbitrary result data."""
	def __init__(self, evtId, data):
		"""Init Result Event."""
		wx.PyEvent.__init__(self)
		self.SetEventType(evtId)
		self.data = data


class UdpScannerApp(threading.Thread):
	
	def __init__(self, dataQueue, recvOptQueue, addrQueue, wxObject,evtId):
		#Queue for passing data to the GUI frontend
		self.dataQueue=dataQueue
		self.recvOptQueue=recvOptQueue
		self.addrQueue=addrQueue
		self.wxObject=wxObject
		self.evtId=evtId
		#Port and buffer size config
		self.udpPort = 9930
		self.udpBuflen = 8192
		#Protocol parameters
		self.protIdentifier = "GW"
		self.protHeaderFormat = '<2sH'
		self.optHeaderFormat = '<HHHHHBBBBBxH'
		self.ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
							stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
							agcLnaGain agcLna2Gain agcDvgaGain rssiWait')
		self.defaultScanOptions = self.ScanOptions(startFreqMhz=779, startFreqKhz=0, stopFreqMhz=928, stopFreqKhz=0, \
					       freqResolution=203, modFormat=3, activateAGC=1, agcLnaGain=0, \
					       agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)
		self.recvScanOptions=self.defaultScanOptions
		#The payload format will be defined once we know the full length of the message
		self.dataPayloadFormat=""
		
		#SM parameters
		self.udpScanState=self.protIdle
		self.udpScanPrevState=self.protIdle
		self.rssiData=np.array([], np.dtype('B'))
		self.protBuffer=bytearray()
		self.amtRssiValues=0
		self.msgExpected=False
		
		threading.Thread.__init__(self)
		self.alive = threading.Event()
		self.alive.set()
		self.start()
	
	def run(self):
		self.sock = socket.socket(socket.AF_INET, # Internet
			socket.SOCK_DGRAM) # UDP
		#Bind without specifying IP address to listen in all the interfaces
		self.sock.bind(("", self.udpPort))
		self.sock.setblocking(False)
		
		while self.alive.isSet():
			readable, _, _ = select.select([self.sock],[],[])
			if self.sock in readable:
				data, self.addr=self.sock.recvfrom(self.udpBuflen)
				print "Received a UDP packet of",len(data),"bytes from:", self.addr
				#Process the incoming UDP data
				self.udpScanProt(data)
			
		
	def join(self, timeout=None):
		self.alive.clear()
		threading.Thread.join(self, timeout)
		
	#Define the state machine for the custom UDP protocol
	#****SM Start****
	def protFail(self):
		#print "FAIL state"
		self.protBuffer=bytearray()
		self.msgExpected=True
		return self.protIdle
	
	def protIdle(self):
		#print "IDLE state"
		if len(self.protBuffer)>0:
			self.rssiData=np.array([], np.dtype('B'))
			self.amtRssiValues=0
			return self.protRecvHeader
		else:
			self.msgExpected=True
			return self.protIdle
		
	def protRecvHeader(self):
		#print "RECV_HEADER state"
		if len(self.protBuffer)>=struct.calcsize(self.protHeaderFormat):
			protId, msgLen=struct.unpack_from(self.protHeaderFormat, bytes(self.protBuffer))
			del self.protBuffer[:struct.calcsize(self.protHeaderFormat)]
			print "The message protocol ID is:", protId
			if protId==self.protIdentifier:
				print "The message length is", msgLen, "bytes"
				if msgLen<struct.calcsize(self.optHeaderFormat):
					print "Erroneous message length: Message length=",msgLen, " OptHeader len=", struct.calcsize(self.optHeaderFormat)
					return self.protFail
				#Get the amount of UINT16 RSSI values that the message contains and initialize the dataPayloadFormat string
				self.amtRssiValues=msgLen-struct.calcsize(self.protHeaderFormat)-struct.calcsize(self.optHeaderFormat)
				if self.amtRssiValues<=0:
					print "Message erroneous, no RSSI data inside"
					return self.protFail
				self.dataPayloadFormat=str(self.amtRssiValues)+"b"
				return self.protRecvOpt
			else:
				print "ProtId erroneous, ignoring message"
				return self.protFail
		else:
			self.msgExpected=True
			return self.protRecvHeader
	
	def protRecvOpt(self):
		#print "RECV_OPT state"
		if len(self.protBuffer)>=struct.calcsize(self.optHeaderFormat):
			self.recvScanOptions=self.ScanOptions._make(struct.unpack_from(self.optHeaderFormat, bytes(self.protBuffer)))
			del self.protBuffer[:struct.calcsize(self.optHeaderFormat)]
			print "**Scan options received**"
			startFreq=float(self.recvScanOptions.startFreqMhz) + self.recvScanOptions.startFreqKhz/1000.0
			stopFreq=float(self.recvScanOptions.stopFreqMhz) + self.recvScanOptions.stopFreqKhz/1000.0
			print "Start frequency(MHz):", startFreq
			print "Stop frequency(MHz):", stopFreq
			print "Frequency resolution(KHz):", self.recvScanOptions.freqResolution
			return self.protRecvData
		else:
			self.msgExpected=True
			return self.protRecvOpt
	
	def protRecvData(self):
		#print "RECV_DATA state"
		if len(self.protBuffer)>=struct.calcsize(self.dataPayloadFormat):
			print "Unpacking RSSI data"
			rssiByteData=struct.unpack_from(self.dataPayloadFormat, bytes(self.protBuffer))
			del self.protBuffer[:struct.calcsize(self.dataPayloadFormat)]
			
			#Convert the RSSI data to dBm
			rssiDbmData = [(int(x)-2*74+1)>>1 for x in rssiByteData]
			#rssiDbmData = [x if x>=-120 else -70 for x in rssiDbmData]
			self.rssiData=np.array(rssiDbmData)
			#print "RSSI data: ", self.rssiData
			return self.protRecvDone
		
		else:
			self.msgExpected=True
			return self.protRecvData
	
	def protRecvDone(self):
		#print "RECV_DONE state"
		print "***Finished receiving packet***\n"
		self.msgExpected=True
		#Pass the data to the graphical front-end
		self.dataQueue.put(self.rssiData)
		self.recvOptQueue.put(self.recvScanOptions)
		self.addrQueue.put(self.addr)
		
		wx.PostEvent(self.wxObject, ResultEvent(self.evtId, "New RSSI data received!"))
		return self.protIdle
	
	def udpScanProt(self, data):
		#Save the new data into the buffer
		self.protBuffer.extend(data)
		self.msgExpected=False
		while not self.msgExpected:
			#if self.udpScanPrevState!=self.udpScanState:
			#	print "State changed to ", self.udpScanState
			self.udpScanPrevState=self.udpScanState
			self.udpScanState=self.udpScanState()
		return
	#****SM END****


class UdpScannerClient(threading.Thread):
    
    def __init__(self, scannerAddr, guiScanOptions):
        self.scannerAddr=scannerAddr
        self.scanOptions=guiScanOptions
        
        #Protocol parameters
        self.protIdentifier="GW"
        self.protHeaderFormat = '<2sH'
        self.optHeaderFormat = '<HHHHHBBBBBxH'
        self.ProtHeader = collections.namedtuple('protHeader', 'protId protLen')
        self.pkgLen=struct.calcsize(self.optHeaderFormat)+struct.calcsize(self.protHeaderFormat)
        self.protHeader = self.ProtHeader(protId=self.protIdentifier, protLen=self.pkgLen)
        
        threading.Thread.__init__(self)
        self.start()
    
    def run(self):
        self.sock = socket.socket(socket.AF_INET, # Internet
                                  socket.SOCK_DGRAM) # UDP
        #Disable UDP checksums
        #self.sock.setsockopt(socket.SOL_SOCKET,socket.SO_NO_CHECK,1)
        #Bind without specifying IP address to listen in all the interfaces
        bytesToSend=self.pkgLen
        #Store the header and the options
        packed_data = struct.pack(self.protHeaderFormat, *self.protHeader)
        packed_data += struct.pack(self.optHeaderFormat, *self.scanOptions)
        #Send the new options to the microcontroller platform
        while bytesToSend>0:
            bytesToSend-=self.sock.sendto(packed_data, self.scannerAddr)
        self.sock.close()
        print "\n\n***Options sent***\n\n"

