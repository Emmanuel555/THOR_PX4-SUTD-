#!/bin/sh
#
# PX4IO interface init script.
#
<<<<<<< HEAD
if [ $USE_IO = yes -a $IO_PRESENT = yes ]
=======

if px4io start
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
then
	# Allow PX4IO to recover from midair restarts.
	px4io recovery

	# Adjust PX4IO update rate limit.
	px4io limit 400
else
	echo "PX4IO start failed" >> $LOG_FILE
	tune_control play -m ${TUNE_ERR}
fi
