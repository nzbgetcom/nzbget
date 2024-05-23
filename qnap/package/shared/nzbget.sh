#!/bin/sh
CONF=/etc/config/qpkg.conf
QPKG_NAME="nzbget"
QPKG_ROOT=`/sbin/getcfg $QPKG_NAME Install_Path -f ${CONF}`
APACHE_ROOT=`/sbin/getcfg SHARE_DEF defWeb -d Qweb -f /etc/config/def_share.info`
export QNAP_QPKG=$QPKG_NAME
DEFAULT_CONF_FILE=$QPKG_ROOT/nzbget/nzbget.conf
CONF_FILE=$DEFAULT_CONF_FILE.qnap

[[ ! -e $CONF_FILE && -e $DEFAULT_CONF_FILE ]] && cp "$DEFAULT_CONF_FILE" "$CONF_FILE"

case "$1" in
  start)
    ENABLED=$(/sbin/getcfg $QPKG_NAME Enable -u -d FALSE -f $CONF)

    if [ "$ENABLED" != "TRUE" ]; then
        echo "$QPKG_NAME is disabled."
        exit 1
    fi

    if /bin/pidof nzbget &>/dev/null; then
        echo "$QPKG_NAME is already running."
        exit 0
    fi

    cd $QPKG_ROOT/nzbget
    $QPKG_ROOT/nzbget/nzbget -c "$CONF_FILE" -D
    ;;

  stop)
    $QPKG_ROOT/nzbget/nzbget -c "$CONF_FILE" -Q

    for ((i=0; i<=10; i++)); do
        /bin/pidof nzbget &>/dev/null || break
        sleep 1
    done
    ;;

  restart)
    $0 stop
    $0 start
    ;;

  remove)
    ;;

  *)
    echo "Usage: $0 {start|stop|restart|remove}"
    exit 1
esac

exit 0
