#!/bin/sh
cd /home/steamos/cod2-vr-work || exit 1
exec /usr/bin/python /home/steamos/cod2-vr-work/launch_vr.py "$@" >> /home/steamos/cod2-vr-work/game-launch.log 2>&1
