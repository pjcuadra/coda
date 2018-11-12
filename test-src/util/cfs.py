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

class cfs:

    def clean_cache():

        cmd = 'cfs flushcache'
        proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate(input=b'y')

    def force_reintegrate():

        cmd = 'cfs fr'
        proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def check_conflict(path):

        # Check it's md5 checksum
        cmd = 'cfs lv {}'.format(path)
        output = subprocess.check_output(cmd, shell=True)
        output = output.decode('utf-8')
        check = output.find('*** There are pending conflicts in this volume ***')

        # Check the text wasn't found
        assert check < 0
