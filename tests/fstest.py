#!/usr/bin/python

import subprocess
import unittest
import shutil
import random
import string
import time
import uuid
import os

HOMEDIR = os.path.expanduser('~')

MARCFS_MOUNTDIR = HOMEDIR + '/marcfs-mount/'
MARCFS_CACHEDIR = HOMEDIR + '/marcfs-cache/'

MARCFS_USER = 'yoru.sulfur@list.ru'
MARCFS_PASS = '5yKGLVT9QmQC'

MARCFS_TEST_DIR = MARCFS_MOUNTDIR + 'test/'


class TestMarcfsFuseOperations(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.mount_marcfs()
        shutil.rmtree(MARCFS_TEST_DIR, ignore_errors=True)

    @classmethod
    def mount_marcfs(cls):
        os.mkdir(MARCFS_MOUNTDIR)
        os.mkdir(MARCFS_CACHEDIR)
        subprocess.check_output(
            './marcfs %s -ousername=%s,password=%s,cachedir=%s'
                  % (MARCFS_MOUNTDIR, MARCFS_USER, MARCFS_PASS, MARCFS_CACHEDIR), shell=True)
        print('MARC-FS mounted')
        # at this point FS is mounted

    @classmethod
    def tearDownClass(cls):
        cls.umount_marcfs()

    @classmethod
    def umount_marcfs(cls):
        os.system('fusermount -u %s' % MARCFS_MOUNTDIR)
        # at this point FS is unmounted, dirs should be empty
        os.rmdir(MARCFS_MOUNTDIR)
        os.rmdir(MARCFS_CACHEDIR)
        print('MARC-FS unmounted')

    def setUp(self):
        os.mkdir(MARCFS_TEST_DIR)

    def tearDown(self):
        shutil.rmtree(MARCFS_TEST_DIR)

    def random_string(self):
        return ''.join([random.choice(string.ascii_lowercase) for i in range(random.randrange(50))])

    def test_create_dirs(self):
        os.mkdir(MARCFS_TEST_DIR + 'dir1')
        pass

    def test_create_file(self):
        fname = MARCFS_TEST_DIR + str(uuid.uuid4())
        with open(fname, 'w'):
            os.utime(fname, None)

    def test_write_to_file(self):
        fname = MARCFS_TEST_DIR + str(uuid.uuid4())
        text = self.random_string()
        with open(fname, 'w') as f:
            f.write(text)
        time.sleep(1) # wait for FUSE to release the file
        self.assertEqual(os.stat(fname).st_size, len(text), 'File size differs from written data length!')
        return fname, text

    def test_read_from_file(self):
        fname, text = self.test_write_to_file()
        with open(fname, 'r') as f:
            self.assertEqual(f.read(), text, 'Written and read content is not equal!')

    def test_read_nonexisting_file_fails(self):
        with self.assertRaises(IOError):
            open('non-existing-file', 'r')

    def test_append_to_file(self):
        fname, text = self.test_write_to_file()
        text2 = 'miles separate us'
        with open(fname, 'a') as f:
            f.write(text2)
        with open(fname, 'r') as f:
            self.assertEqual(f.read(), text + text2, 'Appended and read content is not equal!')

    def test_truncate_file(self):
        fname, text = self.test_write_to_file()
        os.truncate(fname, 0)
        self.assertEqual(os.stat(fname).st_size, 0, 'Truncated file still has non-zero length!')

    def test_rename_file(self):
        fname, text = self.test_write_to_file()
        fname2 = MARCFS_TEST_DIR + 'file2'
        os.rename(fname, fname2)
        self.assertFalse(os.path.exists(fname), 'Renamed file still exists!')
        self.assertTrue(os.path.isfile(fname2), 'Renamed file is not created!')
        self.assertEqual(os.stat(fname2).st_size, len(text), 'Renamed file size differs from original file size!')

    def test_move_file(self):
        fname, text = self.test_write_to_file()
        dirname2 = MARCFS_TEST_DIR + 'testdir1/'
        fname2 = dirname2 + 'file1'
        os.mkdir(dirname2)
        os.rename(fname, fname2)
        self.assertFalse(os.path.exists(fname), 'Moved file still exists!')
        self.assertTrue(os.path.isfile(fname2), 'Moved file is not created!')
        self.assertEqual(os.stat(fname2).st_size, len(text), 'Moved file size differs from original file size!')

    def test_move_file_to_existing(self):
        fname, text = self.test_write_to_file()
        fname2, text2 = self.test_write_to_file()
        os.rename(fname, fname2)
        self.assertFalse(os.path.exists(fname), 'Moved file still exists!')
        self.assertTrue(os.path.isfile(fname2), 'Moved file not exists!')
        self.assertEqual(os.stat(fname2).st_size, len(text), 'Moved file size differs from expected!')

    def test_create_folder(self, dname = 'testdir/'):
        dirname1 = MARCFS_TEST_DIR + dname
        os.mkdir(dirname1)
        self.assertTrue(os.path.isdir(dirname1), 'Creating directory failed!')
        return dirname1

    def test_rename_folder(self):
        dirname1 = self.test_create_folder()
        dirname2 = MARCFS_TEST_DIR + 'testdir2/'
        os.rename(dirname1, dirname2)
        self.assertFalse(os.path.exists(dirname1), 'Renamed dir still exists!')
        self.assertTrue(os.path.isdir(dirname2), 'Renaming dir failed!')

    def test_move_empty_folder(self):
        dirname1 = self.test_create_folder()
        dirname2 = MARCFS_TEST_DIR + 'testdir2/'
        moved_dirname = dirname2 + 'nested_dir'
        os.mkdir(dirname2)
        os.rename(dirname1, moved_dirname)
        self.assertFalse(os.path.exists(dirname1), 'Moved dir still exists!')
        self.assertTrue(os.path.isdir(moved_dirname), 'Moved dir failed!')

    def test_remove_file(self):
        fname, text = self.test_write_to_file()
        os.unlink(fname)
        self.assertFalse(os.path.exists(fname), 'Deleting file failed!')

    def test_remove_nonexisting_file_fails(self):
        with self.assertRaises(OSError):
            os.unlink('some-nonexisting-file')


    def test_remove_empty_folder(self):
        dirname1 = self.test_create_folder()
        os.rmdir(dirname1)
        self.assertFalse(os.path.isdir(dirname1), 'Removed dir still exists!')

    def test_remove_non_empty_folder_fails(self):
        with self.assertRaises(OSError):
            dirname1 = self.test_create_folder()
            fname = dirname1 + 'file'
            with open(fname, 'w') as f:
                f.write('but in our hearts')
            os.rmdir(dirname1)

    def test_remove_folder_recursive(self):
        dirname1 = self.test_create_folder()
        fname = dirname1 + 'file'
        with open(fname, 'w') as f:
            f.write(' we share the same dream')
        shutil.rmtree(dirname1)
        self.assertFalse(os.path.exists(dirname1), 'Removed dir still exists!')

    def test_public_link(self):
        fname, text = self.test_write_to_file()
        plink_fname = fname + '.marcfs-link'
        with open(plink_fname, 'w'):
            os.utime(fname, None)
        time.sleep(1)
        with open(plink_fname, 'r') as f:
            plink_text = f.read()
            self.assertTrue('https://cloud.mail.ru/public/' in plink_text, 'Public link does not contain url!')
            self.assertTrue(fname.split('/')[-1] in plink_text, 'Public link does not contain file name!')

    # Test to check issue https://gitlab.com/Kanedias/MARC-FS/issues/24 is resolved
    # The problem was that in tryFillDir we find dir in sorted cache and then iterate next,
    # expecting that next path will have it with slash, like this: dir/
    # Initial expectation was based on the fact that no other char can be there before slash.
    # But there are other chars like '.' or '-' that have numeric value lower than '/'
    # so after initial filling of some dirs their content was "disappearing" from cache
    def test_filldir_works_issue24(self):
        dirname1 = self.test_create_folder('test/')
        self.test_create_folder('test./')
        fname = 'testfile'
        with open(dirname1 + fname, 'w') as f:
            f.write("there's one for the sorrow")
        files = os.listdir(dirname1)
        self.assertTrue(len(files) == 1, 'There should be one file present in folder!')
        self.assertEqual(files[0], fname, 'There should be a file named testfile in the folder')



unittest.main()
