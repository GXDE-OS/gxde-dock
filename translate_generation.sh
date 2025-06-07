#!/bin/bash
# this file is used to auto-generate .qm file from .ts file.
# author: shibowen at linuxdeepin.com
lupdate -recursive . -ts translations/gxde-dock_*.ts
ts_list=(`ls translations/*.ts`)

for ts in "${ts_list[@]}"
do
    printf "\nprocess ${ts}\n"
    lupdate "${ts}"
    lrelease "${ts}"
done
