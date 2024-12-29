#!/usr/bin/python3

# try to be as strict as possible when asserting directory existence. this means that you should use:
# assertTrue(os.path.isdir(...some_directory...))
# assertFalse(os.path.exists(...some_directory...))

import unittest
import subprocess
import os
import shutil
import time
import tempfile
import stat
import platform
import errno


def call(cmd):
	return subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)


def write_to_file(fn, data):
	with open(fn, 'w') as f:
		f.write(data)


def read_from_file(fn):
	with open(fn, 'r') as f:
		return f.read()


def get_dir_contents(directory):
	return [dirs for (_, dirs, _) in os.walk(directory)]


def get_osxfuse_unionfs_mounts():
	#mount_output = call('mount -t osxfuse').decode('utf8')  # for fuse3? or newer macos?
	mount_output = call('mount -t macfuse').decode('utf8')
	return [line.split(' ')[0] for line in mount_output.split('\n') if len(line) > 0]


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
			os.mkdir('%s/%s_dir' % (d, d))
			write_to_file('%s/%s_dir/%s_file' % (d, d, d), d)
			os.mkdir('%s/common_empty_dir' % d)
			os.mkdir('%s/common_dir' % d)
			write_to_file('%s/common_dir/%s_file' % (d, d), d)
			write_to_file('%s/common_dir/common_file' % d, d)
			write_to_file('%s/common_file' % d, d)

		write_to_file('ro1/ro_common_file', 'ro1')
		write_to_file('ro2/ro_common_file', 'ro2')
		write_to_file('rw1/rw_common_file', 'rw1')
		write_to_file('rw2/rw_common_file', 'rw2')
		write_to_file('ro1/common_dir/ro_common_file', 'ro1')
		write_to_file('ro2/common_dir/ro_common_file', 'ro2')
		write_to_file('rw1/common_dir/rw_common_file', 'rw1')
		write_to_file('rw2/common_dir/rw_common_file', 'rw2')

		os.mkdir('union')
		self.mounted = False

	def tearDown(self):
		# In User Mode Linux, fusermount -u union fails with a permission error
		# when trying to lock the fuse lock file.

		if self.mounted:
			if platform.system() == 'Darwin':
				call('umount %s' % self.mount_device)
			else:
				call('fusermount -u union')

		os.chdir(self.original_cwd)
		shutil.rmtree(self.tmpdir)

	def mount(self, cmd):
		if platform.system() == 'Darwin':
			# Need to get the unionfs device name so that we can unmount it later:
			prev_mounts = get_osxfuse_unionfs_mounts()
			# nobrowse prevents Finder from creating spurious icons on the desktop (which
			# sometimes will not go away after the union filesystem is unmounted!)
			call('%s -o nobrowse %s' % (self.unionfs_path, cmd))
			cur_mounts = get_osxfuse_unionfs_mounts()
			self.mount_device = list(set(cur_mounts)-set(prev_mounts))[0]
		else:
			call('%s %s' % (self.unionfs_path, cmd))
		self.mounted = True


class UnionFS_Help(Common, unittest.TestCase):
	def test_help(self):
		res = call('%s --help' % self.unionfs_path).decode()
		self.assertIn('Usage:', res)


class UnionFS_Version(Common, unittest.TestCase):
	def test_help(self):
		res = call('%s --version' % self.unionfs_path).decode()
		self.assertIn('unionfs-fuse version:', res)


