#!/usr/bin/python

import os
import wx
import Queue
import collections
import time
import datetime
from scannerUdpBackend import UdpScannerApp
from scannerUdpBackend import UdpScannerClient

# The recommended way to use wx with mpl is with the WXAgg
# backend. 
#
import matplotlib
matplotlib.use('WXAgg')
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import \
    FigureCanvasWxAgg as FigCanvas
import numpy as np

# Define notification event for the arrival of new data
EVT_RESULT_ID = wx.NewId()
 
def EVT_RESULT(win, func):
    """Define Result Event."""
    win.Connect(-1, -1, EVT_RESULT_ID, func)
    
class RangeControlBox(wx.Panel):

    def __init__(self, parent, ID, label, wxObject):
        wx.Panel.__init__(self, parent, ID)
        
        self.yMin = -90.0
        self.yMax = 0.0
        self.wxObject=wxObject
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.yMin_label = wx.StaticText(self, -1, 'Y min')
        self.yMin_text = wx.TextCtrl(self, -1, 
            value=str(self.yMin),
            style=wx.TE_PROCESS_ENTER)
        self.yMinUnit_label = wx.StaticText(self, -1, 'dBm')
        self.Bind(wx.EVT_TEXT_ENTER, self.yMin_on_text_enter, self.yMin_text)
        
        self.yMax_label = wx.StaticText(self, -1, 'Y max')
        self.yMax_text = wx.TextCtrl(self, -1, 
            value=str(self.yMax),
            style=wx.TE_PROCESS_ENTER)
        self.yMaxUnit_label = wx.StaticText(self, -1, 'dBm')
        self.Bind(wx.EVT_TEXT_ENTER, self.yMax_on_text_enter, self.yMax_text)
        
        yMin_box = wx.BoxSizer(wx.HORIZONTAL)
        yMin_box.Add(self.yMin_label, flag=wx.ALIGN_CENTER_VERTICAL)
        yMin_box.AddSpacer(10)
        yMin_box.Add(self.yMin_text, flag=wx.ALIGN_CENTER_VERTICAL)
        yMin_box.AddSpacer(5)
        yMin_box.Add(self.yMinUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        yMax_box = wx.BoxSizer(wx.HORIZONTAL)
        yMax_box.Add(self.yMax_label, flag=wx.ALIGN_CENTER_VERTICAL)
        yMax_box.AddSpacer(10)
        yMax_box.Add(self.yMax_text, flag=wx.ALIGN_CENTER_VERTICAL)
        yMax_box.AddSpacer(5)
        yMax_box.Add(self.yMaxUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(yMin_box, 0, wx.ALL, 10)
        sizer.Add(yMax_box, 0, wx.ALL, 10)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def yMin_on_text_enter(self, event):
        self.yMin = float(self.yMin_text.GetValue())
        print "New yMin value: ", self.yMin
        self.wxObject.draw_plot()
        
    def yMax_on_text_enter(self, event):
        self.yMax = float(self.yMax_text.GetValue())
        print "New yMin value: ", self.yMax
        self.wxObject.draw_plot()

class FreqControlBox(wx.Panel):

    def __init__(self, parent, ID, label):
        wx.Panel.__init__(self, parent, ID)
        
        self.startFreq = 779.0
        self.stopFreq = 928.0
        self.freqResolution = 203
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.startFreq_text = wx.TextCtrl(self, -1, 
            value=str(self.startFreq),
            style=wx.TE_PROCESS_ENTER)
        self.startFreq_label = wx.StaticText(self, -1, 'Start frequency')
        self.startFreqUnit_label = wx.StaticText(self, -1, 'MHz')
        
        self.stopFreq_text = wx.TextCtrl(self, -1, 
            value=str(self.stopFreq),
            style=wx.TE_PROCESS_ENTER)
        self.stopFreq_label = wx.StaticText(self, -1, 'Stop frequency')
        self.stopFreqUnit_label = wx.StaticText(self, -1, 'MHz')
        
        self.freqResolution_text = wx.TextCtrl(self, -1, 
            value=str(self.freqResolution),
            style=wx.TE_PROCESS_ENTER)
        self.freqResolution_label = wx.StaticText(self, -1, 'Resolution')
        self.freqResolutionUnit_label = wx.StaticText(self, -1, 'KHz')
        
        
        self.Bind(wx.EVT_TEXT_ENTER, self.startFreq_on_text_enter, self.startFreq_text)
        self.Bind(wx.EVT_TEXT_ENTER, self.stopFreq_on_text_enter, self.stopFreq_text)
        self.Bind(wx.EVT_TEXT_ENTER, self.freqResolution_on_text_enter, self.freqResolution_text)
        
        startFreq_box = wx.BoxSizer(wx.HORIZONTAL)
        startFreq_box.Add(self.startFreq_label, flag=wx.ALIGN_CENTER_VERTICAL)
        startFreq_box.AddSpacer(10)
        startFreq_box.Add(self.startFreq_text, flag=wx.ALIGN_CENTER_VERTICAL)
        startFreq_box.AddSpacer(5)
        startFreq_box.Add(self.startFreqUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        stopFreq_box = wx.BoxSizer(wx.HORIZONTAL)
        stopFreq_box.Add(self.stopFreq_label, flag=wx.ALIGN_CENTER_VERTICAL)
        stopFreq_box.AddSpacer(10)
        stopFreq_box.Add(self.stopFreq_text, flag=wx.ALIGN_CENTER_VERTICAL)
        stopFreq_box.AddSpacer(5)
        stopFreq_box.Add(self.stopFreqUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        freqResolution_box = wx.BoxSizer(wx.HORIZONTAL)
        freqResolution_box.Add(self.freqResolution_label, flag=wx.ALIGN_CENTER_VERTICAL)
        freqResolution_box.AddSpacer(30)
        freqResolution_box.Add(self.freqResolution_text, flag=wx.ALIGN_CENTER_VERTICAL)
        freqResolution_box.AddSpacer(5)
        freqResolution_box.Add(self.freqResolutionUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(startFreq_box, 0, wx.ALL, 10)
        sizer.Add(stopFreq_box, 0, wx.ALL, 10)
        sizer.Add(freqResolution_box, 0, wx.ALL, 10)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def startFreq_on_text_enter(self, event):
        self.startFreq = self.startFreq_text.GetValue()
        print "New startFreq value: ", self.startFreq
    
    def stopFreq_on_text_enter(self, event):
        self.stopFreq = self.stopFreq_text.GetValue()
        print "New stopFreq value: ", self.stopFreq
        
    def freqResolution_on_text_enter(self, event):
        self.freqResolution = self.freqResolution_text.GetValue()
        print "New freqResolution value: ", self.freqResolution
        

class ModControlBox(wx.Panel):
    def __init__(self, parent, ID, label):
        wx.Panel.__init__(self, parent, ID)
        
        self.freqDeviation = 0
        self.decBoundary = 8
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.modFormat_text = wx.StaticText(self, -1, 'Modulation format')
        self.modFormats = ['OOK', 'ASK', '2-FSK', '4-FSK', 'GFSK', 'MSK']
        self.modFormatDict = {'2FSK':1, 'GFSK':1, 'ASK':2, 'OOK':3, '4FSK':4, 'MSK':5}
        self.modFormat_cb = wx.ComboBox(self, choices=self.modFormats, 
            style=wx.CB_READONLY, size=(80,-1))
        self.modFormat_cb.SetValue('OOK')
        self.modFormat_protValue=self.modFormatDict[self.modFormat_cb.GetValue()]
        self.Bind(wx.EVT_COMBOBOX, self.modFormat_on_select, self.modFormat_cb)
        
        self.freqDeviation_label = wx.StaticText(self, -1, 'Frequency deviation')
        self.freqDeviation_text = wx.TextCtrl(self, -1, 
            value=str(self.freqDeviation),
            style=wx.TE_PROCESS_ENTER)
        self.freqDeviationUnit_label = wx.StaticText(self, -1, 'KHz')
        self.freqDeviation_label.Disable()
        self.freqDeviation_text.Disable()
        self.freqDeviationUnit_label.Disable()
        self.Bind(wx.EVT_TEXT_ENTER, self.freqDeviation_on_text_enter, self.freqDeviation_text)
        
        self.decBoundary_label = wx.StaticText(self, -1, 'Decision boundary')
        self.decBoundary_text = wx.TextCtrl(self, -1, 
            value=str(self.decBoundary),
            style=wx.TE_PROCESS_ENTER)
        self.decBoundaryUnit_label = wx.StaticText(self, -1, 'dB')
        self.decBoundary_text.Enable()
        self.Bind(wx.EVT_TEXT_ENTER, self.decBoundary_on_text_enter, self.decBoundary_text)
        
        modFormat_box = wx.BoxSizer(wx.HORIZONTAL)
        modFormat_box.Add(self.modFormat_text, flag=wx.ALIGN_CENTER_VERTICAL)
        modFormat_box.AddSpacer(20)
        modFormat_box.Add(self.modFormat_cb, flag=wx.ALIGN_CENTER_VERTICAL)
        
        freqDeviation_box = wx.BoxSizer(wx.HORIZONTAL)
        freqDeviation_box.Add(self.freqDeviation_label, flag=wx.ALIGN_CENTER_VERTICAL)
        freqDeviation_box.AddSpacer(10)
        freqDeviation_box.Add(self.freqDeviation_text, flag=wx.ALIGN_CENTER_VERTICAL)
        freqDeviation_box.AddSpacer(5)
        freqDeviation_box.Add(self.freqDeviationUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        decBoundary_box = wx.BoxSizer(wx.HORIZONTAL)
        decBoundary_box.Add(self.decBoundary_label, flag=wx.ALIGN_CENTER_VERTICAL)
        decBoundary_box.AddSpacer(15)
        decBoundary_box.Add(self.decBoundary_text, flag=wx.ALIGN_CENTER_VERTICAL)
        decBoundary_box.AddSpacer(5)
        decBoundary_box.Add(self.decBoundaryUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(modFormat_box, 0, wx.ALL, 11)
        sizer.Add(freqDeviation_box, 0, wx.ALL, 10)
        sizer.Add(decBoundary_box, 0, wx.ALL, 10)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def modFormat_on_select(self, event):
        modFormat = event.GetString()
        print "New modulation type chosen: ", modFormat
        if modFormat=='OOK' or modFormat=='ASK':
            self.freqDeviation_label.Disable()
            self.freqDeviation_text.Disable()
            self.freqDeviationUnit_label.Disable()
            self.decBoundary_label.Enable()
            self.decBoundary_text.Enable()
            self.decBoundaryUnit_label.Enable()
        elif modFormat=='2-FSK' or modFormat=='4-FSK' or modFormat=='GFSK':
            self.freqDeviation_label.Enable()
            self.freqDeviation_text.Enable()
            self.freqDeviationUnit_label.Enable()
            self.decBoundary_label.Disable()
            self.decBoundary_text.Disable()
            self.decBoundaryUnit_label.Disable()
        elif modFormat=='MSK':
            self.freqDeviation_label.Disable()
            self.freqDeviation_text.Disable()
            self.freqDeviationUnit_label.Disable()
            self.decBoundary_label.Disable()
            self.decBoundary_text.Disable()
            self.decBoundaryUnit_label.Disable()
        self.modFormat_protValue=self.modFormatDict[self.modFormat_cb.GetValue()]
        
    def freqDeviation_on_text_enter(self, event):
        self.freqDeviation = self.freqDeviation_text.GetValue()
        print "New frequency deviation value: ", self.freqDeviation, self.freqDeviationUnit_label.GetLabelText()
    
    def decBoundary_on_text_enter(self, event):
        self.decBoundary = self.decBoundary_text.GetLabelText()
        print "New decision boundary value: ", self.decBoundary, self.decBoundaryUnit_label.GetString()    
        
class GainControlBox(wx.Panel):
    def __init__(self, parent, ID, label):
        wx.Panel.__init__(self, parent, ID)
        
        self.lnaGain = 2
        self.lna2Gain = 0
        self.dvgaGain = 0
        
        #Careful, LNA gain scale goes from 0 to 2, where 0 represents the maximum gain
        #To avoid confusions, we set a dictionary to map the slider values to those of the LNA register
        self.lnaValueDict={0:2, 1:1, 2:0}
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.agcEnable_cb = wx.CheckBox(self, -1, 
            "Enable AGC",
            style=wx.ALIGN_RIGHT)
        
        self.Bind(wx.EVT_CHECKBOX, self.agcEnable_on_click, self.agcEnable_cb)
        self.agcEnable_cb.SetValue(True)
        
        self.lnaGain_label = wx.StaticText(self, -1, 'LNA Gain')
        self.lnaGain_sld = wx.Slider(self, -1, value=0, minValue=0, maxValue=3, 
                             style=wx.SL_HORIZONTAL | wx.SL_LABELS | wx.SL_BOTTOM, name="LNA Gain", size=(125, -1))
        self.Bind(wx.EVT_COMMAND_SCROLL_THUMBRELEASE, self.lna_on_thumbrelease, self.lnaGain_sld)
        
        self.lna2Gain_label = wx.StaticText(self, -1, 'LNA2 Gain')
        self.lna2Gain_sld = wx.Slider(self, -1, value=0, minValue=0, maxValue=7,
                             style=wx.SL_AUTOTICKS | wx.SL_HORIZONTAL | wx.SL_LABELS, name="LNA2 Gain", size=(125, -1))
        self.Bind(wx.EVT_COMMAND_SCROLL_THUMBRELEASE, self.lna2_on_thumbrelease, self.lna2Gain_sld)
        
        self.dvgaGain_label = wx.StaticText(self, -1, 'DVGA Gain')
        self.dvgaGain_sld = wx.Slider(self, -1, value=0, minValue=0, maxValue=7,
                             style=wx.SL_AUTOTICKS | wx.SL_HORIZONTAL | wx.SL_LABELS, name="DVGA Gain", size=(125, -1))
        self.Bind(wx.EVT_COMMAND_SCROLL_THUMBRELEASE, self.dvga_on_thumbrelease, self.dvgaGain_sld)
        
        self.lnaGain_label.Disable()
        self.lnaGain_sld.Disable()
        self.lna2Gain_label.Disable()
        self.lna2Gain_sld.Disable()
        self.dvgaGain_label.Disable()
        self.dvgaGain_sld.Disable()
        
        agcEnable_box = wx.BoxSizer(wx.HORIZONTAL)
        agcEnable_box.Add(self.agcEnable_cb, flag=wx.ALIGN_CENTER_VERTICAL)
        
        lnaGain_box = wx.BoxSizer(wx.HORIZONTAL)
        lnaGain_box.Add(self.lnaGain_label, flag=wx.ALIGN_CENTER_VERTICAL)
        lnaGain_box.AddSpacer(15)
        lnaGain_box.Add(self.lnaGain_sld, flag=wx.ALIGN_CENTER_VERTICAL)
        
        lna2Gain_box = wx.BoxSizer(wx.HORIZONTAL)
        lna2Gain_box.Add(self.lna2Gain_label, flag=wx.ALIGN_CENTER_VERTICAL)
        lna2Gain_box.AddSpacer(10)
        lna2Gain_box.Add(self.lna2Gain_sld, flag=wx.ALIGN_CENTER_VERTICAL)
        
        dvgaGain_box = wx.BoxSizer(wx.HORIZONTAL)
        dvgaGain_box.Add(self.dvgaGain_label, flag=wx.ALIGN_CENTER_VERTICAL)
        dvgaGain_box.AddSpacer(10)
        dvgaGain_box.Add(self.dvgaGain_sld, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(agcEnable_box, 0, wx.ALL, 3)
        sizer.Add(lnaGain_box, 0, wx.ALL, 2)
        sizer.Add(lna2Gain_box, 0, wx.ALL, 2)
        sizer.Add(dvgaGain_box, 0, wx.ALL, 2)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def agcEnable_on_click(self, event):
        if self.agcEnable_cb.IsChecked():
            self.lnaGain_label.Disable()
            self.lnaGain_sld.Disable()
            self.lna2Gain_label.Disable()
            self.lna2Gain_sld.Disable()
            self.dvgaGain_label.Disable()
            self.dvgaGain_sld.Disable()
            print "Automatic Gain Control Enabled"
        else:
            self.lnaGain_label.Enable()
            self.lnaGain_sld.Enable()
            self.lna2Gain_label.Enable()
            self.lna2Gain_sld.Enable()
            self.dvgaGain_label.Enable()
            self.dvgaGain_sld.Enable()
            print "Automatic Gain Control Disabled"
    
    def lna_on_thumbrelease(self, event):
        self.lnaGain = self.lnaValueDict[self.lnaGain_sld.GetValue()]
        print "New LNA gain value: ", self.lnaGain
    
    def lna2_on_thumbrelease(self, event):
        self.lna2Gain = self.lna2Gain_sld.GetValue()
        print "New LNA2 gain value: ", self.lna2Gain
        
    def dvga_on_thumbrelease(self, event):
        self.dvgaGain = self.dvgaGain_sld.GetValue()
        print "New DVGA gain value: ", self.dvgaGain
        
class RssiTimingControlBox(wx.Panel):

    def __init__(self, parent, ID, label):
        wx.Panel.__init__(self, parent, ID)
        
        self.rssiWait = 1000
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.rssiWait_text = wx.TextCtrl(self, -1, 
            value=str(self.rssiWait),
            style=wx.TE_PROCESS_ENTER)
        self.rssiWait_label = wx.StaticText(self, -1, 'RSSI settling time')
        self.rssiWaitUnit_label = wx.StaticText(self, -1, 'us')   
        
        self.Bind(wx.EVT_TEXT_ENTER, self.rssiWait_on_text_enter, self.rssiWait_text)

        rssiWait_box = wx.BoxSizer(wx.HORIZONTAL)
        rssiWait_box.Add(self.rssiWait_label, flag=wx.ALIGN_CENTER_VERTICAL)
        rssiWait_box.AddSpacer(10)
        rssiWait_box.Add(self.rssiWait_text, flag=wx.ALIGN_CENTER_VERTICAL)
        rssiWait_box.AddSpacer(5)
        rssiWait_box.Add(self.rssiWaitUnit_label, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(rssiWait_box, 0, wx.ALL, 10)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def rssiWait_on_text_enter(self, event):
        self.rssiWait = self.rssiWait_text.GetValue()
        print "New rssiWait value: ", self.rssiWait

class ScannerGUI(wx.Frame):
    title = "GUI for the frequency scanning protocol"
    def __init__(self):
        wx.Frame.__init__(self, None, -1, self.title)
        
        self.data = np.arange(779, 928)
        self.avgData = self.data
        self.amtRecvArrays=0
        self.scannerAddr=("127.0.0.1",60000)
        
        self.ScanOptions = collections.namedtuple('ScanOptions', 'startFreqMhz startFreqKhz \
                                                  stopFreqMhz stopFreqKhz freqResolution modFormat activateAGC \
                                                  agcLnaGain agcLna2Gain agcDvgaGain rssiWait')
        self.defaultScanOptions = self.ScanOptions(startFreqMhz=779, startFreqKhz=0, stopFreqMhz=928, stopFreqKhz=0,
                                                freqResolution=203, modFormat=3, activateAGC=1, agcLnaGain=0,
                                                agcLna2Gain=0, agcDvgaGain=0, rssiWait=1000)
        self.recvScanOptions=self.defaultScanOptions
        self.prevRecvScanOptions=self.defaultScanOptions
        
        #Plotting values
        self.yMin=-90
        self.yMax=0
        
        self.create_menu()
        self.create_status_bar()
        self.create_main_panel()
        
        #Timer that registers the time of the last replot
        self.paintTimer=time.time()
        #Timer that registers the time of the last reset of the average
        self.avgResetTimer=time.time()
        #self.redraw_timer = wx.Timer(self)
        #self.Bind(wx.EVT_TIMER, self.on_redraw_timer, self.redraw_timer)        
        #self.redraw_timer.Start(100)
        #y-axis range of the plot
        
        # Set up event handler for any worker thread results
        EVT_RESULT(self, self.on_new_rssi_data)
        
        #Start the UDP backend
        self.dataQueue=Queue.Queue()
        self.recvOptQueue=Queue.Queue()
        self.addrQueue=Queue.Queue()
        print "Starting UDP server..."
        self.udpScanner = UdpScannerApp(self.dataQueue, self.recvOptQueue, self.addrQueue, self, EVT_RESULT_ID)
        
    def create_menu(self):
        self.menubar = wx.MenuBar()
        
        menu_file = wx.Menu()
        m_expt = menu_file.Append(-1, "&Save plot\tCtrl-S", "Save plot to file")
        self.Bind(wx.EVT_MENU, self.on_save_plot, m_expt)
        menu_file.AppendSeparator()
        m_exit = menu_file.Append(-1, "E&xit\tCtrl-X", "Exit")
        self.Bind(wx.EVT_MENU, self.on_exit, m_exit)
                
        self.menubar.Append(menu_file, "&File")
        self.SetMenuBar(self.menubar)

    def create_main_panel(self):
        self.panel = wx.Panel(self)

        self.init_plot()
        self.canvas = FigCanvas(self.panel, -1, self.fig)

        self.freqRange_control = FreqControlBox(self.panel, -1, "Frequency range")
        
        self.modulation_control = ModControlBox(self.panel, -1, "Modulation")
        
        self.gain_control =  GainControlBox(self.panel, -1, "Gain")
        
        self.rssiTiming_control = RssiTimingControlBox(self.panel, -1, "RSSI timing")
        
        self.yRange_control=RangeControlBox(self.panel, -1, "Y range", self)
        self.yRange_control.Disable()
        
        self.txScanOptions_button = wx.Button(self.panel, -1, "Send scan options")
        self.Bind(wx.EVT_BUTTON, self.on_txScanOptions_button, self.txScanOptions_button)
        
        self.cb_grid = wx.CheckBox(self.panel, -1, 
            "Show Grid",
            style=wx.ALIGN_RIGHT)
        self.Bind(wx.EVT_CHECKBOX, self.on_cb_grid, self.cb_grid)
        self.cb_grid.SetValue(False)
        
        self.cb_yAutoRange = wx.CheckBox(self.panel, -1, 
           "y-axis auto range",
            style=wx.ALIGN_RIGHT)
        self.Bind(wx.EVT_CHECKBOX, self.on_cb_yAutoRange, self.cb_yAutoRange)
        self.cb_yAutoRange.SetValue(True)
        
        self.hbox1 = wx.BoxSizer(wx.HORIZONTAL)
        self.hbox1.Add(self.txScanOptions_button, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL)
        self.hbox1.AddSpacer(20)
        self.hbox1.Add(self.cb_grid, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL)
        self.hbox1.Add(self.cb_yAutoRange, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL)
        
        self.hbox2 = wx.BoxSizer(wx.HORIZONTAL)
        self.hbox2.Add(self.freqRange_control, border=5, flag=wx.ALL)
        self.hbox2.Add(self.modulation_control, border=5, flag=wx.ALL)
        self.hbox2.Add(self.gain_control, border=5, flag=wx.ALL)
        self.hbox2.Add(self.rssiTiming_control, border=5, flag=wx.ALL)
        self.hbox2.Add(self.yRange_control, border=5, flag=wx.ALL)
        
        self.vbox = wx.BoxSizer(wx.VERTICAL)
        self.vbox.Add(self.canvas, 1, flag=wx.LEFT | wx.TOP | wx.EXPAND)        
        self.vbox.Add(self.hbox1, 0, flag=wx.ALIGN_LEFT | wx.TOP)
        self.vbox.Add(self.hbox2, 0, flag=wx.ALIGN_LEFT | wx.TOP)
        
        self.panel.SetSizer(self.vbox)
        self.vbox.Fit(self)
    
    def create_status_bar(self):
        self.statusbar = self.CreateStatusBar()

    def init_plot(self):
        self.dpi = 100
        self.fig = Figure(dpi=self.dpi)
        self.fig.subplots_adjust(left=0.1, bottom=0.1, right=0.95)

        self.axes = self.fig.add_subplot(111)
        self.axes.tick_params(axis='both', labelsize='small')
        self.axes.set_axis_bgcolor('white')
        self.axes.set_title("Frequency scan results", size='medium')
        self.axes.set_ylabel("RSSI(dBm)", size='medium', labelpad=10)
        self.axes.set_xlabel("Frequency(MHz)", size='medium', labelpad=10)
        
        # plot the data as a line series, and save the reference 
        # to the plotted line series
        #
        self.plot_data = self.axes.plot(
            self.data, 
            linewidth=1,
            color='b',
            )[0]
        self.plot_avgData = self.axes.plot(
            self.avgData, 
            linewidth=1,
            color='r',
            )[0]
        
        self.axes.legend(('Current', 'Average'), 'lower right', shadow=False, fontsize='small', frameon=True)

    def draw_plot(self):
        """ Redraws the plot
        """
        # for ymin and ymax, find the minimal and maximal values
        # in the data set and add a mininal margin
        # if the auto range option is checked, otherwise
        # use the user-provided values
        if self.cb_yAutoRange.IsChecked():
            ymin = round(min(min(self.data), min(self.avgData))) - 1
            ymax = round(max(max(self.data), max(self.avgData))) + 1
        else:
            ymin = self.yRange_control.yMin
            ymax = self.yRange_control.yMax

        self.axes.set_ybound(lower=ymin, upper=ymax)
        
        # anecdote: axes.grid assumes b=True if any other flag is
        # given even if b is set to False.
        # so just passing the flag into the first statement won't
        # work.
        #
        if self.cb_grid.IsChecked():
            self.axes.grid(True, color='gray')
        else:
            self.axes.grid(False)
            
        #Calculate the x-axis values based on the scan options
        startFreq=self.recvScanOptions.startFreqMhz+self.recvScanOptions.startFreqKhz/1000
        stopFreq=self.recvScanOptions.stopFreqMhz+self.recvScanOptions.stopFreqKhz/1000
        freqValues=np.linspace(startFreq, stopFreq, len(self.data))
        
        self.axes.set_xbound(lower=startFreq, upper=stopFreq)
        
        self.plot_data.set_xdata(freqValues)
        self.plot_data.set_ydata(self.data)
        
        self.plot_avgData.set_xdata(freqValues)
        self.plot_avgData.set_ydata(self.avgData)
        
        self.canvas.draw()
        #Store the time of the last redraw, useful to prevent GUI overload
        self.paintTimer=time.time()
        
    def on_new_rssi_data(self, msg):
        if not self.dataQueue.empty() and not self.recvOptQueue.empty() and not self.addrQueue.empty():
            self.data=self.dataQueue.get()
            self.prevRecvScanOptions=self.recvScanOptions
            self.recvScanOptions=self.recvOptQueue.get()
            self.scannerAddr=self.addrQueue.get()
            
            #If the scan options have changed, reset the averaging
            if self.recvScanOptions!=self.prevRecvScanOptions or self.amtRecvArrays<=0:
                self.amtRecvArrays=1
                self.avgData=self.data
                self.avgResetTimer=time.time()
            else:
                self.amtRecvArrays+=1
                self.avgData=np.average([self.avgData, self.data], axis=0, weights=[self.amtRecvArrays-1, 1])
                #TODO: Make the averaging reset timer user-defined
                #If a lot of time(for now, 10s) has passed since the last reset of the averaging
                #or too many arrays have been received(for now, 1000), reset again
                if time.time()-self.avgResetTimer>100 or self.amtRecvArrays>10000:
                    self.amtRecvArrays=0
            #Replot the data if more than 100ms have passed since the last time
            #This prevents the GUI from becoming unresponsive
            if time.time()-self.paintTimer>0.1:
                print "\n\n****Plotting data....****\n\n"
                self.draw_plot()
    
    #When this button is clicked, we start a thread that sends
    #the new scanning options to the microcontroller platform
    def on_txScanOptions_button(self, event):
        #Gather all the options into a single ScanOptions namedtuple
        guiStartFreqMhz=int(float(self.freqRange_control.startFreq))
        guiStartFreqKhz=int((float(self.freqRange_control.startFreq) - guiStartFreqMhz)*1000)
        guiStopFreqMhz=int(float(self.freqRange_control.stopFreq))
        guiStopFreqKhz=int((float(self.freqRange_control.stopFreq) - guiStopFreqMhz)*1000)
        guiFreqResolution=int(float(self.freqRange_control.freqResolution))
        guiModFormat=self.modulation_control.modFormat_protValue
        if self.gain_control.agcEnable_cb.IsChecked():
            guiActivateAgc=1
        else:
            guiActivateAgc=0
        guiLnaGain=self.gain_control.lnaGain
        guiLna2Gain=self.gain_control.lna2Gain
        guiDvgaGain=self.gain_control.dvgaGain
        guiRssiWait=int(self.rssiTiming_control.rssiWait)
        self.guiScanOptions = self.ScanOptions(startFreqMhz=guiStartFreqMhz, startFreqKhz=guiStartFreqKhz, stopFreqMhz=guiStopFreqMhz, stopFreqKhz=guiStopFreqKhz, \
                                         freqResolution=guiFreqResolution, modFormat=guiModFormat, activateAGC=guiActivateAgc, agcLnaGain=guiLnaGain, \
                                         agcLna2Gain=guiLna2Gain, agcDvgaGain=guiDvgaGain, rssiWait=guiRssiWait)
        
        #Start the client thread to send the options
        self.udpScannerClient=UdpScannerClient(self.scannerAddr, self.guiScanOptions)
    
    def on_cb_grid(self, event):
        self.draw_plot()
        
    def on_cb_yAutoRange(self, event):
        if self.cb_yAutoRange.IsChecked():
            self.yRange_control.Disable()
        else:
            self.yRange_control.Enable()    
        self.draw_plot()
    
    def on_save_plot(self, event):
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
            self.canvas.print_figure(path, dpi=self.dpi)
            self.flash_status_message("Saved to %s" % path)
    
    def on_redraw_timer(self, event):
        self.draw_plot()
        
    def on_exit(self, event):
        self.udpScanner.join(None)
        self.Destroy()
    
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
        
if __name__ == '__main__':
    app = wx.PySimpleApp()
    app.frame = ScannerGUI()
    app.frame.Show()
    app.MainLoop()
        