rm /tmp/myinit.log

make build
sleep 1
make run

ps -a | grep sleep
sleep 2
pid=$(pgrep sleep | head -n 1)
echo "$pid"
kill -1 $pid
ps -a | grep sleep
sleep 2
ps -a | grep sleep

cat /tmp/myinit.log