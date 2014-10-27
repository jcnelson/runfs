#!/usr/bin/python

import sys
import os 
import time

pidfile_path = sys.argv[1]

os.mkdir(pidfile_path, 0755)

print "pidfile written, waiting to die..."

while True:
    time.sleep(1)

