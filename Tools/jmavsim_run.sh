#! /bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR/jMAVSim"

tcp_port=4560
extra_args=
baudrate=921600
device=
ip="127.0.0.1"
while getopts ":b:d:p:qr:f:i:" opt; do
	case $opt in
		b)
			baudrate=$OPTARG
			;;
		d)
			device="$OPTARG"
			;;
		i)
			ip="$OPTARG"
			;;
		p)
			tcp_port=$OPTARG
			;;
		q)
			extra_args="$extra_args -qgc"
			;;
		r)
			extra_args="$extra_args -r $OPTARG"
			;;
		f)
			extra_args="$extra_args -f $OPTARG"
			;;
		\?)
			echo "Invalid option: -$OPTARG" >&2
			exit 1
			;;
	esac
done

if [ "$device" == "" ]; then
	device="-tcp $ip:$tcp_port"
else
	device="-serial $device $baudrate"
fi

ant create_run_jar copy_res
cd out/production

java -XX:GCTimeRatio=20 -Djava.ext.dirs= -jar jmavsim_run.jar $device $extra_args
ret=$?
if [ $ret -ne 0 -a $ret -ne 130 ]; then # 130 is Ctrl-C
	# if the start of java fails, it's probably because the GC option is not
	# understood. Try starting without it
	java -Djava.ext.dirs= -jar jmavsim_run.jar $device $extra_args
fi

