#!/usr/bin/python

import datetime
import os
import tables as tb
import numpy as np
import threading
import time
    
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
            
            scanResults=qResult
            
            if scanResults.rssiData!=None:
                #Create the table description based on the scan options!
                scanTableDesc={'ipAddr':tb.StringCol(13),
                               'isAlive':tb.BoolCol(1),
                               'freqStart':tb.Float32Col(1),
                               'freqStop':tb.Float32Col(1),
                               'freqRes':tb.Float32Col(1),
                               'modFormat':tb.UInt8Col(1),
                               'agcEnabled':tb.BoolCol(1),
                               'lnaGain':tb.UInt8Col(1),
                               'lna2Gain':tb.UInt8Col(1),
                               'dvgaGain':tb.UInt8Col(1),
                               'rssiWait':tb.UInt32Col(1),
                               'timestamp':tb.Time64Col(1),
                               'rssiData':tb.Float32Col(shape=(len(scanResults.rssiData),)),
                               'rssiMin':tb.Float32Col(shape=(len(scanResults.rssiData),)),
                               'rssiAvg':tb.Float32Col(shape=(len(scanResults.rssiData),)),
                               'rssiMax':tb.Float32Col(shape=(len(scanResults.rssiData),))}
                
                scanOpt=scanResults.recvOpt
            
            
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
                        or table.cols.freqStart[0]!=scanOpt.freqStartMhz+scanOpt.freqStartKhz/1000.0 \
                        or table.cols.freqStop[0]!=scanOpt.freqStopMhz+scanOpt.freqStopKhz/1000.0 \
                        or table.cols.freqRes[0]!=scanOpt.freqRes \
                        or table.cols.modFormat[0]!=scanOpt.modFormat \
                        or table.cols.agcEnabled[0]!=scanOpt.agcEnabled!=0 \
                        or table.cols.lnaGain[0]!=scanOpt.lnaGain \
                        or table.cols.lna2Gain[0]!=scanOpt.lna2Gain \
                        or table.cols.dvgaGain[0]!=scanOpt.dvgaGain \
                        or table.cols.rssiWait[0]!=scanOpt.rssiWait:
                        self.h5File.removeNode(self.h5Group, name="node"+str(nodeNumber), recursive=True)
                        table=self.h5File.createTable(self.h5Group, "node"+str(nodeNumber), scanTableDesc, "Node with the IP " + str(scanResults.clientAddr))
                        self.h5File.flush()                
                
                #If no RSSI data was included in the queue, we assume the board to be inactive
                if scanResults.rssiData==None:
                    for row in table:
                        row['isAlive']=False
                        row.update()
                    table.flush()
                else:
                    #Store the scan options in the table
                    table.row['timestamp']=time.time()
                    table.row['ipAddr']=str(scanResults.clientAddr)
                    table.row['isAlive']=True
                    table.row['freqStart']=scanOpt.freqStartMhz + scanOpt.freqStartKhz/1000.0
                    table.row['freqStop']=scanOpt.freqStopMhz + scanOpt.freqStopKhz/1000.0
                    table.row['freqRes']=scanOpt.freqRes
                    table.row['modFormat']=scanOpt.modFormat
                    table.row['agcEnabled']=scanOpt.agcEnabled!=0
                    table.row['lnaGain']=scanOpt.lnaGain
                    table.row['lna2Gain']=scanOpt.lna2Gain
                    table.row['dvgaGain']=scanOpt.dvgaGain
                    table.row['rssiWait']=scanOpt.rssiWait
                    #Store the scan data and calculate its current max, min and avg
                    table.row['rssiData']=scanResults.rssiData
                    if len(table)>0:
                        table.row['rssiAvg']=np.average(np.vstack((table.cols.rssiAvg[len(table)-1], scanResults.rssiData)), axis=0, weights=[len(table),1])
                        table.row['rssiMin']=np.vstack((table.cols.rssiMin[len(table)-1], scanResults.rssiData)).min(axis=0)
                        table.row['rssiMax']=self.maxData=np.vstack((table.cols.rssiMax[len(table)-1], scanResults.rssiData)).max(axis=0)
                    else:
                        table.row['rssiAvg']=scanResults.rssiData
                        table.row['rssiMin']=scanResults.rssiData
                        table.row['rssiMax']=scanResults.rssiData
                    #Save the changes
                    table.row.append()
                    table.flush()
        
        #Close the H5 file before exit and rename 
        #it to avoid being overwritten if the backend starts again
        with self.h5FileLock:    
            self.h5File.close()
            os.rename("data/newScanData.h5", "data/scanData"+datetime.datetime.now().strftime("_%d-%m-%y_%H-%M")+".h5")
            
            
            
    def join(self, timeout=None):
        self.alive.clear()
        