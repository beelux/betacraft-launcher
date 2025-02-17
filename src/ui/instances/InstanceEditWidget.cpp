#include "InstanceEditWidget.h"

#include <QtWidgets>

extern "C" {
	#include "../../core/Betacraft.h"
	#include "../../core/Network.h"
}

bc_instance instance;

InstanceEditWidget::InstanceEditWidget(QWidget* parent)
	: QWidget{ parent }
{
	char tr[256];

	_layout = new QGridLayout(this);
	_menu = new QTabWidget(this);
	_instanceSaveButtonLayout = new QGridLayout();
	_instanceSaveButton = new QPushButton(this);
	_instanceSaveButtonWidget = new QWidget(this);

	bc_translate("instance_save_button", tr);
	_instanceSaveButton->setText(QString(tr));

	_instanceEditAppearanceWidget = new InstanceEditAppearanceWidget();
	_instanceEditArgumentsWidget = new InstanceEditArgumentsWidget();

	_instanceSaveButtonLayout->setSpacing(5);
	_instanceSaveButtonLayout->setContentsMargins(10, 10, 10, 10);
	_instanceSaveButtonLayout->addWidget(_instanceSaveButton);
	_instanceSaveButtonWidget->setLayout(_instanceSaveButtonLayout);

	_menu->setStyleSheet("QTabWidget::pane { border: 0; }");

	bc_translate("instance_tab_appearance", tr);
	_menu->addTab(_instanceEditAppearanceWidget, QString(tr));

	if (betacraft_online == 1) {
        _instanceEditVersionWidget = new InstanceEditVersionWidget();
        _instanceEditModsWidget = new InstanceEditModsWidget();

		bc_translate("instance_tab_version", tr);
        _menu->addTab(_instanceEditVersionWidget, QString(tr));
        _menu->addTab(_instanceEditModsWidget, "Mods");
	}

	bc_translate("instance_tab_arguments", tr);
	_menu->addTab(_instanceEditArgumentsWidget, QString(tr));

	_layout->addWidget(_menu, 0, 0, 1, 11);
	_layout->addWidget(_instanceSaveButtonWidget, 1, 0, 1, 11);

	bc_translate("instance_edit_title", tr);
	setWindowTitle(QString(tr));

	_layout->setSpacing(0);
	_layout->setContentsMargins(0, 0, 0, 0);

	resize(600, 450);
	setMinimumSize(600, 450);
	setLayout(_layout);

	connect(_instanceSaveButton, SIGNAL(released()), this, SLOT(onInstanceSaveButtonClicked()));

	setWindowModality(Qt::ApplicationModal);
}

void InstanceEditWidget::onInstanceSaveButtonClicked()
{
	bc_instance* appearanceSettings = _instanceEditAppearanceWidget->getSettings();
	std::pair<QString, QString> versionSettings = _instanceEditVersionWidget->getSettings();
	bc_mod_version_array* mods = _instanceEditModsWidget->getSettings();
    bc_instance* arguments = _instanceEditArgumentsWidget->getSettings();

	snprintf(instance.name, sizeof(instance.name), "%s", appearanceSettings->name);
	instance.width = appearanceSettings->width;
	instance.height = appearanceSettings->height;
	instance.fullscreen = appearanceSettings->fullscreen;
	instance.show_log = appearanceSettings->show_log;

    snprintf(instance.program_args, sizeof(instance.program_args), "%s", arguments->program_args);
    snprintf(instance.jvm_args, sizeof(instance.jvm_args), "%s", arguments->jvm_args);

	if (versionSettings.first.compare(instance.version) != 0) {
		// Download version json
		bc_network_download(versionSettings.second.toStdString().c_str(), "versions", 0);
	}

	snprintf(instance.version, sizeof(instance.version), "%s", versionSettings.first.toStdString().c_str());

	bc_instance_update(&instance);

	bc_mod_list_installed_move(mods, instance.path);

	delete mods;
	delete appearanceSettings;
	delete arguments;

	emit signal_instanceSettingsSaved();
}

void InstanceEditWidget::setInstance(bc_instance i)
{
	_menu->setCurrentIndex(0);
	instance = i;

	_instanceEditAppearanceWidget->setInstance(i);
    _instanceEditArgumentsWidget->setInstance(i);
	if (betacraft_online == 1) {
		_instanceEditVersionWidget->setInstance(i);
		_instanceEditModsWidget->setInstance(i);
	}
}
