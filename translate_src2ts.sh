#!/bin/bash
cd `dirname $0`
lupdate -recursive frame/ plugins/ -ts translations/gxde-dock_*.ts
