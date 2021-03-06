#!/usr/bin/python

import os
import sys
import signal
import subprocess
import tables
import wx
import wx.lib.agw.aui as aui
import wx.lib.agw.floatspin as FS
import wx.lib.agw.ultimatelistctrl as ULC
import wx.lib.customtreectrl as CT
import datetime
from scannerUdpBackend import UdpScannerServer, UdpScannerClient, UdpScanProt

# Use wx with matplotlib with the WXAgg backend. 
import matplotlib
matplotlib.use('WXAgg')
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import \
    FigureCanvasWxAgg as FigureCanvas, \
    NavigationToolbar2WxAgg as NavigationToolbar
import numpy as np

class ScanPlotPanel(wx.Panel):
    """
    Resizable panel to represent the scan data of a single board
    """
    def __init__(self, parent):
        wx.Panel.__init__(self, parent, id=wx.ID_ANY)
        
        #Initialize the plot options with dummy values
        self.maxData = np.arange(779, 928)
        self.avgData = np.arange(779, 928)
        self.minData = np.arange(779, 928)
        self.freqStart=779
        self.freqStop=928
        self.freqRes=203
        #Index of the last RSSI array processed
        self.rssiIndex=0
        #IP of the current scanner node associated with this plot
        self.ipAddr=""
        
        self.init_plot()
        self.draw_plot()

        #Bind the resize event to the resizing function to redraw the plot
        self.Bind(wx.EVT_SIZE, self.on_resize)
        #Create the bool and the timer that control the redrawing of the plot
        self.redrawNeeded=False
        self.redrawTimer=wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_redraw_timer, self.redrawTimer)
        self.redrawTimer.Start(500)
        
    def init_plot(self):
        #self.dpi = 200
        self.fig = Figure(None, None)
        self.fig.subplots_adjust(left=0.075, bottom=0.1, right=0.925)
        self.canvas = FigureCanvas(self, -1, self.fig)
        
        #Set the figure and canvas background color
        rgbtuple=wx.NamedColour("white")
        clr = [c/255. for c in rgbtuple]
        self.fig.set_facecolor(clr)
        self.fig.set_edgecolor(clr)
        self.canvas.SetBackgroundColour(wx.Colour(*rgbtuple))

        self.axes = self.fig.add_subplot(111)
        self.axes.tick_params(axis='both', labelsize='small')
        self.axes.set_axis_bgcolor('white')
        self.axes.set_title("Dummy data, no boards found yet", size='medium')
        self.axes.set_ylabel("RSSI(dBm)", size='medium', labelpad=10)
        self.axes.set_xlabel("Frequency(MHz)", size='medium', labelpad=10)
        
        # Plot the data as a line series, and save a reference to it
        self.plot_maxData = self.axes.plot(
            self.maxData, 
            linewidth=1,
            color='r',
            )[0]
        self.plot_avgData = self.axes.plot(
            self.avgData, 
            linewidth=1,
            color='g',
            )[0]
        self.plot_minData = self.axes.plot(
            self.minData, 
            linewidth=1,
            color='b',
            )[0]
        
        self.axes.legend(('Max', 'Avg', 'Min'), 'lower right', shadow=False, fontsize='small', frameon=True)
        
#        self.toolbar=NavigationToolbar(self.canvas)
#        self.toolbar.Realize()

    def draw_plot(self):
        """ 
        Redraws the plot
        """
        
        ymin = round(self.minData.min()) - 1
        ymax = round(self.maxData.max()) + 1

#        if self.cb_yAutoRange.IsChecked():
#            ymin = round(min(self.minData)) - 1
#            ymax = round(max(self.maxData)) + 1
#        else:
#            ymin = self.yRange_control.yMin
#            ymax = self.yRange_control.yMax

        self.axes.set_ybound(lower=ymin, upper=ymax)
        
        
