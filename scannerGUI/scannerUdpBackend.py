#!/usr/bin/python

import socket
import select
import struct
import collections
import binascii
import threading
import Queue
import signal
import numpy as np
from scannerH5Backend import H5ScannerThread


class UdpScanProt():
	'''
	Class that holds all the scanner protocol variables
	'''
	listenPort=9930
	protId="GW"
	headerFormat='<2sH6s'
	optFormat='<HHHHHBBBBBxH'
	Header=collections.namedtuple('ProtHeader', 'protId protLen macAddr')
	Opt=collections.namedtuple('ScanOptions', 'freqStartMhz freqStartKhz \
									freqStopMhz freqStopKhz freqRes modFormat agcEnabled \
									lnaGain lna2Gain dvgaGain rssiWait')
	defaultOpt = Opt(freqStartMhz=779, freqStartKhz=0, freqStopMhz=928, freqStopKhz=0, \
			     freqRes=203, modFormat=2, agcEnabled=1, lnaGain=0, \
			     lna2Gain=7, dvgaGain=7, rssiWait=1000)
	modFormatDict={'2-FSK':0,
				 	'GFSK':1,
					'ASK':2,
					'OOK':3,
					'4-FSK':4,
					'MSK':5 }
	invModFormatDict={0:'2-FSK',
			 		1:'GFSK',
					2:'ASK',
					3:'OOK',
					4:'4-FSK',
					5:'MSK'}
	
	
	#Limits for the opt values
	minFreq=779
	maxFreq=928
	minLnaGain=0
	maxLnaGain=3
	minLna2Gain=0
	maxLna2Gain=7
	minDvgaGain=0
	maxDvgaGain=7
	
	@staticmethod
	def validate_opt(opt):
		'''
		Return True if the scan options make sense and False if they don't
		:param opt: UdpScanProt.Opt namedtuple to be checked
		'''
		if opt.freqStartMhz<UdpScanProt.minFreq or opt.freqStartKhz>=1000 or opt.freqStartKhz<0 \
			or opt.freqStopMhz>UdpScanProt.maxFreq or opt.freqStopKhz>=1000 or opt.freqStopKhz<0 \
			or not UdpScanProt.invModFormatDict.has_key(opt.modFormat) \
			or opt.agcEnabled<0 or opt.agcEnabled>1 \
			or opt.lnaGain<UdpScanProt.minLnaGain or opt.lnaGain>UdpScanProt.maxLnaGain \
			or opt.lna2Gain<UdpScanProt.minLna2Gain or opt.lna2Gain>UdpScanProt.maxLna2Gain \
			or opt.dvgaGain<UdpScanProt.minDvgaGain or opt.dvgaGain>UdpScanProt.maxDvgaGain:
			return False
		else:
			return True

class UdpScannerServer(threading.Thread):
	'''
	Class that starts the receiver socket for the UDP protocol
	and launches the state machine for each new board that 
	communicates with the server
	'''
	
	def __init__(self, guiActive=False):
		'''
		Init the UDP server back-end
		'''
		
		self.udpBuflen = 8192
		self.sock=None
		
		#Dictionary that maps each active client IP address to its protocol handler
		#and the state machine queue
		self.clientDict={}
		self.ClientHandler=collections.namedtuple('ClientHandler', 'udpChunkQueue scannerSM')
		#Queue to pass the scan results to the H5 backend
		#TODO: Consider switching to a Priority queue to 
		#avoid having to read all the remaining data before the 'exit' string
		self.scanDataQueue=Queue.Queue()
		#Lock used to regulate access to the H5 file
		self.h5FileLock=threading.Lock()
		self.h5Thread=H5ScannerThread(self.scanDataQueue, self.h5FileLock)
		
		threading.Thread.__init__(self)
		self.alive = threading.Event()
		self.alive.set()
		
		#Define a custom SIGINT handler to close all the associated threads
		#if there is no GUI associated to take care of that task
		if not guiActive:
			signal.signal(signal.SIGINT, self.sigint_handler)
		
		#Start the thread by default
		self.start()
	
	def run(self):
		try:
			self.sock = socket.socket(socket.AF_INET, # Internet
				socket.SOCK_DGRAM) # UDP
			#Bind without specifying IP address to listen in all the interfaces
			self.sock.bind(("", UdpScanProt.listenPort))
			self.sock.setblocking(False)
			
			while self.alive.isSet():
				readable, _, _ = select.select([self.sock],[],[],1)
				if self.sock in readable:
					dataChunk, ipPortTuple=self.sock.recvfrom(self.udpBuflen)
					self.addr=ipPortTuple[0]
					#print "Received a UDP packet of",len(dataChunk),"bytes from:", self.addr
					#Check if this is a new client and add it to the dictionary
					#or if the associated state machine timed out
					if not self.clientDict.has_key(self.addr) or not self.clientDict[self.addr].scannerSM.isAlive():
						#New client, start a protocol handler thread
						#and create the communication queue
						udpChunkQueue=Queue.Queue()
						self.clientDict[self.addr]=self.ClientHandler(udpChunkQueue=udpChunkQueue, scannerSM=UdpScannerSM(self.addr, udpChunkQueue, self.scanDataQueue))
					#Process the incoming UDP data
					self.clientDict[self.addr].udpChunkQueue.put(dataChunk)
		finally:
			self.sock.close()
		
	def close_backend(self):
		'''
		Close all threads
		'''
		self.alive.clear()
		for clientAddr in self.clientDict:
			self.clientDict[clientAddr].udpChunkQueue.put('exit')
		#Send the exit message to the H5 thread through the queue
		self.scanDataQueue.put('exit')
	
	def sigint_handler(self,signum,stack):
		print "\nCtrl-C detected, closing the backend threads and the HDF5 file..."
		self.close_backend()
		
