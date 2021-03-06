/*
 *    Copyright 2012, 2013 Jan Dalheimer
 *    Copyright 2012-2015 Kai Pastor
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "settings_dialog.h"
#include "settings_dialog_p.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QTabWidget>
#include <QTextCodec>
#include <QToolButton>
#include <QVBoxLayout>

#include "../settings.h"
#include "../util.h"
#include "../util_gui.h"
#include "../util_translation.h"
#include "../util/scoped_signals_blocker.h"
#include "modifier_key.h"
#include "widgets/home_screen_widget.h"


// ### SettingsDialog ###

SettingsDialog::SettingsDialog(QWidget* parent)
 : QDialog(parent)
{
	setWindowTitle(tr("Settings"));
	
	QVBoxLayout* layout = new QVBoxLayout();
	this->setLayout(layout);
	
	tab_widget = new QTabWidget();
	layout->addWidget(tab_widget);
	
	button_box = new QDialogButtonBox(
	  QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::Help,
	  Qt::Horizontal );
	layout->addWidget(button_box);
	connect(button_box, &QDialogButtonBox::clicked, this, &SettingsDialog::buttonPressed);
	
	// Add all pages
	addPage(new GeneralPage(this));
	addPage(new EditorPage(this));
}

SettingsDialog::~SettingsDialog()
{
	// Nothing, not inlined.
}

void SettingsDialog::addPage(SettingsPage* page)
{
	tab_widget->addTab(page, page->title());
}

void SettingsDialog::buttonPressed(QAbstractButton* button)
{
	QDialogButtonBox::StandardButton id = button_box->standardButton(button);
	const int count = tab_widget->count();
	int i;
	switch (id)
	{
	case QDialogButtonBox::Ok:
		for (i = 0; i < count; i++)
			static_cast< SettingsPage* >(tab_widget->widget(i))->ok();
		Settings::getInstance().applySettings();
		this->accept();
		break;
		
	case QDialogButtonBox::Apply:
		for (i = 0; i < count; i++)
			static_cast< SettingsPage* >(tab_widget->widget(i))->apply();
		Settings::getInstance().applySettings();
		break;
		
	case QDialogButtonBox::Cancel:
		for (i = 0; i < count; i++)
			static_cast< SettingsPage* >(tab_widget->widget(i))->cancel();
		this->reject();
		break;
		
	case QDialogButtonBox::Help:
		Util::showHelp(this, QStringLiteral("settings.html"));
		break;
		
	default:
		Q_UNREACHABLE();
		break;
	}
}



// ### SettingsPage ###

SettingsPage::SettingsPage(QWidget* parent)
 : QWidget(parent)
{
	// nothing
}

void SettingsPage::cancel()
{
	changes.clear();
}

void SettingsPage::apply()
{
	QSettings settings;
	for (int i = 0; i < changes.size(); i++)
		settings.setValue(changes.keys().at(i), changes.values().at(i));
	changes.clear();
}

void SettingsPage::ok()
{
	this->apply();
}



// ### EditorPage ###

EditorPage::EditorPage(QWidget* parent)
 : SettingsPage(parent)
{
	QGridLayout* layout = new QGridLayout();
	this->setLayout(layout);
	
	int row = 0;
	
	antialiasing = new QCheckBox(tr("High quality map display (antialiasing)"), this);
	antialiasing->setToolTip(tr("Antialiasing makes the map look much better, but also slows down the map display"));
	layout->addWidget(antialiasing, row++, 0, 1, 2);
	
	text_antialiasing = new QCheckBox(tr("High quality text display in map (antialiasing), slow"), this);
	text_antialiasing->setToolTip(tr("Antialiasing makes the map look much better, but also slows down the map display"));
	layout->addWidget(text_antialiasing, row++, 0, 1, 2);
	
	QLabel* tolerance_label = new QLabel(tr("Click tolerance:"));
	QSpinBox* tolerance = Util::SpinBox::create(0, 50, tr("mm", "millimeters"));
	layout->addWidget(tolerance_label, row, 0);
	layout->addWidget(tolerance, row++, 1);
	
	QLabel* snap_distance_label = new QLabel(tr("Snap distance (%1):").arg(ModifierKey::shift()));
	QSpinBox* snap_distance = Util::SpinBox::create(0, 100, tr("mm", "millimeters"));
	layout->addWidget(snap_distance_label, row, 0);
	layout->addWidget(snap_distance, row++, 1);
	
	QLabel* fixed_angle_stepping_label = new QLabel(tr("Stepping of fixed angle mode (%1):").arg(ModifierKey::control()));
	QSpinBox* fixed_angle_stepping = Util::SpinBox::create(1, 180, trUtf8("°", "Degree sign for angles"));
	layout->addWidget(fixed_angle_stepping_label, row, 0);
	layout->addWidget(fixed_angle_stepping, row++, 1);

	QCheckBox* select_symbol_of_objects = new QCheckBox(tr("When selecting an object, automatically select its symbol, too"));
	layout->addWidget(select_symbol_of_objects, row++, 0, 1, 2);
	
	QCheckBox* zoom_out_away_from_cursor = new QCheckBox(tr("Zoom away from cursor when zooming out"));
	layout->addWidget(zoom_out_away_from_cursor, row++, 0, 1, 2);
	
	QCheckBox* draw_last_point_on_right_click = new QCheckBox(tr("Drawing tools: set last point on finishing with right click"));
	layout->addWidget(draw_last_point_on_right_click, row++, 0, 1, 2);
	
	QCheckBox* keep_settings_of_closed_templates = new QCheckBox(tr("Templates: keep settings of closed templates"));
	layout->addWidget(keep_settings_of_closed_templates, row++, 0, 1, 2);
	
	
	layout->setRowMinimumHeight(row++, 16);
	layout->addWidget(Util::Headline::create(tr("Edit tool:")), row++, 0, 1, 2);
	
	edit_tool_delete_bezier_point_action = new QComboBox();
	edit_tool_delete_bezier_point_action->addItem(tr("Retain old shape"), (int)Settings::DeleteBezierPoint_RetainExistingShape);
	edit_tool_delete_bezier_point_action->addItem(tr("Reset outer curve handles"), (int)Settings::DeleteBezierPoint_ResetHandles);
	edit_tool_delete_bezier_point_action->addItem(tr("Keep outer curve handles"), (int)Settings::DeleteBezierPoint_KeepHandles);
	layout->addWidget(new QLabel(tr("Action on deleting a curve point with %1:").arg(ModifierKey::control())), row, 0);
	layout->addWidget(edit_tool_delete_bezier_point_action, row++, 1);
	
	edit_tool_delete_bezier_point_action_alternative = new QComboBox();
	edit_tool_delete_bezier_point_action_alternative->addItem(tr("Retain old shape"), (int)Settings::DeleteBezierPoint_RetainExistingShape);
	edit_tool_delete_bezier_point_action_alternative->addItem(tr("Reset outer curve handles"), (int)Settings::DeleteBezierPoint_ResetHandles);
	edit_tool_delete_bezier_point_action_alternative->addItem(tr("Keep outer curve handles"), (int)Settings::DeleteBezierPoint_KeepHandles);
	layout->addWidget(new QLabel(tr("Action on deleting a curve point with %1:").arg(ModifierKey::controlShift())), row, 0);
	layout->addWidget(edit_tool_delete_bezier_point_action_alternative, row++, 1);
	
	layout->setRowMinimumHeight(row++, 16);
	layout->addWidget(Util::Headline::create(tr("Rectangle tool:")), row++, 0, 1, 2);
	
	QLabel* rectangle_helper_cross_radius_label = new QLabel(tr("Radius of helper cross:"));
	QSpinBox* rectangle_helper_cross_radius = Util::SpinBox::create(0, 999999, tr("mm", "millimeters"));
	layout->addWidget(rectangle_helper_cross_radius_label, row, 0);
	layout->addWidget(rectangle_helper_cross_radius, row++, 1);
	
	QCheckBox* rectangle_preview_line_width = new QCheckBox(tr("Preview the width of lines with helper cross"));
	layout->addWidget(rectangle_preview_line_width, row++, 0, 1, 2);
	
	
	antialiasing->setChecked(Settings::getInstance().getSetting(Settings::MapDisplay_Antialiasing).toBool());
	text_antialiasing->setChecked(Settings::getInstance().getSetting(Settings::MapDisplay_TextAntialiasing).toBool());
	tolerance->setValue(Settings::getInstance().getSetting(Settings::MapEditor_ClickToleranceMM).toInt());
	snap_distance->setValue(Settings::getInstance().getSetting(Settings::MapEditor_SnapDistanceMM).toInt());
	fixed_angle_stepping->setValue(Settings::getInstance().getSetting(Settings::MapEditor_FixedAngleStepping).toInt());
	select_symbol_of_objects->setChecked(Settings::getInstance().getSetting(Settings::MapEditor_ChangeSymbolWhenSelecting).toBool());
	zoom_out_away_from_cursor->setChecked(Settings::getInstance().getSetting(Settings::MapEditor_ZoomOutAwayFromCursor).toBool());
	draw_last_point_on_right_click->setChecked(Settings::getInstance().getSetting(Settings::MapEditor_DrawLastPointOnRightClick).toBool());
	keep_settings_of_closed_templates->setChecked(Settings::getInstance().getSetting(Settings::Templates_KeepSettingsOfClosed).toBool());
	
	edit_tool_delete_bezier_point_action->setCurrentIndex(edit_tool_delete_bezier_point_action->findData(Settings::getInstance().getSetting(Settings::EditTool_DeleteBezierPointAction).toInt()));
	edit_tool_delete_bezier_point_action_alternative->setCurrentIndex(edit_tool_delete_bezier_point_action_alternative->findData(Settings::getInstance().getSetting(Settings::EditTool_DeleteBezierPointActionAlternative).toInt()));
	
	rectangle_helper_cross_radius->setValue(Settings::getInstance().getSetting(Settings::RectangleTool_HelperCrossRadiusMM).toInt());
	rectangle_preview_line_width->setChecked(Settings::getInstance().getSetting(Settings::RectangleTool_PreviewLineWidth).toBool());
	
	layout->setRowStretch(row, 1);
	
	updateWidgets();

	connect(antialiasing, &QAbstractButton::toggled, this, &EditorPage::antialiasingClicked);
	connect(text_antialiasing, &QAbstractButton::toggled, this, &EditorPage::textAntialiasingClicked);
	connect(tolerance, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &EditorPage::toleranceChanged);
	connect(snap_distance, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &EditorPage::snapDistanceChanged);
	connect(fixed_angle_stepping, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &EditorPage::fixedAngleSteppingChanged);
	connect(select_symbol_of_objects, &QAbstractButton::clicked, this, &EditorPage::selectSymbolOfObjectsClicked);
	connect(zoom_out_away_from_cursor, &QAbstractButton::clicked, this, &EditorPage::zoomOutAwayFromCursorClicked);
	connect(draw_last_point_on_right_click, &QAbstractButton::clicked, this, &EditorPage::drawLastPointOnRightClickClicked);
	connect(keep_settings_of_closed_templates, &QAbstractButton::clicked, this, &EditorPage::keepSettingsOfClosedTemplatesClicked);
	
	connect(edit_tool_delete_bezier_point_action, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &EditorPage::editToolDeleteBezierPointActionChanged);
	connect(edit_tool_delete_bezier_point_action_alternative, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &EditorPage::editToolDeleteBezierPointActionAlternativeChanged);
	
	connect(rectangle_helper_cross_radius,  static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &EditorPage::rectangleHelperCrossRadiusChanged);
	connect(rectangle_preview_line_width, &QAbstractButton::clicked, this, &EditorPage::rectanglePreviewLineWidthChanged);
}

QString EditorPage::title() const
{
	return tr("Editor");
}

void EditorPage::updateWidgets()
{
	text_antialiasing->setEnabled(antialiasing->isChecked());
}

void EditorPage::antialiasingClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapDisplay_Antialiasing), QVariant(checked));
	updateWidgets();
}

void EditorPage::textAntialiasingClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapDisplay_TextAntialiasing), QVariant(checked));
}

void EditorPage::toleranceChanged(int value)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_ClickToleranceMM), QVariant(value));
}

void EditorPage::snapDistanceChanged(int value)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_SnapDistanceMM), QVariant(value));
}

void EditorPage::fixedAngleSteppingChanged(int value)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_FixedAngleStepping), QVariant(value));
}

void EditorPage::selectSymbolOfObjectsClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_ChangeSymbolWhenSelecting), QVariant(checked));
}

void EditorPage::zoomOutAwayFromCursorClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_ZoomOutAwayFromCursor), QVariant(checked));
}

void EditorPage::drawLastPointOnRightClickClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::MapEditor_DrawLastPointOnRightClick), QVariant(checked));
}

void EditorPage::keepSettingsOfClosedTemplatesClicked(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::Templates_KeepSettingsOfClosed), QVariant(checked));
}

void EditorPage::editToolDeleteBezierPointActionChanged(int index)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::EditTool_DeleteBezierPointAction), edit_tool_delete_bezier_point_action->itemData(index));
}

void EditorPage::editToolDeleteBezierPointActionAlternativeChanged(int index)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::EditTool_DeleteBezierPointActionAlternative), edit_tool_delete_bezier_point_action_alternative->itemData(index));
}

void EditorPage::rectangleHelperCrossRadiusChanged(int value)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::RectangleTool_HelperCrossRadiusMM), QVariant(value));
}

void EditorPage::rectanglePreviewLineWidthChanged(bool checked)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::RectangleTool_PreviewLineWidth), QVariant(checked));
}



// ### GeneralPage ###

const int GeneralPage::TranslationFromFile = -1;

GeneralPage::GeneralPage(QWidget* parent)
 : SettingsPage(parent)
{
	QGridLayout* layout = new QGridLayout();
	setLayout(layout);
	
	int row = 0;
	layout->addWidget(Util::Headline::create(tr("Appearance")), row, 1, 1, 2);
	
	row++;
	QLabel* language_label = new QLabel(tr("Language:"));
	layout->addWidget(language_label, row, 1);
	
	language_box = new QComboBox(this);
	updateLanguageBox();
	layout->addWidget(language_box, row, 2);
	
	QAbstractButton* language_file_button = new QToolButton();
	language_file_button->setIcon(QIcon(QStringLiteral(":/images/open.png")));
	layout->addWidget(language_file_button, row, 3);
	
	row++;
	layout->addItem(Util::SpacerItem::create(this), row, 1);
	
	row++;
	layout->addWidget(Util::Headline::create(tr("Screen")), row, 1, 1, 2);
	
	row++;
	QLabel* ppi_label = new QLabel(tr("Pixels per inch:"));
	layout->addWidget(ppi_label, row, 1);
	
	ppi_edit = Util::SpinBox::create(2, 0.01, 9999);
	ppi_edit->setValue(Settings::getInstance().getSetting(Settings::General_PixelsPerInch).toFloat());
	layout->addWidget(ppi_edit, row, 2);
	
	QAbstractButton* ppi_calculate_button = new QToolButton();
	ppi_calculate_button->setIcon(QIcon(QStringLiteral(":/images/settings.png")));
	layout->addWidget(ppi_calculate_button, row, 3);
	
	row++;
	layout->addItem(Util::SpacerItem::create(this), row, 1);
	
	row++;
	layout->addWidget(Util::Headline::create(tr("Program start")), row, 1, 1, 2);
	
	row++;
	QCheckBox* open_mru_check = new QCheckBox(AbstractHomeScreenWidget::tr("Open most recently used file"));
	open_mru_check->setChecked(Settings::getInstance().getSetting(Settings::General_OpenMRUFile).toBool());
	layout->addWidget(open_mru_check, row, 1, 1, 2);
	
	row++;
	QCheckBox* tips_visible_check = new QCheckBox(AbstractHomeScreenWidget::tr("Show tip of the day"));
	tips_visible_check->setChecked(Settings::getInstance().getSetting(Settings::HomeScreen_TipsVisible).toBool());
	layout->addWidget(tips_visible_check, row, 1, 1, 2);
	
	row++;
	layout->addItem(Util::SpacerItem::create(this), row, 1);
	
	row++;
	layout->addWidget(Util::Headline::create(tr("Saving files")), row, 1, 1, 2);
	
	row++;
	QCheckBox* compatibility_check = new QCheckBox(tr("Retain compatibility with Mapper %1").arg(QStringLiteral("0.5")));
	compatibility_check->setChecked(Settings::getInstance().getSetting(Settings::General_RetainCompatiblity).toBool());
	layout->addWidget(compatibility_check, row, 1, 1, 2);
	
	// Possible point: limit size of undo/redo journal
	
	int autosave_interval = Settings::getInstance().getSetting(Settings::General_AutosaveInterval).toInt();
	
	row++;
	QCheckBox* autosave_check = new QCheckBox(tr("Save information for automatic recovery"));
	autosave_check->setChecked(autosave_interval > 0);
	layout->addWidget(autosave_check, row, 1, 1, 2);
	
	row++;
	autosave_interval_label = new QLabel(tr("Recovery information saving interval:"));
	layout->addWidget(autosave_interval_label, row, 1);
	
	autosave_interval_edit = Util::SpinBox::create(1, 120, tr("min", "unit minutes"), 1);
	autosave_interval_edit->setValue(qAbs(autosave_interval));
	autosave_interval_edit->setEnabled(autosave_interval > 0);
	layout->addWidget(autosave_interval_edit, row, 2);
	
	row++;
	layout->addItem(Util::SpacerItem::create(this), row, 1);
	
	row++;
	layout->addWidget(Util::Headline::create(tr("File import and export")), row, 1, 1, 2);
	
	row++;
	QLabel* encoding_label = new QLabel(tr("8-bit encoding:"));
	layout->addWidget(encoding_label, row, 1);
	
	encoding_box = new QComboBox();
	encoding_box->addItem(QStringLiteral("System"));
	encoding_box->addItem(QStringLiteral("Windows-1250"));
	encoding_box->addItem(QStringLiteral("Windows-1252"));
	encoding_box->addItem(QStringLiteral("ISO-8859-1"));
	encoding_box->addItem(QStringLiteral("ISO-8859-15"));
	encoding_box->setEditable(true);
	QStringList availableCodecs;
	for (const QByteArray& item : QTextCodec::availableCodecs())
	{
		availableCodecs.append(QString::fromUtf8(item));
	}
	if (!availableCodecs.empty())
	{
		availableCodecs.sort(Qt::CaseInsensitive);
		availableCodecs.removeDuplicates();
		encoding_box->addItem(tr("More..."));
	}
	QCompleter* completer = new QCompleter(availableCodecs, this);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	encoding_box->setCompleter(completer);
	encoding_box->setCurrentText(Settings::getInstance().getSetting(Settings::General_Local8BitEncoding).toString());
	layout->addWidget(encoding_box, row, 2);
	
	row++;
	QCheckBox* ocd_importer_check = new QCheckBox(tr("Use the new OCD importer also for version 8 files"));
	ocd_importer_check->setChecked(Settings::getInstance().getSetting(Settings::General_NewOcd8Implementation).toBool());
	layout->addWidget(ocd_importer_check, row, 1, 1, 2);
	
	row++;
	layout->setRowStretch(row, 1);
	
#if defined(Q_OS_MAC)
	// Center setting items
	layout->setColumnStretch(0, 2);
#endif
	layout->setColumnStretch(2, 1);
	layout->setColumnStretch(2, 2);
	layout->setColumnStretch(layout->columnCount(), 2);
	
	connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &GeneralPage::languageChanged);
	connect(language_file_button, &QAbstractButton::clicked, this, &GeneralPage::openTranslationFileDialog);
	connect(ppi_edit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &GeneralPage::ppiChanged);
	connect(ppi_calculate_button, &QAbstractButton::clicked, this, &GeneralPage::openPPICalculationDialog);
	connect(open_mru_check, &QAbstractButton::clicked, this, &GeneralPage::openMRUFileClicked);
	connect(tips_visible_check, &QAbstractButton::clicked, this, &GeneralPage::tipsVisibleClicked);
	connect(encoding_box, &QComboBox::currentTextChanged, this, &GeneralPage::encodingChanged);
	connect(ocd_importer_check, &QAbstractButton::clicked, this, &GeneralPage::ocdImporterClicked);
	connect(autosave_check, &QAbstractButton::clicked, this, &GeneralPage::autosaveChanged);
	connect(autosave_interval_edit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GeneralPage::autosaveIntervalChanged);
	connect(compatibility_check, &QAbstractButton::clicked, this, &GeneralPage::retainCompatibilityChanged);
}

QString GeneralPage::title() const
{
	return tr("General");
}

void GeneralPage::apply()
{
	const Settings& settings = Settings::getInstance();
	const QString translation_file_key(settings.getSettingPath(Settings::General_TranslationFile));
	const QString language_key(settings.getSettingPath(Settings::General_Language));
	
	if (changes.contains(language_key) || changes.contains(translation_file_key))
	{
		if (!changes.contains(translation_file_key))
		{
			// Set an empty file name when changing the language without setting a filename.
			changes[translation_file_key] = QString();
		}
		
		QVariant lang = changes.contains(language_key) ? changes[language_key] : settings.getSetting(Settings::General_Language);
		QVariant translation_file = changes[translation_file_key];
		if ( settings.getSetting(Settings::General_Language) != lang ||
		     settings.getSetting(Settings::General_TranslationFile) != translation_file )
		{
			qApp->installEventFilter(this);
			// Show an message box in the new language.
			TranslationUtil translation((QLocale::Language)lang.toInt(), translation_file.toString());
			qApp->installTranslator(&translation.getQtTranslator());
			qApp->installTranslator(&translation.getAppTranslator());
			if (lang.toInt() <= 1 || lang.toInt() == 31) // cf. QLocale::Language
			{
				QMessageBox::information(window(), QStringLiteral("Notice"), QStringLiteral("The program must be restarted for the language change to take effect!"));
			}
			else
			{
				QMessageBox::information(window(), tr("Notice"), tr("The program must be restarted for the language change to take effect!"));
			}
			qApp->removeTranslator(&translation.getAppTranslator());
			qApp->removeTranslator(&translation.getQtTranslator());
			qApp->removeEventFilter(this);

#if defined(Q_OS_MAC)
			// The native [file] dialogs will use the first element of the
			// AppleLanguages array in the application's .plist file -
			// and this file is also the one used by QSettings.
			const QString mapper_language(translation.getLocale().name().left(2));
			changes["AppleLanguages"] = ( QStringList() << mapper_language );
#endif
		}
	}
	SettingsPage::apply();
}

bool GeneralPage::eventFilter(QObject* /* watched */, QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
		return true;
	
	return false;
}

