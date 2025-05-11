#!/bin/bash
cd `dirname $0`
lupdate -recursive frame/ plugins/ -ts translations/dde-control-center_*.ts
