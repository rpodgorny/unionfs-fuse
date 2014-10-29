#!/usr/bin/python3

import unittest
import subprocess
import os
import shutil


def call(cmd):
	return subprocess.call(cmd, shell=True)
#enddef


def write_to_file(fn, data):
	with open(fn, 'w') as f:
		f.write(data)
	#endwith
#enddef


def read_from_file(fn):
	with open(fn, 'r') as f:
		return f.read()
	#endwith
#enddef


class UnionFS_RO_RO_TestCase(unittest.TestCase):
	def setUp(self):
		os.mkdir('ro1')
		os.mkdir('ro2')
		os.mkdir('union')

		write_to_file('ro1/ro1_file', 'ro1_file')

		call('src/unionfs -o cow ro1=ro:ro2=ro union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		shutil.rmtree('union')
		shutil.rmtree('ro1')
		shutil.rmtree('ro2')
	#endef

	def test_write(self):
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef

	def test_delete(self):
		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')
		#endwith
	#enddef
#endclass


class UnionFS_RW_RO_COW_TestCase(unittest.TestCase):
	def setUp(self):
		os.mkdir('ro')
		os.mkdir('rw')
		os.mkdir('union')

		write_to_file('ro/ro_file', 'ro_file')
		write_to_file('rw/rw_file', 'rw_file')

		call('src/unionfs -o cow rw=rw:ro=ro union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		shutil.rmtree('union')
		shutil.rmtree('rw')
		shutil.rmtree('ro')
	#endef

	def test_listing(self):
		self.assertEqual(set(['ro_file', 'rw_file']), set(os.listdir('union')))
	#enddef

	def test_whiteout(self):
		os.remove('union/ro_file')

		self.assertNotIn('ro_file', os.listdir('union'))
		self.assertIn('ro_file', os.listdir('ro'))
	#enddef

	def test_cow(self):
		write_to_file('union/ro_file', 'something')

		self.assertEqual(read_from_file('union/ro_file'), 'something')
		self.assertEqual(read_from_file('ro/ro_file'), 'ro_file')
		self.assertEqual(read_from_file('rw/ro_file'), 'something')
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
		self.assertIn('stats', os.listdir('union'))
	#enddef
#endclass


if __name__ == '__main__':
	unittest.main()
#endif