void GeneralPage::languageChanged(int index)
{
	if (language_box->itemData(index) == TranslationFromFile)
	{
		openTranslationFileDialog();
	}
	else
	{
		changes.insert(Settings::getInstance().getSettingPath(Settings::General_Language), language_box->itemData(index).toInt());
	}
}

void GeneralPage::updateLanguageBox()
{
	const Settings& settings = Settings::getInstance();
	const QString translation_file_key(settings.getSettingPath(Settings::General_TranslationFile));
	const QString language_key(settings.getSettingPath(Settings::General_Language));
	
	LanguageCollection language_map = TranslationUtil::getAvailableLanguages();
	
	// Add the locale of the explicit translation file
	QString translation_file;
	if (changes.contains(translation_file_key))
		translation_file = changes[translation_file_key].toString();
	else 
		translation_file = settings.getSetting(Settings::General_TranslationFile).toString();
	
	QString locale_name = TranslationUtil::localeNameForFile(translation_file);
	if (!locale_name.isEmpty())
	{
		QLocale file_locale(locale_name);
		QString language_name = file_locale.nativeLanguageName();
		if (!language_map.contains(language_name))
			language_map.insert(language_name, file_locale.language());
	}
	
	// Update the language box
	const QSignalBlocker block(language_box);
	language_box->clear();
	
	LanguageCollection::const_iterator end = language_map.constEnd();
	for (LanguageCollection::const_iterator it = language_map.constBegin(); it != end; ++it)
		language_box->addItem(it.key(), (int)it.value());
	language_box->addItem(tr("Use translation file..."), TranslationFromFile);
	
	// Select current language
	int index = language_box->findData(changes.contains(language_key) ? 
	  changes[language_key].toInt() :
	  settings.getSetting(Settings::General_Language).toInt() );
	
	if (index < 0)
		index = language_box->findData((int)QLocale::English);
	language_box->setCurrentIndex(index);
}

