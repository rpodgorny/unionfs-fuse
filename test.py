#!/usr/bin/python3

import unittest
import subprocess
import os
import shutil
import time
import tempfile


def call(cmd):
	return subprocess.check_output(cmd, shell=True)
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
		# TODO: investigate the following
		# the sleep seems to be needed for some users or else the umount fails
		# anyway, everything works fine on my system, so why wait? ;-)
		# if it fails for someone, let's find the race and fix it!
		time.sleep(1)

		# When running the testsuite within usermodelinux, /dev/mtab might not
		# exist. In that case, fusermount -u will fail. We thus fall back to
		# umount.
		if os.path.isfile('/dev/mtab'):
			call('fusermount -u union')
		else:
			call('umount union')

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

	def test_rename(self):
		for d in ['ro1', 'ro2', 'ro_common', 'common']:
			with self.assertRaises(PermissionError):
				os.rename('union/%s_file' % d, 'union/%s_file_renamed' % d)
			#endwith
		#endfor
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
		with self.assertRaisesRegex(OSError, '[Errno 30]'):
			os.remove('union/ro1_file')
		#endwith
	#enddef

	def test_write(self):
		# TODO: shouldn't this be the same as above?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('union/new_file'), 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))
	#enddef

	def test_rename(self):
		# TODO: how should the common file behave?
		#for fn in ['ro1_file', 'ro_common_file', 'common_file']:
		for fn in ['ro1_file', 'ro_common_file']:
			with self.assertRaises(PermissionError):
				os.rename('union/%s' % fn, 'union/%s_renamed' % fn)
			#endwith
		#endfor

		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')
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

	def test_cow_and_whiteout(self):
		write_to_file('union/ro1_file', 'something')
		os.remove('union/ro1_file')

		self.assertFalse(os.path.isfile('union/ro_file'))
		self.assertFalse(os.path.isfile('rw1/ro_file'))
		self.assertEqual(read_from_file('ro1/ro1_file'), 'ro1')
	#enddef

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('union/new_file'), 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))
	#enddef

	def test_rename(self):
		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		os.rename('union/ro1_file', 'union/ro1_file_renamed')
		self.assertEqual(read_from_file('union/ro1_file_renamed'), 'ro1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')
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
		with self.assertRaisesRegex(OSError, '[Errno 30]'):
			os.remove('union/ro1_file')
		#endwith
	#enddef

	def test_write(self):
		# TODO: shouldn't this be the same error as for the delete?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')
		#endwith
	#enddef

	def test_write_new(self):
		with self.assertRaises(PermissionError):
		    write_to_file('union/new_file', 'something')
		#endwith
		self.assertNotIn('new_file', os.listdir('ro1'))
		self.assertNotIn('new_file', os.listdir('rw1'))
	#enddef

	def test_rename(self):
		with self.assertRaises(PermissionError):
			os.rename('union/ro1_file', 'union/ro1_file_renamed')
		#endwith

		# TODO: shouldn't this work?
		#os.rename('union/rw1_file', 'union/rw1_file_renamed')
		#self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')
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

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))
	#enddef

	def test_rename(self):
		with self.assertRaises(PermissionError):
			os.rename('union/ro1_file', 'union/ro1_file_renamed')
		#endwith

		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')
	#enddef
#endclass


@unittest.skipIf(not os.environ.get('RUNNING_ON_TRAVIS_CI'), 'Not supported on Travis')
class IOCTL_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('src/unionfs rw1=rw:ro1=ro union')
		self._temp_dir = tempfile.mkdtemp()
	#enddef

	def test_debug(self):
		temp_file = os.path.join(self._temp_dir, 'debug.log')
		call('src/unionfsctl -p %r -d on union' % temp_file)
		self.assertTrue(os.path.isfile(temp_file))
		self.assertTrue(os.stat(temp_file).st_size == 0)
		# operations on 'union' results in debug output
		write_to_file('union/rw_common_file', 'hello')
		self.assertRegex(read_from_file(temp_file), 'unionfs_write')
		read_from_file('union/rw_common_file')
		self.assertRegex(read_from_file(temp_file),'unionfs_read')
		os.remove('union/rw_common_file')
		self.assertRegex(read_from_file(temp_file),'unionfs_unlink')
		self.assertTrue(os.stat(temp_file).st_size > 0)
	#enddef

	def test_wrong_args(self):
		with self.assertRaises(subprocess.CalledProcessError) as contextmanager:
			call('src/unionfsctl -xxxx 2>/dev/null')
		#endwith
		ex = contextmanager.exception
		self.assertEqual(ex.returncode, 1)
		self.assertEqual(ex.output, b'')
	#enddef

	def tearDown(self):
		super().tearDown()
		shutil.rmtree(self._temp_dir)
	#enddef
#endclass


if __name__ == '__main__':
	unittest.main()
#endif
