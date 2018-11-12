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

coda_realm = "coda.cs.cmu.edu"
coda_volume_path = os.path.join("usr", "pcuadrac", "testdata")
coda_non_replicated_volume_path = os.path.join("usr", "pcuadrac", "testdata",
                                               "reintegration",
                                               "non_replicated")

test_metadata = {
    "files" : [ 
        {
            "file": "files/Earth.mov",
            "md5sum": "041b37a2a13cfde6cc6edde829f313cb",
            "sha256sum": "e48229e766c36fc990281f6fa18c3612ba875965e29eccc6019a81a5390af35b"
        }
    ],
    "movies" : [ 
        {
            "file": "Earth.mov",
            "md5sum": "041b37a2a13cfde6cc6edde829f313cb",
            "sha256sum": "e48229e766c36fc990281f6fa18c3612ba875965e29eccc6019a81a5390af35b"
        }
    ]
}
