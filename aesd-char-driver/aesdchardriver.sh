#!/bin/sh
### BEGIN INIT INFO
# Provides:          aesdchardriver
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start aesdchardriver at boot time
# Description:       Enable service provided by aesdchardriver.
### END INIT INFO

NAME=aesd-char-driver
DESC="AESD Character Driver"

case "$1" in
start)
    echo "Loading $DESC: $NAME"
    /etc/aesd-char-driver/aesdchar_load
    ;;
stop)
    echo "Stopping $DESC: $NAME"
    /etc/aesd-char-driver/aesdchar_unload
    ;;
restart)
    echo "Restarting $DESC: $NAME"
    $0 stop
    $0 start
    ;;
status)
    ;;
*)
    echo "Usage: $SCRIPTNAME {start|stop|restart|status}" >&2
    exit 3
    ;;
esac

exit 0