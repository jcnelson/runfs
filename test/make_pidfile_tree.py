#!/usr/bin/python

import sys
import os 
import time

pidfile_path = sys.argv[1]

os.mkdir(pidfile_path, 0755)

fd = open( pidfile_path + "/foo", "w" )
fd.write("foo")
fd.close()

fd = open( pidfile_path + "/bar", "w" )
fd.write("bar")
fd.close()

print "pidfile written, waiting to die..."

while True:
    time.sleep(1)

