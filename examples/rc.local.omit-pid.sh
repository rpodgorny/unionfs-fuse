#!/bin/sh -e
#
# rc.local
#
# This script is executed at the end of each multiuser runlevel.
# Make sure that the script will "exit 0" on success or any other
# value on error.
#
# In order to enable or disable this script just change the execution
# bits.
#
# By default this script does nothing.

# dont't kill unionfs-fuse
dir=/lib/init/rw/sendsigs.omit.d
[ ! -d $dir ] || omitdir=$dir
dir=/var/run/sendsigs.omit.d
[ ! -d $dir ] || omitdir=$dir

if [ -n "$omitdir" ]; then
	for i in `ps ax |grep unionfs | grep -v grep | awk '{print $1}'`; do 
		echo $i >${omitdir}/unionfs.$i; 
	done
fi

exit 0
