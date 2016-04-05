#!/usr/bin/python3

import unittest
import subprocess
import os
import shutil
import time
import tempfile


def call(cmd):
	return subprocess.check_output(cmd, shell=True)


def write_to_file(fn, data):
	with open(fn, 'w') as f:
		f.write(data)


def read_from_file(fn):
	with open(fn, 'r') as f:
		return f.read()

def get_dir_contents(directory):
	return [dirs for (_, dirs, _) in os.walk(directory)]


class Common:
	def setUp(self):
		self.unionfs_path = os.path.abspath('src/unionfs')
		self.unionfsctl_path = os.path.abspath('src/unionfsctl')

		self.tmpdir = tempfile.mkdtemp()
		self.original_cwd = os.getcwd()
		os.chdir(self.tmpdir)

		self._dirs = ['ro1', 'ro2', 'rw1', 'rw2']

		for d in self._dirs:
			os.mkdir(d)
			write_to_file('%s/%s_file' % (d, d), d)
			write_to_file('%s/common_file' % d, d)

		write_to_file('ro1/ro_common_file', 'ro1')
		write_to_file('ro2/ro_common_file', 'ro2')
		write_to_file('rw1/rw_common_file', 'rw1')
		write_to_file('rw2/rw_common_file', 'rw2')

		os.mkdir('union')

	def tearDown(self):
		# In User Mode Linux, fusermount -u union fails with a permission error
		# when trying to lock the fuse lock file.

		if os.environ.get('RUNNING_ON_TRAVIS_CI'):
			# TODO: investigate the following
			# the sleep seems to be needed for some users or else the umount fails
			# anyway, everything works fine on my system, so why wait? ;-)
			# if it fails for someone, let's find the race and fix it!
			# actually had to re-enable it because travis-ci is one of the bad cases
			time.sleep(1)

			call('umount union')
		else:
			call('fusermount -u union')

		os.chdir(self.original_cwd)

		shutil.rmtree(self.tmpdir)


class UnionFS_RO_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s -o cow ro1=ro:ro2=ro union' % self.unionfs_path)

	def test_listing(self):
		lst = ['ro1_file', 'ro2_file', 'ro_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))

	def test_overlay_order(self):
		self.assertEqual(read_from_file('union/common_file'), 'ro1')

	def test_write(self):
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')

		with self.assertRaises(PermissionError):
			write_to_file('union/ro2_file', 'something')

		with self.assertRaises(PermissionError):
			write_to_file('union/ro_common_file', 'something')

		with self.assertRaises(PermissionError):
			write_to_file('union/common_file', 'something')

	def test_delete(self):
		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')

		with self.assertRaises(PermissionError):
			os.remove('union/ro2_file')

		with self.assertRaises(PermissionError):
			os.remove('union/ro_common_file')

		with self.assertRaises(PermissionError):
			os.remove('union/common_file')

	def test_rename(self):
		for d in ['ro1', 'ro2', 'ro_common', 'common']:
			with self.assertRaises(PermissionError):
				os.rename('union/%s_file' % d, 'union/%s_file_renamed' % d)


class UnionFS_RW_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s rw1=rw:ro1=ro union' % self.unionfs_path)

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))

	def test_delete(self):
		# TODO: shouldn't this be PermissionError?
		with self.assertRaisesRegex(OSError, '[Errno 30]'):
			os.remove('union/ro1_file')

	def test_write(self):
		# TODO: shouldn't this be the same as above?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('union/new_file'), 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))

	def test_rename(self):
		# TODO: how should the common file behave?
		#for fn in ['ro1_file', 'ro_common_file', 'common_file']:
		for fn in ['ro1_file', 'ro_common_file']:
			with self.assertRaises(PermissionError):
				os.rename('union/%s' % fn, 'union/%s_renamed' % fn)

		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')

	def test_copystat(self):
		shutil.copystat('union/ro1_file', 'union/rw1_file')


