#! /bin/sh
# leverages example from Mastering Embedded Linux Programming, Ch. 9

case "$1" in
  start) 
    echo "Starting aesdsocket"
    start-stop-daemon --start --name aesdsocket --exec /usr/bin/aesdsocket -- -d
    ;;
  stop)
    echo "Stopping aesdsocket"
    start-stop-daemon --stop --signal SIGTERM --name aesdsocket
    ;;
  *)
    echo "Usage: $0 {start|stop}"
  exit 1
esac

exit 0

