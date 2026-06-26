#!/bin/bash
# this file is used to auto-generate .qm file from .ts file.
# author: shibowen at linuxdeepin.com

LUPDATE=/usr/lib/qt6/bin/lupdate
LRELEASE=/usr/lib/qt6/bin/lrelease

$LUPDATE -recursive . -ts translations/gxde-dock_*.ts
ts_list=(`ls translations/*.ts`)

for ts in "${ts_list[@]}"
do
    printf "\nprocess ${ts}\n"
    $LUPDATE "${ts}"
    $LRELEASE "${ts}"
done
