#! /bin/sh
# leverages example from Mastering Embedded Linux Programming, Ch. 9

case "$1" in
  start) 
    echo "Starting aesdsocket"
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket
    ;;
  stop)
    echo "Stopping aesdsocket"
    start-stop-daemon -K -s SIGTERM aesdsocket
    ;;
  *)
    echo "Usage: $0 {start|stop}"
  exit 1
esac

exit 0

