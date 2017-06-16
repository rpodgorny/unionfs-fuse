#!/usr/bin/python

import os
import signal
import hashlib
import subprocess
import inspect
import multiprocessing
import time

# global stuffs
TestDir = "testdir"
LoDir = TestDir + "/lower"
UpDir = TestDir + "/upper"
MntDir = TestDir + "/mnt"
LoFile = LoDir + "/file1"
UpFile = UpDir + "/file1"
MntFile = MntDir + "/file1"
OrigSize = 1048576
DbgFile = TestDir + "/log"

# path to unionfs executable
UfsBin = "./src/unionfs"

def create_file(fname, size):
	fh = open(fname, 'w+')
	fh.truncate(size)
	fh.close()

def write_file(fname, data, offset = 0):
	fh = open(fname, 'r+')
	fh.seek(offset)
	fh.write(data)
	fh.close()

def create_testbed():
	os.system("rm -rf " + TestDir)
	os.mkdir(TestDir)
	os.mkdir(LoDir)
	os.mkdir(UpDir)
	os.mkdir(MntDir)
	create_file(LoFile, OrigSize)

def do_setup():
	print "Creating test setup..."
	create_testbed()
	os.system(UfsBin
		+ " -o cow -o auto_unmount,debug_file=" + DbgFile
		+ " "
		+ UpDir + "=RW:" + LoDir + "=RO "
		+ MntDir)

def undo_setup():
	print "Destroying test setup..."
	res = subprocess.check_output(["pgrep", "-f", UfsBin, "-u",  str(os.getuid())])
	os.kill(int(res), signal.SIGTERM)

# test code
def test_func_checker(fc):

	time.sleep(5)
	sz = os.path.getsize(MntFile)
	if sz == OrigSize:
		fc.value = 0
	else:
		print "Size mismatch " + str(sz) + " vs " + str(OrigSize) + ". line no: " + str(inspect.currentframe().f_lineno)

# test code
def test_func_writer(fc):

	smalldata = bytearray(['X']*100)
	write_file(MntFile, smalldata, 2000)

	sz = os.path.getsize(MntFile)
	if sz == OrigSize:
		fc.value = 0
	else:
		print "Size mismatch " + str(sz) + " vs " + str(OrigSize) + ". line no: " + str(inspect.currentframe().f_lineno)

# set up function: do setup and fork a process to execute the tests
def main():
	t1 = multiprocessing.Value('i', -1)
	t2 = multiprocessing.Value('i', -1)
	do_setup()
	p1 = multiprocessing.Process(target=test_func_writer, args=(t1,))
	p2 = multiprocessing.Process(target=test_func_checker, args=(t2,))
	p1.start()
	p2.start()
	p1.join()
	p2.join()
	undo_setup()
	if t1.value < 0:
		print "***** Writer failed. *****"
	elif t2.value < 0:
		print "****** Checker failed. *******"
	else:
		print "***** Test passed *******"

if __name__ == "__main__":
	main()

