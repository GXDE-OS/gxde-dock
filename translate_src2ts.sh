#!/bin/bash
cd `dirname $0`
/usr/lib/qt6/bin/lupdate -recursive frame/ plugins/ -ts translations/gxde-dock_*.ts
