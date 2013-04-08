#!/usr/bin/python

import socket
import select
import struct
import collections
import threading
import Queue
import signal
import numpy as np
from scannerH5Backend import H5ScannerThread

class UdpScannerServer(threading.Thread):
	'''
	Class that starts the receiver socket for the UDP protocol
	and launches the state machine for each new board that 
	communicates with the server
	'''
	
	def __init__(self):
		'''
		Init the UDP server back-end
		'''
		
		#Port and buffer size config
		self.udpPort = 9930
		self.udpBuflen = 8192
		
		#Dictionary that maps each active client IP address to its protocol handler
		#and the state machine queue
		self.clientDict={}
		self.ClientHandler=collections.namedtuple('ClientHandler', 'udpChunkQueue scannerSM')
		#Queue to pass the scan results to the H5 backend
		self.scanDataQueue=Queue.Queue()
		self.h5Thread=H5ScannerThread(self.scanDataQueue)
		
		threading.Thread.__init__(self)
		self.alive = threading.Event()
		self.alive.set()
		
		#Define a custom SIGINT handler to close all the associated threads
		signal.signal(signal.SIGINT, self.sigint_handler)
		
		#Start the thread by default
		self.start()
	
	def run(self):
		self.sock = socket.socket(socket.AF_INET, # Internet
			socket.SOCK_DGRAM) # UDP
		#Bind without specifying IP address to listen in all the interfaces
		self.sock.bind(("", self.udpPort))
		self.sock.setblocking(False)
		
		while self.alive.isSet():
			readable, _, _ = select.select([self.sock],[],[],1)
			if self.sock in readable:
				dataChunk, ipPortTuple=self.sock.recvfrom(self.udpBuflen)
				self.addr=ipPortTuple[0]
				print "Received a UDP packet of",len(dataChunk),"bytes from:", self.addr
				#Check if this is a new client and add it to the dictionary
				#or if the associated state machine timed out
				if not self.clientDict.has_key(self.addr) or not self.clientDict[self.addr].scannerSM.isAlive():
					#New client, start a protocol handler thread
					#and create the communication queue
					udpChunkQueue=Queue.Queue()
					self.clientDict[self.addr]=self.ClientHandler(udpChunkQueue=udpChunkQueue, scannerSM=UdpScannerSM(self.addr, udpChunkQueue, self.scanDataQueue))
				#Process the incoming UDP data
				self.clientDict[self.addr].udpChunkQueue.put(dataChunk)
		
	def join(self, timeout=None):
		self.alive.clear()
		#Close all the state machine handlers and the H5 backend
		self.sigint_handler(None, None)
		threading.Thread.join(self, timeout)
	
	def sigint_handler(self,signum,stack):
		self.alive.clear()
		print "\nCtrl-C detected, closing all the related threads and H5 files"
		for clientAddr in self.clientDict:
			self.clientDict[clientAddr].udpChunkQueue.put('exit')
		#Send the exit message to the H5 thread through the queue
		self.scanDataQueue.put('exit')
		
