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


class Common:
	def setUp(self):
		self._dirs = ['ro1', 'ro2', 'rw1', 'rw2']

		for d in self._dirs:
			os.mkdir(d)
			write_to_file('%s/%s_file' % (d, d), d)
			write_to_file('%s/common_file' % d, d)
		#endfor

		write_to_file('ro1/ro_common_file', 'ro1')
		write_to_file('ro2/ro_common_file', 'ro2')
		write_to_file('rw1/rw_common_file', 'rw1')
		write_to_file('rw2/rw_common_file', 'rw2')

		os.mkdir('union')
	#enddef

	def tearDown(self):
		call('fusermount -u union')

		for d in self._dirs:
			shutil.rmtree(d)
		#endfor

		shutil.rmtree('union')
	#endef
#endclass


class UnionFS_RO_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs -o cow ro1=ro:ro2=ro union')
	#enddef

	def test_listing(self):
		lst = ['ro1_file', 'ro2_file', 'ro_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))
	#enddef

	def test_overlay_order(self):
		self.assertEqual(read_from_file('union/common_file'), 'ro1')
	#enddef

	def test_write(self):
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith

		with self.assertRaises(PermissionError):
			write_to_file('union/ro2_file', 'something')
		#endwith

		with self.assertRaises(PermissionError):
			write_to_file('union/ro_common_file', 'something')
		#endwith

		with self.assertRaises(PermissionError):
			write_to_file('union/common_file', 'something')
		#endwith
	#enddef

	def test_delete(self):
		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')
		#endwith

		with self.assertRaises(PermissionError):
			os.remove('union/ro2_file')
		#endwith

		with self.assertRaises(PermissionError):
			os.remove('union/ro_common_file')
		#endwith

		with self.assertRaises(PermissionError):
			os.remove('union/common_file')
		#endwith
	#enddef
#endclass


class UnionFS_RW_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()

		call('src/unionfs rw1=rw:ro1=ro union')
	#enddef

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))
	#enddef

	def test_delete(self):
		# TODO: shouldn't this be PermissionError?
		# TODO: if not, check the errno (see elswhere)
		with self.assertRaises(OSError):
			os.remove('union/ro1_file')
		#endwith
	#enddef

	def test_write(self):
		# TODO: shouldn't this be the same as above?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef
#endclass


class UnionFS_RW_RO_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs -o cow rw1=rw:ro1=ro union')
	#enddef

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))
	#enddef

	def test_whiteout(self):
		os.remove('union/ro1_file')

		self.assertNotIn('ro1_file', os.listdir('union'))
		self.assertIn('ro1_file', os.listdir('ro1'))
	#enddef

	def test_cow(self):
		write_to_file('union/ro1_file', 'something')

		self.assertEqual(read_from_file('union/ro1_file'), 'something')
		self.assertEqual(read_from_file('ro1/ro1_file'), 'ro1')
		self.assertEqual(read_from_file('rw1/ro1_file'), 'something')
	#enddef
#endclass


class UnionFS_RO_RW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs ro1=ro:rw1=rw union')
	#enddef

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))
	#enddef

	def test_delete(self):
		# TODO: shouldn't this be a PermissionError?
		# TODO: if not, check the errno to make sure it's: [Errno 30] Read-only file system
		with self.assertRaises(OSError):
			os.remove('union/ro1_file')
		#endwith
	#enddef

	def test_write(self):
		# TODO: shouldn't this be the same error as for the delete?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef
#endclass


class UnionFS_RO_RW_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs -o cow ro1=ro:rw1=rw union')
	#enddef

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))
	#enddef

	def test_delete(self):
		# TODO: shouldn't this be and OSError (see above)
		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')
		#endwith
	#enddef

	def test_write(self):
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef
#endclass


class StatsTestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs -o stats rw1=rw:ro1=ro union')
	#enddef

	def test_stats_file_exists(self):
		self.assertIn('stats', os.listdir('union'))
	#enddef
#endclass


if __name__ == '__main__':
	unittest.main()
#endif