void GeneralPage::openMRUFileClicked(bool state)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_OpenMRUFile), state);
}

void GeneralPage::tipsVisibleClicked(bool state)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::HomeScreen_TipsVisible), state);
}

// slot
void GeneralPage::encodingChanged(const QString& name)
{
	const QSignalBlocker block(encoding_box);
	
	if (name == tr("More..."))
	{
		encoding_box->setCurrentText(QString());
		encoding_box->completer()->setCompletionPrefix(QString());
		encoding_box->completer()->complete();
		return;
	}
	
	QTextCodec* codec = (name == QStringLiteral("System"))
	                    ? QTextCodec::codecForLocale()
	                    : QTextCodec::codecForName(name.toLatin1());
	if (codec)
	{
		changes.insert(Settings::getInstance().getSettingPath(Settings::General_Local8BitEncoding), name.toUtf8());
		encoding_box->setCurrentText(name);
	}
}

// slot
void GeneralPage::ocdImporterClicked(bool state)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_NewOcd8Implementation), state);
}

void GeneralPage::openTranslationFileDialog()
{
	Settings& settings = Settings::getInstance();
	const QString translation_file_key(settings.getSettingPath(Settings::General_TranslationFile));
	const QString language_key(settings.getSettingPath(Settings::General_Language));
	
	QString current_filename(settings.getSetting(Settings::General_Language).toString());
	if (changes.contains(translation_file_key))
		current_filename = changes[translation_file_key].toString();
	
	QString filename = QFileDialog::getOpenFileName(this,
	  tr("Open translation"), current_filename, tr("Translation files (*.qm)"));
	if (!filename.isNull())
	{
		QString locale_name(TranslationUtil::localeNameForFile(filename));
		if (locale_name.isEmpty())
		{
			QMessageBox::critical(this, tr("Open translation"),
			  tr("The selected file is not a valid translation.") );
		}
		else
		{
			QLocale locale(locale_name);
			changes.insert(translation_file_key, filename);
			changes.insert(language_key, locale.language());
		}
	}
	updateLanguageBox();
}

