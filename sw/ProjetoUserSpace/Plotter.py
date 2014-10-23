#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import matplotlib.pyplot as plt
import numpy as np

class PlotISI:
    def __init__(self, spk_num=1, markersize=5, linewidth=2):
        """Plots the ISI of the arriving data."""
        self.ms = markersize
        self.lw = linewidth

        self.fig = plt.figure(1)
        self.ax = []
        self.line = []
        for i in range(8):
            self.ax.append(self.fig.add_subplot(4,2,i+1))
            self.line.append(self.ax[-1].plot([],[],linestyle='solid', marker='o', \
                color="#000000",markersize=self.ms,linewidth=self.lw)[0])

        self.first_ts = []
        self.timestamps = []
        self.isi = []
        self.ts_now = []
        for i in range(8):
            self.first_ts.append(0)
            self.timestamps.append([])
            self.isi.append([])
            self.ts_now.append(None)

        self.ymin = 0
        self.ymax = 2 #s

        #self.ax.set_title('Inter Spike Intervals')
        #self.ax.set_xlabel('time (s)')
        #self.ax.set_ylabel('Inter Spike Interval (ms)')

        self.fig.show()

    # TODO: Fazer subplot para varios canais
    def process(self, flags, timestamp):
        for i in range(8):
            if ((1 << i) & flags) != 0:
                if self.ts_now[i] == None:
                    # Register the first timestamp
                    self.first_ts[i] = timestamp
                    self.ts_now[i] = 0.
                    self.timestamps[i].append(0.)
                else:
                    self.isi[i].append((timestamp-self.first_ts[i]) - self.ts_now[i])
                    self.timestamps[i].append(timestamp-self.first_ts[i])
                    self.ts_now[i] = timestamp-self.first_ts[i]

                    # Sets the new data
                    self.line[i].set_xdata(self.timestamps[i][1:])
                    self.line[i].set_ydata(self.isi[i])
            
                    # Adjust axes
                    xmax = self.timestamps[i][-1]
                    xmin = xmax - 30
                    self.ax[i].axis([xmin,xmax,self.ymin,self.ymax])
            
        # Draw figure
        self.fig.canvas.draw()
