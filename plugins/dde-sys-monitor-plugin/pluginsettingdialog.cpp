#include "pluginsettingdialog.h"
#include "ui_pluginsettingdialog.h"

pluginSettingDialog::pluginSettingDialog(Settings *settings,QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pluginSettingDialog)
{
    ui->setupUi(this);
    if(settings->efficient==DisplayContentSetting::CPUMEM)ui->onlyCPUMEMRadioButton->setChecked(true);
    else if(settings->efficient==DisplayContentSetting::NETSPEED)ui->onlyNetSpeedRadioButton->setChecked(true);
    else if(settings->efficient==ALL)ui->showAllRadioButton->setChecked(true);
    else ui->showAllAndCPUAndBattery->setChecked(true);

    ui->lineHeightSpinBox->setValue(settings->lineHeight);
}

pluginSettingDialog::~pluginSettingDialog()
{
    delete ui;
}

void pluginSettingDialog::getDisplayContentSetting(Settings *settings)
{
    if(ui->onlyCPUMEMRadioButton->isChecked())settings->efficient=DisplayContentSetting::CPUMEM;
    else if(ui->onlyNetSpeedRadioButton->isChecked())settings->efficient=DisplayContentSetting::NETSPEED;
    else if(ui->showAllRadioButton->isChecked())settings->efficient=ALL;
    else settings->efficient=DisplayContentSetting::AllAndTempAndBattery;

    settings->lineHeight=ui->lineHeightSpinBox->value();
}
