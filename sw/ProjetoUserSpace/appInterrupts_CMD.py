#!/usr/bin/env python
# -*- encoding: utf-8 -*-

from PyQt4.QtGui import QInputDialog, QMessageBox, QApplication
from PyQt4.QtCore import QSocketNotifier, SIGNAL
import os, sys, struct

# Same numbers used on the driver (see defines)
__FIFO_DAT__ = '/dev/rtf0'
__FIFO_CMD__ = '/dev/rtf1'

receivers = []

# QSocketNotifier is a interface for monitoring a file descriptor
# It associates a function to an event on that descriptor
class LambdaQSocketNotifier(QSocketNotifier):
    """ Creates a QSocketNotifiers that activates a function. """
    def __init__(self, fd, type, func):
        QSocketNotifier.__init__(self, fd, type)
        self.func = func
        self.connect(self, SIGNAL('activated(int)'), self.func)

def fifo_send(devnode, data):
    sys.stderr.write('fifo_send devnode=%s, data=%s\n' % (devnode, data.encode('hex')))
    sys.stderr.flush()
    fd = os.open(devnode, os.O_WRONLY)
    os.write(fd, data)
    os.close(fd)

def ask_and_send_command():
    global __FIFO_CMD__
    ok = False
    cmd = -1
    while ((not ok) or ((cmd != 0) and (cmd != 1))):
        command, ok = QInputDialog.getText(None, "Command", "'0' for disabling; '1' for enabling; '86' for exit")
        cmd = int(command)
        if cmd == 86:
            sys.exit(-1);
        if ((cmd != 0)  and (cmd != 1)):
            msg = QMessageBox(QMessageBox.Information, "Error", "Command must be '0' or '1'")
            btn = msg.addButton("Ok", QMessageBox.AcceptRole)
            msg.exec_()
    fifo_send(__FIFO_CMD__, struct.pack('<I', cmd))
    return cmd

def start():
    """ Initialize FIFOs and add the given receivers. """
    global receivers
    global __FIFO_CMD__, __FIFO_DAT__

    fd = os.open(__FIFO_DAT__, os.O_RDONLY)
    def callback_dat(fd_):
        """ Callback for the receiving DATA. """
        # receive data
        data = os.read(fd_, 8)
        if data == '':
            return
        data, = struct.unpack('<Q', data)
        # TODO: Interpret data
    __recv_notifier__ = LambdaQSocketNotifier(fd, QSocketNotifier.Read, callback_dat)

    while True:
        ask_and_send_command()

app = QApplication(sys.argv)
