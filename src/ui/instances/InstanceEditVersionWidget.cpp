#include "InstanceEditVersionWidget.h"

#include <QtWidgets>

extern "C" {
	#include "../../core/Betacraft.h"
	#include "../../core/VersionList.h"
}

InstanceEditVersionWidget::InstanceEditVersionWidget(QWidget* parent)
	: QWidget{ parent } {
	char tr[256];

	_layout = new QGridLayout(this);
	_searchButton = new QPushButton(this);
	_searchTextbox = new QLineEdit(this);
	_menu = new QTabBar(this);
	_versionsTreeView = new QTreeView();
	_versionListAll = new QStandardItemModel(this);
	_versionListRelease = new QStandardItemModel(this);
	_versionListOldBeta = new QStandardItemModel(this);
    _versionListOldAlpha = new QStandardItemModel(this);
    _versionListSnapshot = new QStandardItemModel(this);
    _versionListSearch = new QStandardItemModel(this);

	bc_translate("instance_version_search_button", tr);
	_searchButton->setText(QString(tr));

	bc_translate("instance_version_search_placeholder", tr);
	_searchTextbox->setPlaceholderText(QString(tr));

	versionListInit();

	_versionsTreeView->header()->setStretchLastSection(true);
	_versionsTreeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	_versionsTreeView->setIndentation(0);
	_versionsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

	bc_translate("instance_version_tab_all", tr);
	_menu->addTab(QString(tr));
	bc_translate("instance_version_tab_release", tr);
	_menu->addTab(QString(tr));
	_menu->addTab("Beta");
	_menu->addTab("Alpha");
	_menu->addTab("Snapshot");

	_layout->addWidget(_searchTextbox, 0, 0, 1, 10);
	_layout->addWidget(_searchButton, 0, 10, 1, 1);
	_layout->addWidget(_menu, 1, 0, 1, 11);
	_layout->addWidget(_versionsTreeView, 2, 0, 1, 11);

	_layout->setAlignment(Qt::AlignVCenter);

	_layout->setSpacing(5);
	_layout->setContentsMargins(10, 10, 10, 10);

	setStyleSheet("QLabel { font-size: 14px; }");
	setLayout(_layout);

	connect(_searchButton, SIGNAL(released()), this, SLOT(onSearchButtonClicked()));
	connect(_menu, SIGNAL(currentChanged(int)), this, SLOT(onMenuTabChanged(int)));
}

void InstanceEditVersionWidget::versionListInit() {
	bc_versionlist* version_list = bc_versionlist_fetch();

	for (int i = 0; i < version_list->versions_len; i++) {
        QList<QStandardItem*> items;
        QStandardItem* item = new QStandardItem(QString(version_list->versions[i].id));

        QVariant q;
        q.setValue(QString(version_list->versions[i].url));
        item->setData(q);

        QString dateFormatted = QDateTime::fromString(QString(version_list->versions[i].releaseTime), Qt::ISODate).toString("yyyy-MM-dd HH:mm:ss");
        QStandardItem* releaseTime = new QStandardItem(dateFormatted);

        items.append(item);
        items.append(releaseTime);

        _versionListAll->appendRow(items);

		QList<QStandardItem*> itemsClone;
		itemsClone.append(item->clone());
		itemsClone.append(releaseTime->clone());

		if (strcmp(version_list->versions[i].type, "release") == 0) {
			_versionListRelease->appendRow(itemsClone);
		} else if (strcmp(version_list->versions[i].type, "old_beta") == 0) {
			_versionListOldBeta->appendRow(itemsClone);
		} else if (strcmp(version_list->versions[i].type, "old_alpha") == 0) {
			_versionListOldAlpha->appendRow(itemsClone);
		} else if (strcmp(version_list->versions[i].type, "snapshot") == 0) {
			_versionListSnapshot->appendRow(itemsClone);
		}
	}

	_versionsTreeView->setModel(_versionListAll);
	_versionsTreeView->model()->setHeaderData(0, Qt::Horizontal, "Name");
	_versionsTreeView->model()->setHeaderData(1, Qt::Horizontal, "Released");

	free(version_list);
}

void InstanceEditVersionWidget::onMenuTabChanged(int index) {
	switch (index) {
		case 0:
			_versionsTreeView->setModel(_versionListAll);
			break;
		case 1:
			_versionsTreeView->setModel(_versionListRelease);
			break;
		case 2:
			_versionsTreeView->setModel(_versionListOldBeta);
			break;
		case 3:
			_versionsTreeView->setModel(_versionListOldAlpha);
			break;
		case 4:
			_versionsTreeView->setModel(_versionListSnapshot);
			break;
		default:
			break;
	}

	_versionsTreeView->model()->setHeaderData(0, Qt::Horizontal, "Name");
	_versionsTreeView->model()->setHeaderData(1, Qt::Horizontal, "Released");

	setSelectedInstance();
}

void InstanceEditVersionWidget::setSelectedInstance() {
	QStandardItemModel* model = static_cast<QStandardItemModel*>(_versionsTreeView->model());

	for (int i = 0; i < model->rowCount(); i++) {
		if (model->item(i)->text().compare(_version) == 0) {
			model->item(i, 0)->setBackground(QBrush(QColor(206, 250, 208, 255)));
			model->item(i, 1)->setBackground(QBrush(QColor(206, 250, 208, 255)));
			break;
		}
	}
}

void InstanceEditVersionWidget::setInstance(bc_instance instance) {
	_version = QString(instance.version);
	setSelectedInstance();
}

std::pair<QString, QString> InstanceEditVersionWidget::getSettings() {
	QModelIndexList indexes = _versionsTreeView->selectionModel()->selectedIndexes();
	QStandardItemModel* model = static_cast<QStandardItemModel*>(_versionsTreeView->model());

	if (indexes.size() > 0) {
		QStandardItem* item = model->itemFromIndex(indexes.at(0));
		return { item->text(), item->data().value<QString>()};
	}

	return { _version, NULL };
}

void InstanceEditVersionWidget::onSearchButtonClicked() {
	onMenuTabChanged(_menu->currentIndex());
	_versionListSearch->clear();

	QString search = _searchTextbox->text().trimmed().toLower();

	QStandardItemModel* model = static_cast<QStandardItemModel*>(_versionsTreeView->model());

	for (int i = 0; i < model->rowCount(); i++) {
		if (model->item(i)->text().contains(search, Qt::CaseInsensitive)) {
            QList<QStandardItem*> itemsClone;
            itemsClone.append(model->item(i, 0)->clone());
            itemsClone.append(model->item(i, 1)->clone());

			_versionListSearch->appendRow(itemsClone);
		}
	}

	_versionsTreeView->setModel(_versionListSearch);
	setSelectedInstance();

	_versionsTreeView->model()->setHeaderData(0, Qt::Horizontal, "Name");
	_versionsTreeView->model()->setHeaderData(1, Qt::Horizontal, "Released");
}

void InstanceEditVersionWidget::keyPressEvent(QKeyEvent* e) {
	if (e->key() == Qt::Key_Return) {
		onSearchButtonClicked();
	}
}