# TODO: this is supposed to trigger unionfs_fsync but it doesn't seem to work
class UnionFS_Sync(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('ro1=ro:ro2=ro union')

	def test_sync(self):
		call('sync union')


class UnionFS_RO_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('-o cow ro1=ro:ro2=ro union')

	def test_listing(self):
		lst = ['ro1_file', 'ro2_file', 'ro_common_file', 'common_file', 'ro1_dir', 'ro2_dir', 'common_dir', 'common_empty_dir', ]
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

	def test_rmdir(self):
		with self.assertRaises(OSError):
			os.rmdir('union/ro1_dir')

	def test_rename(self):
		for d in ['ro1', 'ro2', 'ro_common', 'common']:
			with self.assertRaises(PermissionError):
				os.rename('union/%s_file' % d, 'union/%s_file_renamed' % d)


class UnionFS_RW_RO_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('rw1=rw:ro1=ro union')

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file', 'ro1_dir', 'rw1_dir', 'common_dir', 'common_empty_dir', ]
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

	def test_mkdir(self):
		os.mkdir('union/dir')
		self.assertTrue(os.path.isdir('union/dir'))
		self.assertTrue(os.path.isdir('rw1/dir'))
		self.assertFalse(os.path.exists('ro1/dir'))

	# TODO: enable this once we decide a good way to run root-only tests
	#def test_mknod(self):
	#	os.mknod('union/node', stat.S_IFBLK)
	#	self.assertTrue(os.path.exists('union/node'))
	#	self.assertTrue(os.path.exists('rw1/node'))
	#	self.assertFalse(os.path.exists('ro1/node'))

	def test_rmdir(self):
		with self.assertRaises(OSError):
			os.rmdir('union/ro1_dir')
		os.remove('union/rw1_dir/rw1_file')
		os.rmdir('union/rw1_dir')
		self.assertFalse(os.path.exists('union/rw1_dir'))
		self.assertFalse(os.path.exists('rw1/rw1_dir'))
		os.rmdir('union/common_empty_dir')
		# TODO: decide what the correct behaviour should be
		#self.assertFalse(os.path.exists('union/common_empty_dir'))
		#os.remove('union/common_dir/common_file')
		#os.rmdir('union/common_dir')
		#self.assertFalse(os.path.exists('union/common_dir'))


class UnionFS_RW_RO_RO_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('-o cow rw1=rw:ro1=ro:ro2=ro union')

	def test_listing(self):
		lst = ['ro1_file', 'ro2_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file', 'ro1_dir', 'ro2_dir', 'rw1_dir', 'common_dir', 'common_empty_dir', ]
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
		self.assertFalse(os.path.exists('union/rw1_file'))

		os.rename('union/ro1_file', 'union/ro1_file_renamed')
		self.assertEqual(read_from_file('union/ro1_file_renamed'), 'ro1')
		self.assertFalse(os.path.exists('union/ro1_file'))

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

		self.assertFalse(os.path.exists(original))
		self.assertTrue(os.path.isdir(ro_dirs))
		self.assertFalse(os.path.exists('ro1/recursive_cow'))

		# the files in the subdirectories should match after renaming
		self.assertEqual(get_dir_contents('ro1/recursive'), get_dir_contents(renamed))
		self.assertEqual(get_dir_contents(renamed), get_dir_contents(cow_path))

		self.assertTrue(os.path.islink(new_link))

		os.rename('union/common_file', 'union/common_file_renamed')
		self.assertEqual(read_from_file('union/common_file_renamed'), 'rw1')
		self.assertFalse(os.path.exists('union/common_file'))

	def test_rename_common_dir(self):
		common_dir_contents = ['ro2_file', 'ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(os.listdir('union/common_dir')), set(common_dir_contents))

		os.rename('union/common_dir', 'union/common_dir_renamed')

		self.assertFalse(os.path.exists('union/common_dir'))
		self.assertTrue(os.path.isdir('union/common_dir_renamed'))
		self.assertEqual(set(os.listdir('union/common_dir_renamed')), set(common_dir_contents))
		self.assertEqual(read_from_file('union/common_dir_renamed/ro_common_file'), 'ro1')
		self.assertEqual(read_from_file('union/common_dir_renamed/common_file'), 'rw1')

		self.assertTrue(os.path.isdir('ro2/common_dir'))
		self.assertFalse(os.path.exists('ro2/common_dir_renamed'))
		self.assertEqual(set(os.listdir('ro2/common_dir')), set(['ro2_file', 'ro_common_file', 'common_file']))

		self.assertTrue(os.path.isdir('ro1/common_dir'))
		self.assertFalse(os.path.exists('ro1/common_dir_renamed'))
		self.assertEqual(set(os.listdir('ro1/common_dir')), set(['ro1_file', 'ro_common_file', 'common_file']))

		self.assertFalse(os.path.exists('rw1/common_dir'))
		self.assertTrue(os.path.isdir('rw1/common_dir_renamed'))
		self.assertEqual(set(os.listdir('rw1/common_dir_renamed')), set(common_dir_contents))
		self.assertEqual(read_from_file('rw1/common_dir_renamed/ro_common_file'), 'ro1')
		self.assertEqual(read_from_file('rw1/common_dir_renamed/common_file'), 'rw1')

	def test_rename_common_dir_with_whiteout(self):
		os.remove('union/common_dir/ro1_file')
		self.assertFalse(os.path.exists('union/common_dir/ro1_file'))

		os.rename('union/common_dir', 'union/common_dir_renamed')

		self.assertFalse(os.path.exists('union/common_dir_renamed/ro1_file'))

	def test_rename_common_dir_back(self):
		common_dir_contents = ['ro2_file', 'ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file']
		self.assertEqual(set(os.listdir('union/common_dir')), set(common_dir_contents))

		os.rename('union/common_dir', 'union/common_dir_renamed')
		os.rename('union/common_dir_renamed', 'union/common_dir')

		self.assertEqual(set(os.listdir('union/common_dir')), set(common_dir_contents))

	def test_rename_file_masking_directory(self):
		os.rmdir('union/common_empty_dir')
		write_to_file('union/common_empty_dir', 'this is a file')
		self.assertEqual(read_from_file('union/common_empty_dir'), 'this is a file')

		os.rename('union/common_empty_dir', 'union/common_empty_dir_renamed')

		self.assertEqual(read_from_file('union/common_empty_dir_renamed'), 'this is a file')
		self.assertFalse(os.path.exists('union/common_empty_dir'))

	def test_rename_onto_lower(self):
		os.rename('union/rw1_file', 'union/ro1_file')

		self.assertFalse(os.path.exists('union/rw1_file'))
		self.assertEqual(read_from_file('union/ro1_file'), 'rw1')

	def test_rename_onto_higher(self):
		os.rename('union/ro1_file', 'union/rw1_file')

		self.assertFalse(os.path.exists('union/ro1_file'))
		self.assertEqual(read_from_file('union/rw1_file'), 'ro1')

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
			os.rename('union/ro1_file', 'union/ro1_file_%s' % new_name)
		with self.assertRaisesRegex(OSError, '[Errno 36]'):
			os.rename('union/ro1_file%s' % new_name, 'union/ro1_file')

	def test_posix_operations(self):
		# See https://github.com/rpodgorny/unionfs-fuse/issues/25
		# POSIX operations such as chmod, chown, etc. shall not create copies of files.
		ro_dirs = 'ro1/recursive/dirs/1/2/3'
		os.makedirs(ro_dirs)
		union = 'union/recursive'
		cow_path = 'rw1/recursive'

		operations = [
			('access', lambda path: os.access(path, os.F_OK)),
			('chmod', lambda path: os.chmod(path, 0o644)),
			('chown', lambda path: os.chown(path, os.getuid(), os.getgid())),  # no-op chown to avoid permission errors
			('lchown', lambda path: os.lchown(path, os.getuid(), os.getgid())),
			('stat', lambda path: os.stat(path)),
		]

		for name, op in operations:
			op(union)
			self.assertNotEqual(get_dir_contents(union), get_dir_contents(cow_path), name)

	def test_rmdir(self):
		with self.assertRaises(OSError):
			os.rmdir('union/ro1_dir')

		os.remove('union/rw1_dir/rw1_file')
		os.rmdir('union/rw1_dir')
		self.assertFalse(os.path.exists('union/rw1_dir'))
		self.assertFalse(os.path.exists('rw1/rw1_dir'))

		os.rmdir('union/common_empty_dir')
		self.assertFalse(os.path.exists('union/common_empty_dir'))

		os.remove('union/common_dir/common_file')
		os.remove('union/common_dir/ro_common_file')
		os.remove('union/common_dir/rw_common_file')
		os.remove('union/common_dir/ro2_file')
		os.remove('union/common_dir/ro1_file')
		os.remove('union/common_dir/rw1_file')
		os.rmdir('union/common_dir')
		self.assertFalse(os.path.exists('union/common_dir'))


class UnionFS_RO_RW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('ro1=ro:rw1=rw union')

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file', 'ro1_dir', 'rw1_dir', 'common_dir', 'common_empty_dir', ]
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

	def test_rmdir(self):
		with self.assertRaises(OSError):
			os.rmdir('union/ro1_dir')
		os.remove('union/rw1_dir/rw1_file')
		os.rmdir('union/rw1_dir')
		self.assertFalse(os.path.exists('union/rw1_dir'))
		self.assertFalse(os.path.exists('rw1/rw1_dir'))
		# TODO: decide what the correct behaviour should be
		#os.rmdir('union/common_empty_dir')
		#self.assertFalse(os.path.exists('union/common_empty_dir'))
		#os.remove('union/common_dir/common_file')
		#os.rmdir('union/common_dir')
		#self.assertFalse(os.path.exists('union/common_dir'))


class UnionFS_RO_RW_COW_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('-o cow ro1=ro:rw1=rw union')

	def test_listing(self):
		lst = ['ro1_file', 'rw1_file', 'ro_common_file', 'rw_common_file', 'common_file', 'ro1_dir', 'rw1_dir', 'common_dir', 'common_empty_dir', ]
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

	def test_rmdir(self):
		with self.assertRaises(OSError):
			os.rmdir('union/ro1_dir')
		os.remove('union/rw1_dir/rw1_file')
		os.rmdir('union/rw1_dir')
		self.assertFalse(os.path.exists('union/rw1_dir'))
		self.assertFalse(os.path.exists('rw1/rw1_dir'))
		# TODO: decide what the correct behaviour should be
		#os.rmdir('union/common_empty_dir')
		#self.assertFalse(os.path.exists('union/common_empty_dir'))
		#os.remove('union/common_dir/common_file')
		#os.rmdir('union/common_dir')
		#self.assertFalse(os.path.exists('union/common_dir'))


@unittest.skipIf(os.environ.get('RUNNING_ON_TRAVIS_CI'), 'Not supported on Travis')
@unittest.skipIf(platform.system() == 'Darwin', 'Not supported on macOS')
class IOCTL_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('rw1=rw:ro1=ro union')

	def test_debug(self):
		debug_fn = '%s/debug.log' % self.tmpdir
		call('%s -p %r -d on union' % (self.unionfsctl_path, debug_fn))
		self.assertTrue(os.path.isfile(debug_fn))
		self.assertTrue(os.stat(debug_fn).st_size == 0)
		# operations on 'union' results in debug output
		write_to_file('union/rw_common_file', 'hello')
		self.assertRegex(read_from_file(debug_fn), 'unionfs_write')
		read_from_file('union/rw_common_file')
		self.assertRegex(read_from_file(debug_fn), 'unionfs_read')
		os.remove('union/rw_common_file')
		self.assertRegex(read_from_file(debug_fn), 'unionfs_unlink')
		self.assertTrue(os.stat(debug_fn).st_size > 0)

	def test_wrong_args(self):
		with self.assertRaises(subprocess.CalledProcessError) as contextmanager:
			call('%s -xxxx 2>/dev/null' % self.unionfsctl_path)
		ex = contextmanager.exception
		self.assertEqual(ex.returncode, 1)
		self.assertEqual(ex.output, b'')


class UnionFS_RW_RO_COW_RelaxedPermissions_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('-o cow,relaxed_permissions rw1=rw:ro1=ro union')

	def test_access(self):
		self.assertFalse(os.access('union/file', os.F_OK))
		write_to_file('union/file', 'something')
		os.chmod('union/file', 0o222)  # -w--w--w-
		self.assertTrue(os.access('union/file', os.W_OK))
		self.assertFalse(os.access('union/file', os.R_OK))
		self.assertFalse(os.access('union/file', os.X_OK))
		os.chmod('union/file', 0o444)  # r--r--r--
		self.assertTrue(os.access('union/file', os.R_OK))
		self.assertFalse(os.access('union/file', os.W_OK))
		self.assertFalse(os.access('union/file', os.X_OK))

class UnionFS_RW_RW_NoPreserveBranch_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('rw1=rw:rw2=rw union')

	def test_file_move_from_high_branch_to_common(self):
		write_to_file('rw1/rw1_dir/rw1_file2', 'something')
		self.assertTrue(os.access('union/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw1_file2', os.F_OK))

		os.rename('union/rw1_dir/rw1_file2', 'union/common_dir/rw1_file2')

		self.assertFalse(os.access('rw2/common_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('rw1/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/common_dir/rw1_file2', os.F_OK))
		self.assertTrue(os.access('rw1/common_dir/rw1_file2', os.F_OK))
		self.assertEqual(read_from_file('union/common_dir/rw1_file2'), 'something')

	def test_file_move_from_high_branch_to_high_branch(self):
		write_to_file('rw1/rw1_file2', 'something')
		self.assertTrue(os.access('union/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw1_file2', os.F_OK))

		os.rename('union/rw1_file2', 'union/rw1_dir/rw1_file2')

		self.assertFalse(os.access('rw2/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('rw1/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw1_file2', os.F_OK))
		self.assertTrue(os.access('rw1/rw1_dir/rw1_file2', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw1_file2', os.F_OK))
		self.assertEqual(read_from_file('union/rw1_dir/rw1_file2'), 'something')

	def test_file_move_from_low_branch_to_low_branch(self):
		write_to_file('rw2/rw2_file2', 'something')
		self.assertTrue(os.access('union/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw2_file2', os.F_OK))

		os.rename('union/rw2_file2', 'union/rw2_dir/rw2_file2')

		self.assertFalse(os.access('rw1/rw2_dir/rw2_file2', os.F_OK))
		self.assertFalse(os.access('rw2/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_file2', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertEqual(read_from_file('union/rw2_dir/rw2_file2'), 'something')

	def test_file_move_from_low_branch_to_common(self):
		write_to_file('rw2/rw2_dir/rw2_file2', 'something')
		self.assertTrue(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_file2', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir/rw2_file2', 'union/common_dir/rw2_file2')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw2/common_dir/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw2_file2', os.F_OK))

	def test_file_move_from_high_branch_to_low_branch(self):
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_file', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw1_dir/rw1_file', 'union/rw2_dir/rw1_file')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw1/rw2_dir/rw1_file', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))

	def test_file_move_from_low_branch_to_high_branch(self):
		self.assertTrue(os.access('union/rw2_dir/rw2_file', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_file', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir/rw2_file', 'union/rw1_dir/rw2_file')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw2/rw1_dir/rw2_file', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw2_file', os.F_OK))

	def test_file_move_replace_between_branches(self):
		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir/rw2_file', 'union/rw1_dir/rw1_file')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file', os.F_OK))
		self.assertEqual(read_from_file('union/rw1_dir/rw1_file'), 'rw1')

	def test_folder_move_from_low_branch_to_common(self):
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_dir', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir', 'union/common_dir/rw2_dir')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw1/common_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('rw2/common_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_dir', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))

	def test_folder_move_from_low_branch_to_high_branch(self):
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_dir', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir', 'union/rw1_dir/rw2_dir')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw1/rw1_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('rw2/rw1_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_dir', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))

	def test_folder_move_from_high_branch_to_low_branch(self):
		self.assertTrue(os.access('union/rw1_dir', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_dir', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw1_dir', 'union/rw2_dir/rw1_dir')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertFalse(os.access('rw2/rw2_dir/rw1_dir', os.F_OK))
		self.assertFalse(os.access('rw1/rw2_dir/rw1_dir', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_dir', os.F_OK))
		self.assertTrue(os.access('rw1/rw1_dir', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir', os.F_OK))

class UnionFS_RW_RW_PreserveBranch_TestCase(Common, unittest.TestCase):
	def setUp(self):
		super().setUp()
		self.mount('-o preserve_branch rw1=rw:rw2=rw union')

	def test_file_move_from_high_branch_to_common(self):
		# Should have same behavior as when not using preserve_branch
		write_to_file('rw1/rw1_dir/rw1_file2', 'something')
		self.assertTrue(os.access('union/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw1_file2', os.F_OK))

		os.rename('union/rw1_dir/rw1_file2', 'union/common_dir/rw1_file2')

		self.assertFalse(os.access('rw2/common_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('rw1/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/common_dir/rw1_file2', os.F_OK))
		self.assertTrue(os.access('rw1/common_dir/rw1_file2', os.F_OK))
		self.assertEqual(read_from_file('union/common_dir/rw1_file2'), 'something')

	def test_file_move_from_high_branch_to_high_branch(self):
		# Should have same behavior as when not using preserve_branch
		write_to_file('rw1/rw1_file2', 'something')
		self.assertTrue(os.access('union/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw1_file2', os.F_OK))

		os.rename('union/rw1_file2', 'union/rw1_dir/rw1_file2')

		self.assertFalse(os.access('rw2/rw1_dir/rw1_file2', os.F_OK))
		self.assertFalse(os.access('rw1/rw1_file2', os.F_OK))
		self.assertFalse(os.access('union/rw1_file2', os.F_OK))
		self.assertTrue(os.access('rw1/rw1_dir/rw1_file2', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw1_file2', os.F_OK))
		self.assertEqual(read_from_file('union/rw1_dir/rw1_file2'), 'something')

	def test_file_move_from_low_branch_to_low_branch(self):
		# Should have same behavior as when not using preserve_branch
		write_to_file('rw2/rw2_file2', 'something')
		self.assertTrue(os.access('union/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw2_file2', os.F_OK))

		os.rename('union/rw2_file2', 'union/rw2_dir/rw2_file2')

		self.assertFalse(os.access('rw1/rw2_dir/rw2_file2', os.F_OK))
		self.assertFalse(os.access('rw2/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/rw2_file2', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertEqual(read_from_file('union/rw2_dir/rw2_file2'), 'something')

	def test_file_move_from_low_branch_to_common(self):
		write_to_file('rw2/rw2_dir/rw2_file2', 'something')
		self.assertTrue(os.access('union/rw2_dir/rw2_file2', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_file2', os.F_OK))

		os.rename('union/rw2_dir/rw2_file2', 'union/common_dir/rw2_file2')

		self.assertFalse(os.access('rw2/rw2_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('rw2/common_dir/rw2_file2', os.F_OK))
		self.assertTrue(os.access('union/common_dir/rw2_file2', os.F_OK))

	def test_file_move_from_high_branch_to_low_branch(self):
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_file', os.F_OK))

		os.rename('union/rw1_dir/rw1_file', 'union/rw2_dir/rw1_file')

		self.assertFalse(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw1/rw2_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw1_file', os.F_OK))

	def test_file_move_from_low_branch_to_high_branch(self):
		self.assertTrue(os.access('union/rw2_dir/rw2_file', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_file', os.F_OK))

		os.rename('union/rw2_dir/rw2_file', 'union/rw1_dir/rw2_file')

		self.assertFalse(os.access('rw2/rw2_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw1_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw2_file', os.F_OK))

	def test_file_move_to_nonexistent_path(self):
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		self.assertFalse(os.access('union/common_dir/new_dir', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw1_dir/rw1_file', 'union/common_dir/new_dir/rw1_file')
		self.assertEqual(ctx.exception.errno, errno.ENOENT)

		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		self.assertFalse(os.access('rw2/common_dir/new_dir/rw1_file', os.F_OK))

	def test_file_move_replace_in_single_branch(self):
		write_to_file('rw1/rw1_file', 'rw1b')
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_file', os.F_OK))

		os.rename('union/rw1_dir/rw1_file', 'union/rw1_file')

		self.assertFalse(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw1/rw1_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_file', os.F_OK))
		self.assertEqual(read_from_file('union/rw1_file'), 'rw1')

	def test_file_move_replace_between_branches(self):
		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file', os.F_OK))

		with self.assertRaises(OSError) as ctx:
			os.rename('union/rw2_dir/rw2_file', 'union/rw1_dir/rw1_file')
		self.assertEqual(ctx.exception.errno, errno.EXDEV)

		self.assertTrue(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw2_dir/rw2_file', os.F_OK))
		self.assertEqual(read_from_file('union/rw1_dir/rw1_file'), 'rw1')

	def test_folder_move_from_low_branch_to_common(self):
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/common_dir/rw2_dir', os.F_OK))

		os.rename('union/rw2_dir', 'union/common_dir/rw2_dir')

		self.assertFalse(os.access('rw1/common_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('rw2/rw2_dir', os.F_OK))
		self.assertTrue(os.access('rw2/common_dir/rw2_dir', os.F_OK))
		self.assertTrue(os.access('union/common_dir/rw2_dir', os.F_OK))

	def test_folder_move_from_low_branch_to_high_branch(self):
		self.assertTrue(os.access('union/rw2_dir', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_dir', os.F_OK))

		os.rename('union/rw2_dir', 'union/rw1_dir/rw2_dir')

		self.assertFalse(os.access('rw1/rw1_dir/rw2_dir', os.F_OK))
		self.assertFalse(os.access('rw2/rw2_dir', os.F_OK))
		self.assertTrue(os.access('rw2/rw1_dir/rw2_dir', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw2_dir', os.F_OK))

	def test_folder_move_from_high_branch_to_low_branch(self):
		self.assertTrue(os.access('union/rw1_dir', os.F_OK))
		self.assertFalse(os.access('union/rw2_dir/rw1_dir', os.F_OK))

		os.rename('union/rw1_dir', 'union/rw2_dir/rw1_dir')

		self.assertFalse(os.access('rw2/rw2_dir/rw1_dir', os.F_OK))
		self.assertFalse(os.access('rw1/rw1_dir', os.F_OK))
		self.assertTrue(os.access('rw1/rw2_dir/rw1_dir', os.F_OK))
		self.assertTrue(os.access('union/rw2_dir/rw1_dir', os.F_OK))

	def test_permissions_after_creating_directories(self):
		self.assertTrue(os.access('union/rw2_dir/rw2_file', os.F_OK))
		self.assertFalse(os.access('union/rw1_dir/rw2_file', os.F_OK))
		self.assertNotEqual(oct(os.stat('union/rw1_dir').st_mode)[-3:], '760')

		os.chmod('union/rw1_dir', 0o760);
		self.assertEqual(oct(os.stat('rw1/rw1_dir').st_mode)[-3:], '760')
		self.assertEqual(oct(os.stat('union/rw1_dir').st_mode)[-3:], '760')

		os.rename('union/rw2_dir/rw2_file', 'union/rw1_dir/rw2_file')

		self.assertEqual(oct(os.stat('rw2/rw1_dir').st_mode)[-3:], '760')
		self.assertEqual(oct(os.stat('union/rw1_dir').st_mode)[-3:], '760')
		self.assertFalse(os.access('rw2/rw2_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('rw2/rw1_dir/rw2_file', os.F_OK))
		self.assertTrue(os.access('union/rw1_dir/rw2_file', os.F_OK))

	def test_file_move_without_access(self):
		self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
		os.chmod('union/rw2_dir', 0o500);

		try:
			self.assertFalse(os.access('union/rw2_dir/rw1_file', os.F_OK))

			with self.assertRaises(PermissionError) as ctx:
				os.rename('union/rw1_dir/rw1_file', 'union/rw2_dir/rw1_file')
			self.assertEqual(ctx.exception.errno, errno.EACCES)

			self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))
			self.assertFalse(os.access('union/rw2_dir/rw1_file', os.F_OK))
		finally:
			# Ensure teardown can delete the files it needs to:
			os.chmod('union/rw2_dir', 0o700);

	def test_file_move_with_access(self):
		os.mkdir('rw1/rw2_dir')
		os.chmod('rw2/rw2_dir', 0o500);

		try:
			self.assertTrue(os.access('union/rw1_dir/rw1_file', os.F_OK))

			os.rename('union/rw1_dir/rw1_file', 'union/rw2_dir/rw1_file')

			self.assertFalse(os.access('rw1/rw1_dir/rw1_file', os.F_OK))
			self.assertTrue(os.access('rw1/rw2_dir/rw1_file', os.F_OK))
			self.assertTrue(os.access('union/rw2_dir/rw1_file', os.F_OK))
		finally:
			# Ensure teardown can delete the files it needs to:
			os.chmod('rw2/rw2_dir', 0o700);


if __name__ == '__main__':
	unittest.main()
