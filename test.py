#!/usr/bin/python3

import unittest
import subprocess
import os
import shutil


def call(cmd):
	return subprocess.call(cmd, shell=True)
#enddef


class UnionFSTestCase(unittest.TestCase):
	def setUp(self):
		os.mkdir('ro')
		os.mkdir('rw')
		os.mkdir('union')

		call('src/unionfs -o cow rw=rw:ro=ro union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		shutil.rmtree('union')
		shutil.rmtree('rw')
		shutil.rmtree('ro')
	#endef

	def test_listing(self):
		with open('ro/ro_file', 'w') as f:
			f.write('ro_file')
		#endwith

		with open('rw/rw_file', 'w') as f:
			f.write('rw_file')
		#endwith

		self.assertTrue(set(['ro_file', 'rw_file']) == set(os.listdir('union')))

		os.remove('rw/rw_file')
		os.remove('ro/ro_file')
	#enddef
#endclass


class StatsTestCase(unittest.TestCase):
	def setUp(self):
		os.mkdir('ro')
		os.mkdir('rw')
		os.mkdir('union')

		call('src/unionfs -o stats rw=rw:ro=ro union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		shutil.rmtree('union')
		shutil.rmtree('rw')
		shutil.rmtree('ro')
	#enddef

	def test_stats_file_exists(self):
		self.assertTrue('stats' in os.listdir('union'))
	#enddef
#endclass


if __name__ == '__main__':
	unittest.main()
#endif
