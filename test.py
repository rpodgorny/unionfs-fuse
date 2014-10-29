#!/usr/bin/python3

import unittest
import subprocess
import os
import shutil


def call(cmd):
	return subprocess.call(cmd, shell=True)
#enddef


class UnionFS_RO_RO_TestCase(unittest.TestCase):
	def setUp(self):
		os.mkdir('ro1')
		os.mkdir('ro2')
		os.mkdir('union')

		call('src/unionfs -o cow ro1=ro:ro2=ro union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		shutil.rmtree('union')
		shutil.rmtree('ro1')
		shutil.rmtree('ro2')
	#endef

	def test_write(self):
		with open('ro1/ro1_file', 'w') as f:
			f.write('ro1_file')
		#endwith

		with self.assertRaises(PermissionError):
			f = open('union/ro1_file', 'w')
			f.close()
		#endwith
	#enddef

	def test_delete(self):
		with open('ro1/ro1_file', 'w') as f:
			f.write('ro1_file')
		#endwith

		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')
		#endwith
	#enddef
#endclass


class UnionFS_RW_RO_TestCase(unittest.TestCase):
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

		self.assertEqual(set(['ro_file', 'rw_file']), set(os.listdir('union')))
	#enddef

	def test_whiteout(self):
		with open('ro/ro_file', 'w') as f:
			f.write('ro_file')
		#endwith

		with open('rw/rw_file', 'w') as f:
			f.write('rw_file')
		#endwith

		self.assertIn('ro_file', os.listdir('union'), 'ro_file')

		os.remove('union/ro_file')

		self.assertNotIn('ro_file', os.listdir('union'))
		self.assertIn('ro_file', os.listdir('ro'))
	#enddef

	def test_cow(self):
		with open('ro/ro_file', 'w') as f:
			f.write('ro_file')
		#endwith

		with open('rw/rw_file', 'w') as f:
			f.write('rw_file')
		#endwith

		with open('union/ro_file', 'w') as f:
			f.write('something')
		#endwith

		with open('union/ro_file', 'r') as f:
			self.assertEqual(f.read(), 'something')
		#endwith

		with open('ro/ro_file', 'r') as f:
			self.assertEqual(f.read(), 'ro_file')
		#endwith

		with open('rw/ro_file', 'r') as f:
			self.assertEqual(f.read(), 'something')
		#endwith
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