class UnionFS_RW_RO_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s -o cow rw1=rw:ro1=ro union' % self.unionfs_path)

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))

	def test_whiteout(self):
		os.remove('union/ro1_file')

		self.assertNotIn('ro1_file', os.listdir('union'))
		self.assertIn('ro1_file', os.listdir('ro1'))

	def test_cow(self):
		write_to_file('union/ro1_file', 'something')

		self.assertEqual(read_from_file('union/ro1_file'), 'something')
		self.assertEqual(read_from_file('ro1/ro1_file'), 'ro1')
		self.assertEqual(read_from_file('rw1/ro1_file'), 'something')

	def test_cow_and_whiteout(self):
		write_to_file('union/ro1_file', 'something')
		os.remove('union/ro1_file')

		self.assertFalse(os.path.isfile('union/ro_file'))
		self.assertFalse(os.path.isfile('rw1/ro_file'))
		self.assertEqual(read_from_file('ro1/ro1_file'), 'ro1')

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('union/new_file'), 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))

	def test_rename(self):
		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		os.rename('union/ro1_file', 'union/ro1_file_renamed')
		self.assertEqual(read_from_file('union/ro1_file_renamed'), 'ro1')

		# See https://github.com/rpodgorny/unionfs-fuse/issues/25
		ro_dirs = 'ro1/recursive/dirs/1/2/3'
		os.makedirs(ro_dirs)
		ro_link = 'ro1/symlink'
		os.symlink('recursive', ro_link)
		self.assertTrue(os.path.islink(ro_link))

		original = 'union/recursive'
		renamed = 'union/recursive_cow'
		cow_path = 'rw1/recursive_cow'
		new_link = 'union/newsymlink'

		os.rename(original, renamed)
		os.rename('union/symlink', new_link)

		self.assertFalse(os.path.isdir(original))
		self.assertTrue(os.path.isdir(ro_dirs))

		# the files in the subdirectories should match after renaming
		self.assertEqual(get_dir_contents('ro1/recursive'), get_dir_contents(renamed))
		self.assertEqual(get_dir_contents(renamed), get_dir_contents(cow_path))

		self.assertTrue(os.path.islink(new_link))

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')

	def test_copystat(self):
		shutil.copystat('union/ro1_file', 'union/rw1_file')

	def test_rename_fifo(self):
		ro_fifo = 'ro1/fifo'
		os.mkfifo(ro_fifo)
		self.assertTrue(os.path.lexists(ro_fifo))

		old_fifo = 'union/fifo'
		new_fifo = 'union/newfifo'

		os.rename(old_fifo, new_fifo)
		self.assertTrue(os.path.lexists(new_fifo))
		self.assertFalse(os.path.lexists(old_fifo))
		self.assertTrue(os.path.lexists(ro_fifo))

		# TODO test that ro1/fifo is still functional

	def test_rename_long_name(self):
		# renaming with pathlen > PATHLEN_MAX should fail
		new_name = 1000 * 'a'
		with self.assertRaisesRegex(OSError, '[Errno 36]'):
			os.rename('union/ro1_file', 'union/ro1_file_%s' %new_name)
		with self.assertRaisesRegex(OSError, '[Errno 36]'):
			os.rename('union/ro1_file%s' %new_name, 'union/ro1_file')

	def test_posix_operations(self):
		# See https://github.com/rpodgorny/unionfs-fuse/issues/25
		# POSIX operations such as chmod, chown, etc. shall not create
		# copies of files.
		ro_dirs = 'ro1/recursive/dirs/1/2/3'
		os.makedirs(ro_dirs)
		union = 'union/recursive'
		cow_path = 'rw1/recursive'

		operations = [
			lambda path:
				os.access(path, os.F_OK),
			lambda path:
				os.chmod(path, 0o644),
			lambda path:
				#  no-op chown to avoid permission errors
				os.chown(path, os.getuid(), os.getgid()),
			lambda path:
				os.lchown(path, os.getuid(), os.getgid()),
			lambda path:
				os.stat(path)
			]

		for op in operations:
			op(union)
			self.assertNotEqual(get_dir_contents(union), get_dir_contents(cow_path))


class UnionFS_RO_RW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s ro1=ro:rw1=rw union' % self.unionfs_path)

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))

	def test_delete(self):
		# TODO: shouldn't this be a PermissionError?
		with self.assertRaisesRegex(OSError, '[Errno 30]'):
			os.remove('union/ro1_file')

	def test_write(self):
		# TODO: shouldn't this be the same error as for the delete?
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')

	def test_write_new(self):
		with self.assertRaises(PermissionError):
		    write_to_file('union/new_file', 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))
		self.assertNotIn('new_file', os.listdir('rw1'))

	def test_rename(self):
		with self.assertRaises(PermissionError):
			os.rename('union/ro1_file', 'union/ro1_file_renamed')

		# TODO: shouldn't this work?
		#os.rename('union/rw1_file', 'union/rw1_file_renamed')
		#self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')


class UnionFS_RO_RW_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s -o cow ro1=ro:rw1=rw union' % self.unionfs_path)

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(lst), set(os.listdir('union')))

	def test_delete(self):
		# TODO: shouldn't this be and OSError (see above)
		with self.assertRaises(PermissionError):
			os.remove('union/ro1_file')

	def test_write(self):
		with self.assertRaises(PermissionError):
			write_to_file('union/ro1_file', 'something')

	def test_write_new(self):
		write_to_file('union/new_file', 'something')
		self.assertEqual(read_from_file('rw1/new_file'), 'something')
		self.assertNotIn('new_file', os.listdir('ro1'))

	def test_rename(self):
		with self.assertRaises(PermissionError):
			os.rename('union/ro1_file', 'union/ro1_file_renamed')

		os.rename('union/rw1_file', 'union/rw1_file_renamed')
		self.assertEqual(read_from_file('union/rw1_file_renamed'), 'rw1')

		# TODO: how should the common file behave?
		#os.rename('union/common_file', 'union/common_file_renamed')
		#self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')


@unittest.skipIf(os.environ.get('RUNNING_ON_TRAVIS_CI'), 'Not supported on Travis')
class IOCTL_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		call('%s rw1=rw:ro1=ro union' % self.unionfs_path)

	def test_debug(self):
		debug_fn = '%s/debug.log' % self.tmpdir
		call('%s -p %r -d on union' % (self.unionfsctl_path, debug_fn))
		self.assertTrue(os.path.isfile(debug_fn))
		self.assertTrue(os.stat(debug_fn).st_size == 0)
		# operations on 'union' results in debug output
		write_to_file('union/rw_common_file', 'hello')
		self.assertRegex(read_from_file(debug_fn), 'unionfs_write')
		read_from_file('union/rw_common_file')
		self.assertRegex(read_from_file(debug_fn),'unionfs_read')
		os.remove('union/rw_common_file')
		self.assertRegex(read_from_file(debug_fn),'unionfs_unlink')
		self.assertTrue(os.stat(debug_fn).st_size > 0)

	def test_wrong_args(self):
		with self.assertRaises(subprocess.CalledProcessError) as contextmanager:
			call('%s -xxxx 2>/dev/null' % self.unionfsctl_path)
		ex = contextmanager.exception
		self.assertEqual(ex.returncode, 1)
		self.assertEqual(ex.output, b'')


if __name__ == '__main__':
	unittest.main()
