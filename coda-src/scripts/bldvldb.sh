#!/bin/sh
# BLURB gpl
# 
#                            Coda File System
#                               Release 5
# 
#           Copyright (c) 1987-1999 Carnegie Mellon University
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
#*/

THISHOST=`hostname | tr A-Z a-z`
REMOTE=/vice/vol/remote

PATH=/sbin:/usr/sbin:$PATH
export PATH
cd /vice/vol/remote
SERVERS=""

# Get the locally generated /vice/vol/VolumeList from 
#  - all servers (if argc = 1)
#  - the listed servers (if argc > 1) 

if [ $#  = 0 ]; then
	SERVERS=`awk '{ print $1 }' /vice/db/servers`
else
    for i in $* ; do
        NEWSERVER=`awk '{ print $1 }' /vice/db/servers | grep $i `
	SERVERS="$NEWSERVER $SERVERS"
    done
fi

echo "Fetching /vice/vol/Volumelist from $SERVERS"

for server in $SERVERS
do 

    updatefetch -h ${server} -r /vice/vol/VolumeList -l \
	${REMOTE}/${server}.list.new

    if [ -r ${REMOTE}/${server}.list.new ]; then
        mv ${REMOTE}/${server}.list.new ${REMOTE}/${server}.list
    else 
        echo "Trouble getting new list for server $server"
    fi

done

# Make on big list called composite
cd ${REMOTE}; cat *.list> ../BigVolumeList

# Make a new vldb from the list
volutil makevldb  /vice/vol/BigVolumeList