#        if self.cb_grid.IsChecked():
#            self.axes.grid(True, color='gray')
#        else:
#            self.axes.grid(False)

        freqValues=np.linspace(self.freqStart, self.freqStop, len(self.minData))
        
        self.axes.set_xbound(lower=self.freqStart, upper=self.freqStop)
        
        self.plot_maxData.set_xdata(freqValues)
        self.plot_maxData.set_ydata(self.maxData)
        
        self.plot_avgData.set_xdata(freqValues)
        self.plot_avgData.set_ydata(self.avgData)
        
        self.plot_minData.set_xdata(freqValues)
        self.plot_minData.set_ydata(self.minData)
        
        self.canvas.draw()
        
    def update_plot(self, h5table):
        '''
        Update the plot with the information contained in
        the h5table passed as a parameter
        :param h5table: HDF5 data table identifier, see the H5Backend code
        for details on the table fields
        '''
        self.maxData=h5table.cols.rssiMax[len(h5table)-1]
        self.avgData=h5table.cols.rssiAvg[len(h5table)-1]
        self.minData=h5table.cols.rssiMin[len(h5table)-1]
        self.freqStart=h5table.cols.freqStart[0]
        self.freqStop=h5table.cols.freqStop[0]
        self.freqRes=h5table.cols.freqRes[0]
        self.axes.set_title("Scan results from the detector with the MAC "+h5table.cols.macAddr[0]+" as of "+
                    datetime.datetime.fromtimestamp(h5table.cols.timestamp[len(h5table)-1]).strftime('%c'), size='medium')
        self.redrawNeeded=True
        
    def on_redraw_timer(self, evt):
        '''
        Function triggered by the redraw timer
        :param evt: wx.EVT_TIMER
        '''
        if self.redrawNeeded:
            self.draw_plot()
            self.redrawNeeded=False
            
    def on_resize(self, evt):
        '''
        Function triggered by a resizing event
        :param evt: wx.EVT_SIZE
        '''
        self.set_size(evt.GetSize())

    def set_size(self, size):
        '''
        Adapt the size of the figure to that of its container panel
        :param size: Size of the panel in which the figure is embedded
        '''
        pixels = tuple(size)
        #print "Resizing image to ", pixels
        self.SetSize(pixels)
        self.canvas.SetSize(pixels)
        self.fig.set_size_inches(float(pixels[0])/self.fig.get_dpi(),
                                 float(pixels[1])/self.fig.get_dpi())
            
    def print_figure(self, path):
        '''
        Call the print_figure method of the FigureCanvas to store the image in a PNG file
        :param path: Full path of the file where the plot will be saved
        '''
        self.canvas.print_figure(path, dpi=self.fig.get_dpi())
            
