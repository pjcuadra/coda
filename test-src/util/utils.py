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

import os
import subprocess
import uuid
from .testdata import coda_realm, coda_volume_path


def get_coda_root_path(realm=coda_realm):

    ret_path = os.path.join('/coda', realm)

    return ret_path


def get_coda_path(realm, path):

    ret_path = os.path.join('/coda', realm)
    ret_path = os.path.join(ret_path, path)

    return ret_path


def get_coda_tmp_path():

    ret_path = os.path.join('/coda', coda_realm)
    ret_path = os.path.join(ret_path, coda_volume_path)
    ret_path = os.path.join(ret_path, "tmp")
    ret_path = os.path.join(ret_path, str(uuid.uuid4()))

    return ret_path


def get_file_size(file):
    return os.path.getsize(file)


def check_sha256sum(file, expected_checksum):

    # Check it's md5 checksum
    cmd = 'sha256sum {}'.format(file)
    output = subprocess.check_output(cmd, shell=True)
    checksum = output.decode('utf-8').split(' ')
    assert len(checksum) > 1

    checksum = checksum[0]
    assert checksum == expected_checksum


def check_md5sum(file, expected_checksum):

    # Check it's md5 checksum
    cmd = 'md5sum {}'.format(file)
    output = subprocess.check_output(cmd, shell=True)
    checksum = output.decode('utf-8').split(' ')
    assert len(checksum) > 1

    checksum = checksum[0]
    print(file)
    assert checksum == expected_checksum


def get_file_path(file):
    return get_coda_path(coda_realm, 
                         os.path.join(coda_volume_path, file))


def get_movie_path(movie):
    return get_file_path(os.path.join('movies', movie))


def get_slide_path(slide):
    return get_file_path(os.path.join('slides', slide))
