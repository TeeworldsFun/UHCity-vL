# run.sh
#!/bin/bash

while true
do
    if pidof -x "UHCity-Server" >/dev/null; then
        sleep 2s
    else
        nohup ./UHCity-Server &
    fi
done