class ScannerGUI(wx.Frame):
    def __init__(self, parent):
        wx.Frame.__init__(self,
                          parent,
                          id=wx.ID_ANY,
                          pos=wx.DefaultPosition,
                          size=wx.Size( -1,-1 ),
                          title="Scanner GUI",
                          style=wx.DEFAULT_FRAME_STYLE|wx.TAB_TRAVERSAL)
        
        self.create_menu()
        self.create_toolbar()
        self.statusbar=self.CreateStatusBar()
        self.create_boardList()
        self.create_settingsTree()
        self.scanPlot = ScanPlotPanel(self)
        
        self.mgr=aui.AuiManager(self)
        
        self.mgr.AddPane(self.boardList, aui.AuiPaneInfo().Left().Position(0).Caption("Board list").
                         MinimizeButton(True).MaximizeButton(True).CloseButton(False))
        self.mgr.AddPane(self.settingsTree, aui.AuiPaneInfo().Left().Position(1).Caption("Scan settings").
                         MinimizeButton(True).MaximizeButton(True).CloseButton(False))
        self.mgr.AddPane(self.scanPlot, aui.AuiPaneInfo().Center().Caption("Scan plot").
                         MinimizeButton(True).MaximizeButton(True).CloseButton(False))
        self.mgr.AddPane(self.toolbar, aui.AuiPaneInfo().Top().ToolbarPane())
        self.Maximize()
        
        self.mgr.Update()
        
        #print self.mgr.SavePerspective()
        
        #IP of the board currently displayed on the FigureCanvas
        self.macPlottedBoard=None
        #Flag to force a replot even if there's no new data in the HDF5 file
        self.replotRequested=False
        #Timestamp of the last data plotted in the FigureCanvas
        self.plottedDataTimestamp=None
        
        #Bind the handler to the SIGINT(Ctrl-C) signal
        signal.signal(signal.SIGINT, self.on_sigint)
        #Start the UDP backend
        self.udpScanServer=UdpScannerServer(guiActive=True)
        #Get the HDF5 file lock identifier
        self.h5FileLock=self.udpScanServer.h5FileLock
        
        #Bool that controls whether or not the user changed the scan settings
        self.scanOptChanged=False
        #Latest scan options received from the grid
        self.recvScanOptTimestamp=0
        self.recvScanOpt=UdpScanProt.defaultOpt
        #Scan options defined by the user through the scan settings
        self.sendScanOpt=UdpScanProt.defaultOpt
        #Timer that triggers the sending of the scan settings to the boards of the grid
        self.sendScanOptTimer=wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_sendScanOpt_timer, self.sendScanOptTimer)
        self.sendScanOptTimer.Start(1000)
        
        
        #Create the timer for checking the HDF5 scan data
        self.h5Timer=wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_pollh5_timer, self.h5Timer)
        self.h5Timer.Start(250)
        
    def create_menu(self):
        self.menubar=wx.MenuBar()
        filemenu=wx.Menu()
        
        quitItem=filemenu.Append(wx.ID_EXIT, 'Quit', 'Quit application')
        self.Bind(wx.EVT_MENU, self.on_exit, quitItem)
        
        quickSaveItem=filemenu.Append(wx.ID_SAVE, 'Quick save plot', 'Quick save the plot')
        self.Bind(wx.EVT_MENU, self.on_quick_save, quickSaveItem)
        
        saveItem=filemenu.Append(wx.ID_SAVEAS, 'Save plot as', 'Save the plot to the specified file')
        self.Bind(wx.EVT_MENU, self.on_save, saveItem)
        
        self.menubar.Append(filemenu,"&File")
        self.SetMenuBar(self.menubar)
        
    def create_toolbar(self):
        self.toolbar = aui.AuiToolBar(self, agwStyle=aui.AUI_TB_HORZ_LAYOUT)
        
        quitTool=self.toolbar.AddSimpleTool(wx.ID_EXIT, "Quit tool", wx.ArtProvider.GetBitmap(wx.ART_QUIT),"Exit program")
        self.Bind(wx.EVT_MENU, self.on_exit, quitTool)
        
        quickSaveTool=self.toolbar.AddSimpleTool(wx.ID_SAVE, "Quick save tool", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE),"Quick save plot")
        self.Bind(wx.EVT_MENU, self.on_quick_save, quickSaveTool)
        
        saveTool=self.toolbar.AddSimpleTool(wx.ID_SAVEAS, "Save tool", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE_AS),"Save plot as")
        self.Bind(wx.EVT_MENU, self.on_save, saveTool)
        
        h5ViewTool=self.toolbar.AddSimpleTool(wx.ID_ANY, "H5 view tool", wx.ArtProvider.GetBitmap(wx.ART_REPORT_VIEW),"View the scan HDF5 data")
        self.Bind(wx.EVT_MENU, self.on_h5_view, h5ViewTool)
        
        self.toolbar.Realize()
    
    def create_boardList(self):
        '''
        Create the pane that lists the active white space detector boards
        '''
        self.boardList = ULC.UltimateListCtrl(self, wx.ID_ANY, size=wx.DefaultSize ,agwStyle=wx.LC_REPORT)
        self.boardList.InsertColumn(0, "MAC", format=ULC.ULC_FORMAT_CENTER, width=wx.LIST_AUTOSIZE_USEHEADER)
        self.boardList.InsertColumn(1, "IP", format=ULC.ULC_FORMAT_CENTER, width=wx.LIST_AUTOSIZE_USEHEADER)
        self.boardList.InsertColumn(2, "Status", format=ULC.ULC_FORMAT_CENTER, width=wx.LIST_AUTOSIZE_USEHEADER)
        self.boardList.InsertColumn(3, "Join date", format=ULC.ULC_FORMAT_CENTER, width=wx.LIST_AUTOSIZE_USEHEADER)
        
        #Bind the event of double clicking or pressing enter on top of a list item,
        #used to change the board being plotted in the main window
        self.Bind(ULC.EVT_LIST_ITEM_SELECTED, self.on_boardListItem_selected, self.boardList)
        
    def create_settingsTree(self):
        '''
        Create the TreeCtrl that handles the settings for the plot and the protocol
        '''
        self.settingsTree = CT.CustomTreeCtrl(self, agwStyle=CT.TR_DEFAULT_STYLE|CT.TR_HIDE_ROOT|CT.TR_HAS_VARIABLE_ROW_HEIGHT|CT.TR_ALIGN_WINDOWS)
        self.settingsTree.SetBackgroundColour(wx.NamedColour("white"))
        
        root = self.settingsTree.AddRoot("Root")
        scanOptItem = self.settingsTree.AppendItem(root, "Scan options")
        plotOptItem = self.settingsTree.AppendItem(root, "Plot options")
        
        #Create the scan options branch of the TreeCtrl
        freqRangeItem = self.settingsTree.AppendItem(scanOptItem, "Scan range")
        modItem = self.settingsTree.AppendItem(scanOptItem, "Modulation")
        gainItem = self.settingsTree.AppendItem(scanOptItem, "Gain")
        timingItem = self.settingsTree.AppendItem(scanOptItem, "Timing")
        
        self.freqStartSpinCtrl = FS.FloatSpin(self.settingsTree, -1, min_val=779, max_val=927.797,
                                         increment=0.203, digits=3, value=779, agwStyle=FS.FS_LEFT)
        freqStartItem = self.settingsTree.AppendItem(freqRangeItem, "Start(MHz)", wnd=self.freqStartSpinCtrl)
        self.Bind(FS.EVT_FLOATSPIN, self.on_freqRange_change, self.freqStartSpinCtrl)
        
        self.freqStopSpinCtrl = FS.FloatSpin(self.settingsTree, -1, min_val=779.203, max_val=928,
                                         increment=0.203, digits=3, value=928, agwStyle=FS.FS_LEFT)
        freqStopItem = self.settingsTree.AppendItem(freqRangeItem, "Stop(MHz)", wnd=self.freqStopSpinCtrl)
        self.Bind(FS.EVT_FLOATSPIN, self.on_freqRange_change, self.freqStopSpinCtrl)
        
        freqResChoices = ['58', '68', '81', '102', 
                          '116', '135', '162', '203',
                          '232', '270', '325', '406',
                          '464', '541', '650', '812']
        self.freqResBox = wx.ComboBox(self.settingsTree, -1, choices=freqResChoices, style=wx.CB_READONLY, size=(80,-1))
        self.freqResBox.SetValue('203')
        freqResItem = self.settingsTree.AppendItem(freqRangeItem, "Resolution(KHz)", wnd=self.freqResBox)
        self.Bind(wx.EVT_COMBOBOX, self.on_freqRange_change, self.freqResBox)
        
        modFormatChoices = ['OOK', 'ASK', '2-FSK', '4-FSK', 'GFSK', 'MSK']
        self.modFormatBox = wx.ComboBox(self.settingsTree, -1, choices=modFormatChoices, style=wx.CB_READONLY, size=(80,-1))
        self.modFormatBox.SetValue('ASK')
        modFormatItem = self.settingsTree.AppendItem(modItem, "Modulation format", wnd=self.modFormatBox)
        self.Bind(wx.EVT_COMBOBOX, self.on_modFormat_change, self.modFormatBox)
        
        self.agcEnableItem = self.settingsTree.AppendItem(gainItem, "Enable AGC", ct_type=1)
        self.settingsTree.CheckItem(self.agcEnableItem, checked=True)
        #Bind the item checked event
        self.Bind(CT.EVT_TREE_ITEM_CHECKED, self.on_settings_check, self.settingsTree)
        
        self.lnaGainSpinCtrl=wx.SpinCtrl(self.settingsTree, -1, min=0, max=3, initial=0, style=wx.ALIGN_LEFT|wx.SP_ARROW_KEYS)
        self.lnaGainItem = self.settingsTree.AppendItem(gainItem, "LNA Gain", wnd=self.lnaGainSpinCtrl)
        self.settingsTree.EnableItem(self.lnaGainItem, enable=False, torefresh=True)
        self.Bind(wx.EVT_SPINCTRL, self.on_gain_change, self.lnaGainSpinCtrl)
        
        self.lna2GainSpinCtrl=wx.SpinCtrl(self.settingsTree, -1, min=0, max=7, initial=7, style=wx.ALIGN_LEFT|wx.SP_ARROW_KEYS)
        self.lna2GainItem = self.settingsTree.AppendItem(gainItem, "LNA2 Gain", wnd=self.lna2GainSpinCtrl)
        self.settingsTree.EnableItem(self.lna2GainItem, enable=False, torefresh=True)
        self.Bind(wx.EVT_SPINCTRL, self.on_gain_change, self.lna2GainSpinCtrl)
        
        self.dvgaGainSpinCtrl=wx.SpinCtrl(self.settingsTree, -1, min=0, max=7, initial=7, style=wx.ALIGN_LEFT|wx.SP_ARROW_KEYS)
        self.dvgaGainItem = self.settingsTree.AppendItem(gainItem, "DVGA Gain", wnd=self.dvgaGainSpinCtrl)
        self.settingsTree.EnableItem(self.dvgaGainItem, enable=False, torefresh=True)
        self.Bind(wx.EVT_SPINCTRL, self.on_gain_change, self.dvgaGainSpinCtrl)
        
        self.rssiWaitSpinCtrl=FS.FloatSpin(self.settingsTree, -1, min_val=0.25, max_val=65,
                                         increment=1, digits=2, value=1, agwStyle=FS.FS_LEFT)
        rssiWaitItem = self.settingsTree.AppendItem(timingItem, "RSSI wait(ms)", wnd=self.rssiWaitSpinCtrl)
        self.Bind(FS.EVT_FLOATSPIN, self.on_timing_change, self.rssiWaitSpinCtrl)
        
        #Create the plot options branch of the TreeCtrl
        #TODO: Add graphic options here!!!!
        
    def on_sendScanOpt_timer(self, event):
        '''
        Function triggered when the sendScanOpt timer expires,
        checks if the scan options have changed and in that case
        sends the new options to all the active boards in the grid
        :param event: wx.Timer event
        '''         
        #Update the current options to be sent
        if self.scanOptChanged:
            self.sendScanOpt=UdpScanProt.Opt(freqStartMhz=int(self.freqStartSpinCtrl.GetValue()), \
                                             freqStartKhz=int((self.freqStartSpinCtrl.GetValue()-int(self.freqStartSpinCtrl.GetValue()))*1000), \
                                             freqStopMhz=int(self.freqStopSpinCtrl.GetValue()), \
                                             freqStopKhz=int((self.freqStopSpinCtrl.GetValue()-int(self.freqStopSpinCtrl.GetValue()))*1000), \
                                             freqRes=int(self.freqResBox.GetValue()), \
                                             modFormat=UdpScanProt.modFormatDict[self.modFormatBox.GetValue()], \
                                             agcEnabled=1 if self.settingsTree.IsItemChecked(self.agcEnableItem) else 0, \
                                             lnaGain=int(self.lnaGainSpinCtrl.GetValue()), \
                                             lna2Gain=int(self.lna2GainSpinCtrl.GetValue()), \
                                             dvgaGain=int(self.dvgaGainSpinCtrl.GetValue()), \
                                             rssiWait=int(self.rssiWaitSpinCtrl.GetValue()*1000))
            self.scanOptChanged=False
            
        if self.recvScanOpt!=self.sendScanOpt:
            print "Changed options: \n Send opt: \n", self.sendScanOpt, "\n Rev opt: \n", self.recvScanOpt
            for name in self.recvScanOpt._fields:
                recValue=getattr(self.recvScanOpt, name)
                sendValue=getattr(self.sendScanOpt, name)
                if recValue!=sendValue:
                    print "\n***The scan options differ in the ", name, " field: \n Sent: ", sendValue, " \tRecv: ", recValue
        if self.recvScanOpt!=self.sendScanOpt:
            maxIndex=self.boardList.GetItemCount()
            boardIpList=[]
            #Get a list of all the active boards to which we will send the new scan options
            for index in range(0, maxIndex):
                boardIp=self.boardList.GetItem(index, 1).GetText()
                if self.boardList.GetItem(index, 2).GetText()=='Active':
                    #Ignore localhost
                    if not boardIp.startswith("127."):
                        boardIpList.append(boardIp)
            if len(boardIpList)>0 and self.udpScanServer.sock!=None:
                #Start a thread to send the options
                UdpScannerClient(self.udpScanServer.sock, boardIpList, self.sendScanOpt)
        
    def on_pollh5_timer(self, event):
        '''
        Function triggered whenever the H5 poll timer expires that checks
        the HDF5 scan data file if it's been modified since the last access
        :param event: wx.Timer event
        '''
        with self.h5FileLock:
            #Check if the file exists before attempting to read it
            if not os.path.isfile("data/newScanData.h5"):
                return
            
            if not hasattr(self, 'h5LastModified') \
            or self.h5LastAccessed<os.stat("data/newScanData.h5").st_mtime \
            or self.replotRequested:
                if self.replotRequested:
                    self.replotRequested=False
                self.h5FileLastAccessed=os.stat("data/newScanData.h5").st_mtime
                self.process_h5_data()
            
    def process_h5_data(self):
        '''
        Checks the HDF5 scan data file and updates the GUI accordingly.
        '''
        h5File=tables.openFile("data/newScanData.h5", mode="r+", title="Scan data file")
        #Iterate through the node list and update the boardList pane
        for node in h5File.root.scannerNodes:
            #Make sure that the node has data
            if len(node)<=0:
                continue
            #Update the node if it's already in the list
            #and otherwise create it(FindItem returns -1 in case of failure)
            boardIndex=self.boardList.FindItem(start=-1, str=node.cols.macAddr[0], partial=False)
            if boardIndex==-1:
                macIndex=self.boardList.InsertStringItem(sys.maxint, label=node.cols.macAddr[0])
                statIndex=self.boardList.SetStringItem(macIndex, col=1, label=node.cols.ipAddr[0])
                if node.cols.isAlive[0]:
                    statIndex=self.boardList.SetStringItem(macIndex, col=2, label="Active")
                    self.boardList.SetItemTextColour(statIndex, wx.NamedColour("forest green"))
                else:
                    statIndex=self.boardList.SetStringItem(macIndex, col=2, label="Inactive")
                    self.boardList.SetItemTextColour(statIndex, wx.NamedColour("indian red"))
                self.boardList.SetStringItem(macIndex, col=3, label=datetime.datetime.fromtimestamp(node.cols.timestamp[0]).strftime('%c'))
                self.flash_status_message("Detected a new board with the MAC "+node.cols.macAddr[0])
            else:
                statItem=self.boardList.GetItem(boardIndex, col=2)
                joinItem=self.boardList.GetItem(boardIndex, col=3)
                if node.cols.isAlive[0] and statItem.GetText()!="Active":
                    statItem.SetText("Active")
                    self.boardList.SetItem(statItem)
                    self.boardList.SetItemTextColour(boardIndex, wx.NamedColour("forest green"))
                    joinItem.SetText(datetime.datetime.fromtimestamp(node.cols.timestamp[0]).strftime('%c'))
                    self.boardList.SetItem(joinItem)
                    self.flash_status_message("The board with the MAC "+node.cols.macAddr[0]+" became active")
                elif not node.cols.isAlive[0] and statItem.GetText()!="Inactive":
                    statItem.SetText("Inactive")
                    self.boardList.SetItem(statItem)
                    self.boardList.SetItemTextColour(boardIndex, wx.NamedColour("indian red"))
                    self.flash_status_message("The board with the MAC "+node.cols.macAddr[0]+" became inactive")
                    
            #Get the scan options of the node with the most recent timestamp
            if node.cols.isAlive[0] and node.cols.timestamp[len(node)-1]>self.recvScanOptTimestamp:
                self.recvScanOpt=UdpScanProt.Opt(freqStartMhz=int(node.cols.freqStart[0]), \
                                                 freqStartKhz=int((node.cols.freqStart[0]-int(node.cols.freqStart[0]))*1000), \
                                                 freqStopMhz=int(node.cols.freqStop[0]), \
                                                 freqStopKhz=int((node.cols.freqStop[0]-int(node.cols.freqStop[0]))*1000), \
                                                 freqRes=int(node.cols.freqRes[0]), \
                                                 modFormat=int(node.cols.modFormat[0]), \
                                                 agcEnabled=1 if node.cols.agcEnabled[0] else 0, \
                                                 lnaGain=int(node.cols.lnaGain[0]), \
                                                 lna2Gain=int(node.cols.lna2Gain[0]), \
                                                 dvgaGain=int(node.cols.dvgaGain[0]), \
                                                 rssiWait=int(node.cols.rssiWait[0]))
                    
            #If there is new data, update the board selected in the list,
            #if there is none we pick the first board of the HDF5 file
            if self.macPlottedBoard==None:
                self.macPlottedBoard=node.cols.macAddr[0]
                self.plottedDataTimestamp=node.cols.timestamp[len(node)-1]
                self.scanPlot.update_plot(node)
            elif self.macPlottedBoard==node.cols.macAddr[0] and self.plottedDataTimestamp<node.cols.timestamp[len(node)-1]:
                self.plottedDataTimestamp=node.cols.timestamp[len(node)-1]
                self.scanPlot.update_plot(node)
        
        #Close the HDF5 file after reading
        h5File.close()
        
    def flash_status_message(self, msg, flash_len_ms=1500):
        self.statusbar.SetStatusText(msg)
        self.timeroff = wx.Timer(self)
        self.Bind(
            wx.EVT_TIMER, 
            self.on_flash_status_off, 
            self.timeroff)
        self.timeroff.Start(flash_len_ms, oneShot=True)
    
    def on_flash_status_off(self, event):
        self.statusbar.SetStatusText('')
        
    def on_boardListItem_selected(self, event):
        '''
        Class triggered when the user selects one board of the list, 
        which becomes the one plotted in the main window
        :param event: ULC.EVT_LIST_ITEM_SELECTED
        '''
        self.macPlottedBoard=event.GetText()
        self.plottedDataTimestamp=0
        self.replotRequested=True
        
    def on_settings_check(self, event):
        '''
        Function triggered when a CustomTreeCtrl item is checked or unchecked
        :param event: CT.EVT_TREE_ITEM_CHECKED
        '''
        #Disable the manual gain settings if the AGC is activated and vice versa
        if event.GetItem() == self.agcEnableItem:
            self.scanOptChanged=True
            agcDisabled = not self.settingsTree.IsItemChecked(self.agcEnableItem)
            self.settingsTree.EnableItem(self.lnaGainItem, enable=agcDisabled, torefresh=True)
            self.settingsTree.EnableItem(self.lna2GainItem, enable=agcDisabled, torefresh=True)
            self.settingsTree.EnableItem(self.dvgaGainItem, enable=agcDisabled, torefresh=True)
            
    def on_freqRange_change(self, event):
        '''
        Function triggered when the user changes the freqStart, freqStop or freqRange options,
        which ensures that the scan range options remain consistent
        and updates the scanOpt tuple to be sent to the boards
        :param event: Event that triggered the function
        '''
        self.scanOptChanged=True
        if event.GetEventObject()==self.freqStartSpinCtrl:
            self.freqStopSpinCtrl.SetRange(min_val=self.freqStartSpinCtrl.GetValue()+float(self.freqResBox.GetValue())/1000.0,
                                           max_val=self.freqStopSpinCtrl.GetMax())
        elif event.GetEventObject()==self.freqStopSpinCtrl:
            self.freqStartSpinCtrl.SetRange(max_val=self.freqStopSpinCtrl.GetValue()-float(self.freqResBox.GetValue())/1000.0,
                                            min_val=self.freqStartSpinCtrl.GetMin())
        elif event.GetEventObject()==self.freqResBox:
            self.freqStartSpinCtrl.SetIncrement(float(self.freqResBox.GetValue())/1000.0)
            self.freqStopSpinCtrl.SetIncrement(float(self.freqResBox.GetValue())/1000.0)

    def on_gain_change(self, event):
        '''
        Function triggered when the user changes the gain values of the different amplifier stages
        that updates the scanOpt tuple to be sent to the boards
        :param event: wx.EVT_SPIN
        '''
        self.scanOptChanged=True
        
    def on_modFormat_change(self, event):
        '''
        Function triggered when the user changes the modulation format settings,
        which updates the scanOpt tuple to be sent to the boards
        :param event: wx.EVT_COMBOBOX
        '''
        self.scanOptChanged=True
    
    def on_timing_change(self, event):
        '''
        Function triggered when the user changes the timing settings,
        which updates the scanOpt tuple to be sent to the boards
        :param event: wx.EVT_SPIN
        '''
        self.scanOptChanged=True
            
        
    def on_h5_view(self, event):
        '''
        Open the graphical HDF5 view tool to explore the data
        :param event: Event that triggered this function
        '''
        subprocess.Popen(["vitables", "data/newScanData.h5"])
        self.flash_status_message("Opening the HDF5 file explorer...")
        
    def on_save(self, event):
        '''
        Open the file save dialogue to store the plot image
        :param event: Event that triggered this function
        '''
        file_choices = "PNG (*.png)|*.png"
        defaultFileName="rssiScanPlot_capturedOn_"+datetime.datetime.now().strftime("%d-%m-%y_%H-%M")+".png"
        
        dlg = wx.FileDialog(
            self, 
            message="Save plot as...",
            defaultDir=os.getcwd(),
            defaultFile=defaultFileName,
            wildcard=file_choices,
            style=wx.SAVE)
        
        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.scanPlot.print_figure(path)
            self.flash_status_message("Saved to %s" % path)
        
    def on_quick_save(self, event):
        '''
        Save the plot image using the default name and without going through the file browser
        :param event: Event that triggered this function
        '''
        defaultFileName="rssiScanPlot_capturedOn_"+datetime.datetime.now().strftime("%d-%m-%y_%H-%M")+".png"
        path=os.getcwd()+defaultFileName
        self.scanPlot.print_figure(path)
        self.flash_status_message("Saved to %s" % path)
        
    def on_exit(self, event):
        '''
        Close the interface and the associated backend
        :param event: EVT_EXIT
        '''
        self.exit_program()
        
    def on_sigint(self,signum,stack):
        print "\nCtrl-C detected, closing the main program and backend threads..."
        self.exit_program()
        
    def exit_program(self):
        self.h5Timer.Stop()
        self.sendScanOptTimer.Stop()
        self.udpScanServer.close_backend()
        self.Destroy()
        
if __name__ == '__main__':
    app = wx.App(0)
    frame = ScannerGUI(None)
    frame.Show()
    app.MainLoop()
