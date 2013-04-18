#!/usr/bin/python

import os
import datetime
import tables as tb
import threading
import time
import collections
    
class H5ScannerThread(threading.Thread):
    '''
    Class that writes the scanning data received by the
    UDP backend into a HDF5-formatted file
    '''
    
    def __init__(self, scanQueue, h5FileLock):
        '''
        Constructor
        :param scanQueue: Queue where the scanning data
        is stored as a ScanResults namedtuple (clientAddr,recvOpt,rssiData)
        :param h5FileLock: Lock that regulates access to the HDF5 scan data file
        '''
        self.scanQueue = scanQueue
        self.h5FileLock=h5FileLock
        self.ScanResults=collections.namedtuple('ScanResult', 'clientAddr recvOpt rssiData')
        self.ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
                    stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
                    agcLnaGain agcLna2Gain agcDvgaGain rssiWait')
        
        threading.Thread.__init__(self)
        self.alive = threading.Event()
        self.alive.set()
        
        #Check if the data folder exists and create it otherwise
        if not os.path.exists("data/"):
            os.makedirs("data/")
        
        #Open the HDF5 file and create the table to store the data
        with self.h5FileLock:
            self.h5File=tb.openFile("data/newScanData.h5", mode="w", title="Scan data file", complevel=1, complib="lzo")
            self.h5Group = self.h5File.createGroup("/", 'scannerNodes', 'White space detector nodes')
        
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
            
            if scanResults.rssiData!=None:
                #Create the table description based on the scan options!
                scanTableDesc={'ipAddr':tb.StringCol(13),
                               'isAlive':tb.BoolCol(1),
                               'freqStart':tb.Float32Col(1),
                               'freqStop':tb.Float32Col(1),
                               'freqRes':tb.Float32Col(1),
                               'timestamp':tb.Time64Col(1),
                               'rssiData':tb.Int32Col(shape=(len(scanResults.rssiData),))}
                
                scanOpt=self.ScanOptions(*scanResults.recvOpt)
            
            
            #Obtain the table associated with the node IP address 
            #or create it if it doesn't exist
            with self.h5FileLock:
                self.tableFound=False
                nodeNumber=0
                for table in self.h5File.root.scannerNodes:
                    nodeNumber+=1
                    if len(table)>0 and table.cols.ipAddr[0]==scanResults.clientAddr:
                        self.tableFound=True
                        break
                
                if not self.tableFound:
                    #The new tables will be consecutively named as node1, node2, node3, etc by order of arrival
                    tableName="node"+str(nodeNumber+1)
                    table=self.h5File.createTable(self.h5Group, tableName, scanTableDesc, "Node with the IP " + str(scanResults.clientAddr), expectedrows=65536)
                    
                else:
                    #Reset the node if it was inactive in the previous iteration
                    #or if the scan options have changed
                    if table.nrows>0 and not table.cols.isAlive[0] \
                        or table.cols.freqStart[0]!=(scanOpt.startFreqMhz+scanOpt.startFreqKhz/1000.0) \
                        or table.cols.freqStop[0]!=(scanOpt.stopFreqMhz+scanOpt.stopFreqKhz/1000.0) \
                        or table.cols.freqRes[0]!=(scanOpt.freqResolution):
                        table.remove()
                        table=self.h5File.createTable(self.h5Group, tableName, scanTableDesc, "Node with the IP " + str(scanResults.clientAddr))
                
                
                #If no RSSI data was included in the queue, we assume the board to be inactive
                if scanResults.rssiData==None:
                    table.modifyColumn(start=0, stop=None, step=1, column=False, colname='isAlive')
                else:
                    #Store the scan data in the table
                    table.row['ipAddr']=str(scanResults.clientAddr)
                    table.row['isAlive']=True
                    table.row['freqStart']=scanOpt.startFreqMhz + scanOpt.startFreqKhz/1000.0
                    table.row['freqStop']=scanOpt.stopFreqMhz + scanOpt.stopFreqKhz/1000.0
                    table.row['freqRes']=scanOpt.freqResolution
                    table.row['timestamp']=time.time()
                    table.row['rssiData']=scanResults.rssiData
                    table.row.append()
                    table.flush()
        
        #Close the H5 file before exit and rename 
        #it to avoid being overwritten if the backend starts again
        with self.h5FileLock:    
            self.h5File.close()
            os.rename("data/newScanData.h5", "data/scanData"+datetime.datetime.now().strftime("_%d-%m-%y_%H-%M")+".h5")
            
            
            
    def join(self, timeout=None):
        self.alive.clear()
        