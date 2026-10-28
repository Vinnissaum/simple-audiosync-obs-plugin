// SPDX-License-Identifier: GPL-2.0-or-later
#include "camera-sync-dock.hpp"

#include <string>
#include <vector>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include "controller.hpp"
#include "viewmodel/strings.hpp"
#include "viewmodel/view-model.hpp"

namespace {

const char *DOCK_ID = "camera-sync-dock";

// Sync Dock's README, which links the pattern files, until a release hosts its own (READ-73).
const char *PATTERN_URL = "https://github.com/norihiro/obs-audio-video-sync-dock/blob/main/README.md";

// The camera checklist's columns: the tick and the name, the status, and the marker counts shown with Details.
enum Column { NameColumn = 0, StatusColumn = 1, DetailsColumn = 2, ColumnCount = 3 };

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QString utf8(const std::string &value)
{
	return QString::fromUtf8(value.c_str());
}

// The dock: it shows the view it is handed and passes every click to the controller. It works nothing out itself
// (READ-74): every text, tick and button state comes from the view, and the fixed labels from the locale file.
class CameraSyncDock : public QWidget, public camera_sync::ViewSink {
public:
	explicit CameraSyncDock(camera_sync::Controller &controller);

	void render(const camera_sync::DockView &view) override;
	bool confirm(const std::string &question, const std::string &detail) override;

private:
	void render_cameras(const std::vector<camera_sync::CameraRowView> &rows, bool details);
	void render_mics(const std::vector<camera_sync::MicView> &mics);

	camera_sync::Controller &controller_;
	QTreeWidget *cameras_;
	QComboBox *mics_;
	QPushButton *start_;
	QPushButton *reset_;
	QPushButton *apply_;
	QPushButton *undo_;
	QCheckBox *details_;
	QLabel *message_;
};

CameraSyncDock::CameraSyncDock(camera_sync::Controller &controller)
	: controller_(controller),
	  cameras_(new QTreeWidget(this)),
	  mics_(new QComboBox(this)),
	  start_(new QPushButton(this)),
	  reset_(new QPushButton(text(camera_sync::strings::button_reset), this)),
	  apply_(new QPushButton(text(camera_sync::strings::button_apply), this)),
	  undo_(new QPushButton(text(camera_sync::strings::button_undo), this)),
	  details_(new QCheckBox(text(camera_sync::strings::label_details), this)),
	  message_(new QLabel(this))
{
	cameras_->setColumnCount(ColumnCount);
	cameras_->setHeaderHidden(true);
	cameras_->setRootIsDecorated(false);
	cameras_->setSelectionMode(QAbstractItemView::NoSelection);
	cameras_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	cameras_->setColumnHidden(DetailsColumn, true);

	auto *mic_row = new QHBoxLayout();
	mic_row->addWidget(new QLabel(text(camera_sync::strings::label_mic), this));
	mic_row->addWidget(mics_, 1);

	auto *pattern = new QLabel(this);
	pattern->setTextFormat(Qt::RichText);
	pattern->setText(QString("<a href=\"%1\">%2</a>")
				 .arg(QString::fromUtf8(PATTERN_URL),
				      text(camera_sync::strings::label_pattern).toHtmlEscaped()));
	// The link opens in the system browser (READ-73).
	pattern->setOpenExternalLinks(true);
	pattern->setTextInteractionFlags(Qt::TextBrowserInteraction);

	auto *buttons = new QHBoxLayout();
	buttons->addWidget(start_);
	buttons->addWidget(reset_);
	buttons->addWidget(apply_);
	buttons->addWidget(undo_);
	buttons->addStretch(1);
	buttons->addWidget(details_);

	message_->setWordWrap(true);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(cameras_, 1);
	layout->addLayout(mic_row);
	layout->addWidget(pattern);
	layout->addLayout(buttons);
	layout->addWidget(message_);

	// Clicks only reach the controller (READ-71, READ-72). Rendering blocks these signals, so a repaint sends none.
	QObject::connect(cameras_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
		if (column != NameColumn)
			return;
		const std::string id = item->data(NameColumn, Qt::UserRole).toString().toStdString();
		controller_.set_camera_ticked(id, item->checkState(NameColumn) == Qt::Checked);
	});
	QObject::connect(mics_, &QComboBox::activated, this, [this](int index) {
		const std::string id = mics_->itemData(index).toString().toStdString();
		if (!id.empty())
			controller_.choose_mic(id);
	});
	QObject::connect(start_, &QPushButton::clicked, this, [this] { controller_.start_or_stop(); });
	QObject::connect(reset_, &QPushButton::clicked, this, [this] { controller_.reset(); });
	QObject::connect(apply_, &QPushButton::clicked, this, [this] { controller_.apply(); });
	QObject::connect(undo_, &QPushButton::clicked, this, [this] { controller_.undo(); });
	QObject::connect(details_, &QCheckBox::toggled, this, [this](bool on) { controller_.set_details(on); });
}