class UdpScannerSM(threading.Thread):
	'''
	State machine class for the UDP scanning protocol. It processes the chunks of data
	received via UDP and stores them into the Queue that feeds the graphical interface 
	'''
	
	def __init__(self, clientAddr, udpChunkQueue, scanDataQueue):
		self.clientAddr = clientAddr
		self.udpChunkQueue = udpChunkQueue
		self.scanDataQueue = scanDataQueue
		
		#Namedtuple format to input data into the queue of the graphical interface 
		self.ScanResults=collections.namedtuple('ScanResult', 'clientAddr recvOpt rssiData')
		
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
		#If we hear nothing from the board in 5 seconds, we close the state machine
		self.maxSilentWait=5.0
		
		#SM parameters
		self.udpScanState=self.protIdle
		self.udpScanPrevState=self.protIdle
		self.rssiData=np.array([], np.dtype('B'))
		self.protBuffer=bytearray()
		self.newChunk=bytearray()
		self.amtRssiValues=0
		self.msgExpected=False
		
		threading.Thread.__init__(self)
		self.alive = threading.Event()
		self.alive.set()
		self.start()
		
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
			#print "The message protocol ID is:", protId
			if protId==self.protIdentifier:
				#print "The message length is", msgLen, "bytes"
				if msgLen<struct.calcsize(self.optHeaderFormat):
					#print "Erroneous message length: Message length=",msgLen, " OptHeader len=", struct.calcsize(self.optHeaderFormat)
					return self.protFail
				#Get the amount of UINT16 RSSI values that the message contains and initialize the dataPayloadFormat string
				self.amtRssiValues=msgLen-struct.calcsize(self.protHeaderFormat)-struct.calcsize(self.optHeaderFormat)
				if self.amtRssiValues<=0:
					#print "Message erroneous, no RSSI data inside"
					return self.protFail
				self.dataPayloadFormat=str(self.amtRssiValues)+"b"
				return self.protRecvOpt
			else:
				#print "ProtId erroneous, ignoring message"
				return self.protFail
		else:
			self.msgExpected=True
			return self.protRecvHeader
	
	def protRecvOpt(self):
		#print "RECV_OPT state"
		if len(self.protBuffer)>=struct.calcsize(self.optHeaderFormat):
			self.recvScanOptions=self.ScanOptions._make(struct.unpack_from(self.optHeaderFormat, bytes(self.protBuffer)))
			del self.protBuffer[:struct.calcsize(self.optHeaderFormat)]
			#print "**Scan options received**"
			startFreq=float(self.recvScanOptions.startFreqMhz) + self.recvScanOptions.startFreqKhz/1000.0
			stopFreq=float(self.recvScanOptions.stopFreqMhz) + self.recvScanOptions.stopFreqKhz/1000.0
			#print "Start frequency(MHz):", startFreq
			#print "Stop frequency(MHz):", stopFreq
			#print "Frequency resolution(KHz):", self.recvScanOptions.freqResolution
			return self.protRecvData
		else:
			self.msgExpected=True
			return self.protRecvOpt
	
	def protRecvData(self):
		#print "RECV_DATA state"
		if len(self.protBuffer)>=struct.calcsize(self.dataPayloadFormat):
			#print "Unpacking RSSI data"
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
		#print "***Finished receiving packet***\n"
		self.msgExpected=True
		#Pass the data to the graphical front-end as a namedtuple through the queue
		self.scanDataQueue.put(self.ScanResults(clientAddr=self.clientAddr, recvOpt=self.recvScanOptions, rssiData=self.rssiData))
		return self.protIdle
		
		#****SM END****
	
	def run(self):
		'''
		Main handler of the state machine
		:param data: Chunk of UDP data received through the socket
		'''
		while self.alive.isSet():
			#Wait until a new chunk of data arrives or 
			#the timeout condition is fulfilled
			try:
				newDataChunk=self.udpChunkQueue.get(block=True, timeout=self.maxSilentWait)
			except Queue.Empty, e:
				#Inform the H5 backend about the timeout by sending None in place of the rssi array
				self.scanDataQueue.put(self.ScanResults(clientAddr=self.clientAddr, recvOpt=None, rssiData=None))
				self.join()
			else:
				#Check if we received the exit message
				if newDataChunk=='exit':
					self.alive.clear()
					break
				#Save the new data into the buffer
				self.protBuffer.extend(newDataChunk)
				self.msgExpected=False
				while not self.msgExpected:
					#if self.udpScanPrevState!=self.udpScanState:
					#	print "State changed to ", self.udpScanState
					self.udpScanPrevState=self.udpScanState
					self.udpScanState=self.udpScanState()
	
	def join(self, timeout=None):
		self.alive.clear()
#		threading.Thread.join(self, timeout)


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

if __name__ == '__main__':
	print "Starting UDP scanner server backend"
	t=UdpScannerServer()
	while(t.isAlive()):
		pass
	
