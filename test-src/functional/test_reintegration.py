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
import os
from util.utils import get_coda_root_path, get_coda_path
from util.cfs import cfs
from util.testdata import coda_volume_path, coda_non_replicated_volume_path


class TestClass(object):
    
    def check_exits(self, file):
        assert os.path.exists(file)
        assert os.path.isfile(file)

    def setup_method(self, method):
        # Clear the cache
        cfs.clean_cache()

    def test_single_file_create(self):

        vol = os.path.join(get_coda_root_path(), coda_volume_path)

        file_path = os.path.join(vol, 'reintegration', 'replicated',
                                 'single_file_create.txt')

        # Ensure that the file doesn't exist
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

        with open(file_path, 'w') as fh:
            fh.close()

        cfs.force_reintegrate()
        cfs.check_conflict(vol)

        # Delete it after success
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

    def test_single_file_create_and_store(self):

        vol = os.path.join(get_coda_root_path(), coda_volume_path)

        file_path = os.path.join(vol, 'reintegration', 
                                 'replicated',
                                 'single_file_create_and_store.txt')

        # Ensure that the file doesn't exist
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

        with open(file_path, 'w') as fh:
            fh.write('TEST CONTENT')
            fh.close()

        cfs.force_reintegrate()
        cfs.check_conflict(vol)

        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

    @pytest.mark.skip(reason="Non-replicated volumes reintegration isn't "
                             "currently supported in coda.cs.cmu.edu")
    def test_non_replicated_single_file_create(self):

        vol = os.path.join(get_coda_root_path(), 
                           coda_non_replicated_volume_path)

        file_path = os.path.join(vol, 'single_file_create.txt')

        # Ensure that the file doesn't exist
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

        with open(file_path, 'w') as fh:
            fh.close()

        cfs.force_reintegrate()
        cfs.check_conflict(vol)

        # Delete it after success
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

    @pytest.mark.skip(reason="Non-replicated volumes reintegration isn't "
                             "currently supported in coda.cs.cmu.edu")
    def test_non_replicated_single_file_create_and_store(self):

        vol = os.path.join(get_coda_root_path(),
                           coda_non_replicated_volume_path)

        file_path = os.path.join(vol, 'single_file_create_and_store.txt')

        # Ensure that the file doesn't exist
        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()

        with open(file_path, 'w') as fh:
            fh.write('TEST CONTENT')
            fh.close()

        cfs.force_reintegrate()
        cfs.check_conflict(vol)

        if os.path.exists(file_path):
            os.remove(file_path)
            cfs.force_reintegrate()