class UdpScannerSM(threading.Thread):
	'''
	State machine class for the UDP scanning protocol. It processes the chunks of data
	received via UDP and stores them into the Queue that feeds the graphical interface 
	'''
	
	def __init__(self, clientAddr, udpChunkQueue, scanDataQueue):
		self.ipAddr = clientAddr
		self.macAddr=""
		self.udpChunkQueue = udpChunkQueue
		self.scanDataQueue = scanDataQueue
		
		#Namedtuple format to input data into the queue of the graphical interface 
		self.ScanResults=collections.namedtuple('ScanResult', 'macAddr ipAddr recvOpt rssiData')
		
		#Protocol parameters
		self.recvScanOptions=UdpScanProt.defaultOpt
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
		if len(self.protBuffer)>=struct.calcsize(UdpScanProt.headerFormat):
# 			protId, msgLen, rawMac=struct.unpack_from(UdpScanProt.headerFormat, bytes(self.protBuffer))
			protHeader=UdpScanProt.Header._make(struct.unpack_from(UdpScanProt.headerFormat, bytes(self.protBuffer)))
			#Format the mac address for easier printing
			mac = [binascii.hexlify(x) for x in protHeader.macAddr]
			self.macAddr=":".join(mac)
			del self.protBuffer[:struct.calcsize(UdpScanProt.headerFormat)]
			if protHeader.protId==UdpScanProt.protId:
				if protHeader.protLen<struct.calcsize(UdpScanProt.optFormat):
					return self.protFail
				#Get the amount of UINT16 RSSI values that the message contains and initialize the dataPayloadFormat string
				self.amtRssiValues=protHeader.protLen-struct.calcsize(UdpScanProt.headerFormat)-struct.calcsize(UdpScanProt.optFormat)
				if self.amtRssiValues<=0:
					#Bad message, no RSSI data inside
					return self.protFail
				self.dataPayloadFormat=str(self.amtRssiValues)+"b"
				return self.protRecvOpt
			else:
				#Wrong ProtId, ignoring message
				return self.protFail
		else:
			self.msgExpected=True
			return self.protRecvHeader
	
	def protRecvOpt(self):
		#print "RECV_OPT state"
		if len(self.protBuffer)>=struct.calcsize(UdpScanProt.optFormat):
			self.recvScanOptions=UdpScanProt.Opt._make(struct.unpack_from(UdpScanProt.optFormat, bytes(self.protBuffer)))
			del self.protBuffer[:struct.calcsize(UdpScanProt.optFormat)]
			#Check if the options make sense
			if UdpScanProt.validate_opt(self.recvScanOptions):
				return self.protRecvData
			else:
				return self.protFail
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
			rssiDbmData = [(int(x)-2*74+1)/2.0 for x in rssiByteData]
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
		self.scanDataQueue.put(self.ScanResults(macAddr=self.macAddr, ipAddr=self.ipAddr, recvOpt=self.recvScanOptions, rssiData=self.rssiData))
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
				self.scanDataQueue.put(self.ScanResults(macAddr=self.macAddr, ipAddr=self.ipAddr, recvOpt=None, rssiData=None))
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
	'''
	Thread used to send the current scan options to a board of the grid
	'''
	def __init__(self, sock, boardIpList, scanOpt):
		self.sock=sock
		self.boardIpList=boardIpList
		self.scanOpt=scanOpt
		
		#Protocol parameters
		self.pkgLen=struct.calcsize(UdpScanProt.optFormat)+struct.calcsize(UdpScanProt.headerFormat)
		self.protHeader = UdpScanProt.Header(protId=UdpScanProt.protId, protLen=self.pkgLen, macAddr="000000000000")
		
		threading.Thread.__init__(self)
		self.start()
		
	def run(self):
		#Send the scan options to all the boards in the grid
		for boardIp in self.boardIpList:
			bytesToSend=self.pkgLen
			#Store the header and the options
			packed_data = struct.pack(UdpScanProt.headerFormat, *self.protHeader)
			packed_data += struct.pack(UdpScanProt.optFormat, *self.scanOpt)
			#Send the new options to the microcontroller platform
			while bytesToSend>0:
				bytesToSend-=self.sock.sendto(packed_data, (boardIp, UdpScanProt.listenPort))

if __name__ == '__main__':
	print "Starting UDP scanner server backend"
	t=UdpScannerServer()
	while(t.isAlive()):
		pass
	
