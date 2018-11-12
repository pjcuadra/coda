
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
import av
import random
import PIL
from shutil import copyfile
from util.testdata import test_metadata
from util.utils import *
from util.cfs import cfs

movies = test_metadata["movies"]

class TestClass(object):

    def check_exits(self, file):
        assert os.path.exists(file)
        assert os.path.isfile(file)

    def setup_method(self, method):
        # Clear the cache
        cfs.clean_cache()

    @pytest.mark.skip(reason="No need to play the video test_read_all_frames"
                             "tests the same behavior")
    def test_play_movies(self):
        for movie in movies:
            file = get_movie_path(movie['file'])
            
            self.check_exits(file)

            # Play the video
            cmd = 'vlc --play-and-exit {}'.format(file)
            output = subprocess.run(cmd, shell=True)
            assert output.returncode == 0

            # Check it's checksums
            check_md5sum(file, movie['md5sum'])
            check_sha256sum(file, movie['sha256sum'])

            # Check the downloaded size

    def test_read_all_frames(self):
        for movie in movies:
            file = get_movie_path(movie['file'])

            self.check_exits(file)
            print(file)

            vid = av.open(file)

            for frame in vid.decode(video=0):
                frame.to_image()

            # Check it's checksums
            check_md5sum(file, movie['md5sum'])
            check_sha256sum(file, movie['sha256sum'])
