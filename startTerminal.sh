#!/bin/bash
#set -x
#trap read debug

echo "startTerminal.sh started at `date +"%Y-%m-%d %H:%M:%S"`" > /tmp/startTerminal.log

sleep 10

echo "Running `date +"%Y-%m-%d %H:%M:%S"`" >> /tmp/startTerminal.log

source /home/peter/.bashrc
echo "PATH: $PATH" >> /tmp/startTerminal.log

export DISPLAY=:0.0
echo "DISPLAY: $DISPLAY" >> /tmp/startTerminal.log

export PATH="/home/peter/miniconda3/bin:$PATH"
export PATH="/home/peter/.local/bin:$PATH"
echo "PATH: $PATH" >> /tmp/startTerminal.log

cd /home/peter/lend-me-your-face-main/

echo "/usr/bin/gnome-terminal -- /home/peter/lend-me-your-face-main/odeep.sh" >> /tmp/startTerminal.log
dbus-launch /usr/bin/gnome-terminal -- /home/peter/lend-me-your-face-main/odeep.sh >> /tmp/startTerminal.log 2>&1