void CameraSyncDock::render(const camera_sync::DockView &view)
{
	const QSignalBlocker block_cameras(cameras_);
	const QSignalBlocker block_mics(mics_);
	const QSignalBlocker block_details(details_);

	render_cameras(view.cameras, view.show_details);
	render_mics(view.mics);

	start_->setText(utf8(view.start_label));
	start_->setEnabled(view.start_enabled);
	reset_->setEnabled(view.reset_enabled);
	apply_->setEnabled(view.apply_enabled);
	undo_->setEnabled(view.undo_enabled);
	details_->setChecked(view.show_details);
	message_->setText(utf8(view.message));
	message_->setVisible(!view.message.empty());
}

// The on-air question, modal, with "Apply" and "Cancel"; Cancel is the default, so Enter writes nothing (ALIGN-64).
bool CameraSyncDock::confirm(const std::string &question, const std::string &detail)
{
	QMessageBox box(this);
	box.setWindowTitle(text("CameraSync.Dock.Title"));
	box.setIcon(QMessageBox::Question);
	box.setText(utf8(question));
	if (!detail.empty())
		box.setInformativeText(utf8(detail));
	QPushButton *apply = box.addButton(text(camera_sync::strings::button_apply), QMessageBox::AcceptRole);
	QPushButton *cancel = box.addButton(text(camera_sync::strings::button_cancel), QMessageBox::RejectRole);
	box.setDefaultButton(cancel);
	box.setEscapeButton(cancel);
	box.exec();
	return box.clickedButton() == apply;
}

// Rows are updated in place, so a tick under the cursor is not rebuilt; the list is rebuilt only when its cameras or
// their order change.
void CameraSyncDock::render_cameras(const std::vector<camera_sync::CameraRowView> &rows, bool details)
{
	const int count = static_cast<int>(rows.size());
	bool same = cameras_->topLevelItemCount() == count;
	for (int i = 0; same && i < count; i++)
		same = cameras_->topLevelItem(i)->data(NameColumn, Qt::UserRole).toString() ==
		       utf8(rows[static_cast<size_t>(i)].id);
	if (!same) {
		cameras_->clear();
		for (const camera_sync::CameraRowView &row : rows) {
			auto *item = new QTreeWidgetItem(cameras_);
			item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
			item->setData(NameColumn, Qt::UserRole, utf8(row.id));
		}
	}

	for (int i = 0; i < count; i++) {
		const camera_sync::CameraRowView &row = rows[static_cast<size_t>(i)];
		QTreeWidgetItem *item = cameras_->topLevelItem(i);
		item->setText(NameColumn, utf8(row.name));
		item->setCheckState(NameColumn, row.ticked ? Qt::Checked : Qt::Unchecked);
		item->setText(StatusColumn, utf8(row.status));
		item->setText(DetailsColumn, utf8(row.details));
	}
	cameras_->setColumnHidden(DetailsColumn, !details);
}

// The picker is rebuilt only when its sources change, so an open list is not closed under the user.
void CameraSyncDock::render_mics(const std::vector<camera_sync::MicView> &mics)
{
	const int count = static_cast<int>(mics.size());
	bool same = mics_->count() == count;
	for (int i = 0; same && i < count; i++)
		same = mics_->itemData(i).toString() == utf8(mics[static_cast<size_t>(i)].id);
	if (!same) {
		mics_->clear();
		for (const camera_sync::MicView &mic : mics)
			mics_->addItem(utf8(mic.name), utf8(mic.id));
	}

	int chosen = -1;
	for (int i = 0; i < count; i++) {
		const camera_sync::MicView &mic = mics[static_cast<size_t>(i)];
		mics_->setItemText(i, utf8(mic.name));
		if (mic.chosen)
			chosen = i;
	}
	mics_->setCurrentIndex(chosen);
}

} // namespace

void camera_sync_register_dock(camera_sync::Controller &controller)
{
	// OBS puts the widget in a dock and owns it from then on. The controller keeps a guarded pointer to it.
	auto *dock = new CameraSyncDock(controller);

	if (!obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("CameraSync.Dock.Title"), dock)) {
		obs_log(LOG_WARNING, "could not add the dock %s", DOCK_ID);
		delete dock;
		return;
	}
	controller.set_view_sink(dock, dock);
}