void GeneralPage::autosaveChanged(bool state)
{
	autosave_interval_label->setEnabled(state);
	autosave_interval_edit->setEnabled(state);
	
	int interval = autosave_interval_edit->value();
	if (!state)
		interval = -interval;
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_AutosaveInterval), interval);
}

void GeneralPage::autosaveIntervalChanged(int value)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_AutosaveInterval), value);
}

void GeneralPage::retainCompatibilityChanged(bool state)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_RetainCompatiblity), state);
}

void GeneralPage::ppiChanged(double ppi)
{
	changes.insert(Settings::getInstance().getSettingPath(Settings::General_PixelsPerInch), QVariant(ppi));
}

void GeneralPage::openPPICalculationDialog()
{
	int primary_screen_width = QApplication::primaryScreen()->size().width();
	int primary_screen_height = QApplication::primaryScreen()->size().height();
	float screen_diagonal_pixels = qSqrt(primary_screen_width*primary_screen_width + primary_screen_height*primary_screen_height);
	
	float old_ppi = ppi_edit->value();
	float old_screen_diagonal_inches = screen_diagonal_pixels / old_ppi;
	
	QDialog* dialog = new QDialog(window(), Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
	
	QGridLayout* layout = new QGridLayout();
	
	int row = 0;
	QLabel* resolution_label = new QLabel(tr("Primary screen resolution in pixels:"));
	layout->addWidget(resolution_label, row, 0);
	
	
	QLabel* resolution_display = new QLabel(QStringLiteral("%1 x %2").arg(primary_screen_width).arg(primary_screen_height));
	layout->addWidget(resolution_display, row, 1);
	
	row++;
	QLabel* size_label = new QLabel(tr("Primary screen size in inches (diagonal):"));
	layout->addWidget(size_label, row, 0);
	
	QDoubleSpinBox* size_edit = Util::SpinBox::create(2, 0.01, 9999);
	size_edit->setValue(old_screen_diagonal_inches);
	layout->addWidget(size_edit, row, 1);
	
	row++;
	layout->addItem(Util::SpacerItem::create(this), row, 1);
	
	row++;
	QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
	layout->addWidget(button_box, row, 0, 1, 2);
	
	dialog->setLayout(layout);
	connect(button_box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	dialog->exec();
	
	if (dialog->result() == QDialog::Accepted)
	{
		float screen_diagonal_inches = size_edit->value();
		float new_ppi = screen_diagonal_pixels / screen_diagonal_inches;
		ppi_edit->setValue(new_ppi);
		ppiChanged(new_ppi);
	}
}
