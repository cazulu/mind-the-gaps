#!/usr/bin/python

import tables as tb
import numpy as np
import threading
import time
import collections
    
class H5ScannerThread(threading.Thread):
    '''
    Class that writes the scanning data received by the
    UDP backend into a HDF5-formatted file
    '''
    
    def __init__(self, scanQueue):
        '''
        Constructor
        :param scanQueue: Queue where the scanning data
        is stored as a ScanResults namedtuple (clientAddr,recvOpt,rssiData)
        '''
        self.scanQueue = scanQueue
        self.ScanResults=collections.namedtuple('ScanResult', 'clientAddr recvOpt rssiData')
        self.ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
                    stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
                    agcLnaGain agcLna2Gain agcDvgaGain rssiWait')
        
        threading.Thread.__init__(self)
        self.alive = threading.Event()
        self.alive.set()
        #Open the HDF5 file and create the table to store the data
        #TODO: Append data if file exists instead of overwriting it
        self.h5File=tb.openFile("scanData.h5", mode="w", title="Scan data file")
        self.h5Group = self.h5File.createGroup("/", 'scannerNodes', 'White space detector nodes')
        #Dictionary that stores the (ipAddr, nodeName) pairs
        self.nodeDict={}
        
        self.start()
        
    def run(self):
        '''
        Main loop of the thread, checks if there is any data
        in the queue and stores it in the proper HDF5 table
        '''
        while self.alive.isSet():
            qResult=self.scanQueue.get()
            
            #Check if we received the exit message
            if qResult=='exit':
                self.alive.clear()
                break
            
            scanResults=self.ScanResults(*qResult)
            scanOpt=self.ScanOptions(*scanResults.recvOpt)
            
            #Create the table description based on the scan options
            #TODO: Right now we assume that the scan options will never change!!!!
            scanTableDesc={'ipAddr':tb.StringCol(13),
                           'isAlive':tb.BoolCol(1),
                           'freqStart':tb.Float32Col(1),
                           'freqStop':tb.Float32Col(1),
                           'freqRes':tb.Float32Col(1),
                           'timestamp':tb.Time64Col(),
                           'rssiData':tb.Int32Col(shape=(len(scanResults.rssiData),))}
            
            #Obtain the table associated with the node IP address 
            #or create it if it doesn't exist
            if not self.nodeDict.has_key(str(scanResults.clientAddr)):
                #The new tables will be consecutively named as node1, node2, node3, etc by order of arrival
                tableName="node"+str(len(self.nodeDict)+1)
                self.nodeDict[str(scanResults.clientAddr)]=tableName
                table=self.h5File.createTable(self.h5Group, tableName, scanTableDesc, "Node with the IP " + str(scanResults.clientAddr))
                
            else:
                tableName=self.nodeDict[str(scanResults.clientAddr)]
                table=self.h5File.getNode(self.h5Group, tableName)
                
            #Store the scan data in the table
            row=table.row
            row['ipAddr']=str(scanResults.clientAddr)
            row['isAlive']=True
            row['freqStart']=scanOpt.startFreqMhz + scanOpt.startFreqKhz/1000.0
            row['freqStop']=scanOpt.stopFreqMhz + scanOpt.stopFreqKhz/1000.0
            row['freqRes']=scanOpt.freqResolution
            row['timestamp']=time.time()
            row['rssiData']=scanResults.rssiData
            
            #Save the changes
            row.append()
            table.flush()
        
        #Close the H5 file before exit    
        self.h5File.close()
            
            
    def join(self, timeout=None):
        self.alive.clear()
        