#  BLURB gpl
#
#                            Coda File System
#                               Release 7
#
#           Copyright (c) 1987-2018 Carnegie Mellon University
#                   Additional copyrights listed below
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public Licence Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
#
#                         Additional copyrights
#                            none currently
#

import pytest
import subprocess
import random
import os
from shutil import copyfile, rmtree
from util.testdata import test_metadata
from util.utils import *
from util.cfs import cfs

max_read_size = 3*1024*1024

class TestClass(object):

    def check_exits(self, file_path):
        assert os.path.exists(file_path)
        assert os.path.isfile(file_path)

    def setup_method(self, method):
        # Clear the cache
        cfs.clean_cache()

    def random_read_access_body(self, file_path, file_obj):
        max = get_file_size(file_path)
        f = open(file_path, "rb")
        
        print('pos: {}, count: {}'.format(max, file_path))
        
        # 3 random reads of the file
        for i in range(3):
            rand_seek = random.randrange(max)
            rand_size_max = max - rand_seek
            if (rand_size_max > max_read_size):
                rand_size_max = max_read_size 
            rand_size = random.randrange(rand_size_max)
            
            print('pos: {}, count: {}'.format(rand_seek, rand_size))
            
            f.seek(rand_seek)
            f.read(rand_size)
            
        check_md5sum(file_path, file_obj['md5sum'])

    def test_random_read_access(self):
        for file_obj in test_metadata["files"]:
            file_path = get_file_path(file_obj['file'])
            self.random_read_access_body(file_path, file_obj)

    def test_copy_and_reintegrate(self):
        for file_obj in test_metadata["files"]:
            src = get_file_path(file_obj['file'])
            dst_tmp_dir = get_coda_tmp_path()
            dst = os.path.join(dst_tmp_dir, file_obj['file'])

            # Create Directory
            directory =  os.path.dirname(dst)
            if not os.path.exists(directory):
                os.makedirs(directory)
            
            copyfile(src, dst)
            
            cfs.force_reintegrate()

            # Remove the temporary directory
            if os.path.exists(dst_tmp_dir):
                rmtree(dst_tmp_dir)
                cfs.force_reintegrate()

    @pytest.mark.skip(reason="Copying a VASTRO to an already existing file fails.")
    def test_override_and_reintegrate(self):
        for file_obj in test_metadata["files"]:
            src = get_file_path(file_obj['file'])
            dst = get_coda_tmp_path(file_obj['file'])
            
            copyfile(src, dst)
            
            cfs.force_reintegrate()
            
            copyfile(src, dst)
            
            cfs.force_reintegrate()

    def test_check_sha256sum(self):
        for file_obj in test_metadata["files"]:
            file_path = get_file_path(file_obj['file'])
            self.check_exits(file_path)
            check_sha256sum(file_path, file_obj['sha256sum'])

    def test_check_md5sum(self):
        for file_obj in test_metadata["files"]:
            file_path = get_file_path(file_obj['file'])
            self.check_exits(file_path)
            check_md5sum(file_path, file_obj['md5sum'])

    def copy_body(self, file_path, file_obj, tmpdir):
        self.check_exits(file_path)
        new_copy_path = os.path.join(tmpdir, file_obj['file'].split('/')[-1])
        copyfile(file_path, new_copy_path)
        check_md5sum(new_copy_path, file_obj['md5sum'])

    def test_copy(self, tmpdir):            
        for file_obj in test_metadata["files"]:
            file_path = get_file_path(file_obj['file'])
            self.copy_body(file_path, file_obj, tmpdir)

    def cat_to_null_body(self, file_path, file_obj):
        self.check_exits(file_path)
        
        # Play the video
        cmd = 'cat {} >> /dev/null'.format(file_path)
        output = subprocess.run(cmd, shell=True)
        assert output.returncode == 0
        
        check_md5sum(file_path, file_obj['md5sum'])
        check_sha256sum(file_path, file_obj['sha256sum'])

    def test_cat_to_null(self):
        for file_obj in test_metadata["files"]:
            file_path = get_file_path(file_obj['file'])
            self.cat_to_null_body(file_path, file_obj)
