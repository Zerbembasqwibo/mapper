/*
 *    Copyright 2012 Thomas Schöps
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


#include "symbol_setting_dialog.h"

#include <QtGui>

#include "map.h"
#include "object.h"
#include "object_text.h"
#include "symbol.h"
#include "symbol_point.h"
#include "symbol_line.h"
#include "symbol_area.h"
#include "symbol_text.h"
#include "main_window.h"
#include "map_editor.h"
#include "map_widget.h"
#include "template.h"
#include "template_image.h"
#include "template_dock_widget.h"
#include "symbol_point_editor.h"
#include "symbol_combined.h"
#include "symbol_properties_widget.h"

SymbolSettingDialog::SymbolSettingDialog(Symbol* map_symbol, Map* map, QWidget* parent)
: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint), source_symbol(map_symbol)
{
	setWindowTitle(tr("Symbol settings"));
	setSizeGripEnabled(true);
	
	this->symbol = map_symbol->duplicate();
	this->source_map = map;
	
	symbol_icon_label = new QLabel();
	symbol_icon_label->setPixmap(QPixmap::fromImage(*symbol->getIcon(source_map)));
	
	symbol_text_label = new QLabel();
	QFont symbol_text_label_font = symbol_text_label->font();
	symbol_text_label_font.setBold(true);
	symbol_text_label->setFont(symbol_text_label_font);
	updateSymbolLabel();

	QPushButton* cancel_button = new QPushButton(tr("Cancel"));
	ok_button = new QPushButton(QIcon(":/images/arrow-right.png"), tr("OK"));
	ok_button->setDefault(true);
	
	preview_map = new Map();
	preview_map->useColorsFrom(map);
	preview_map->setScaleDenominator(map->getScaleDenominator());
	
	createPreviewMap();
	
	preview_widget = new MainWindow(false);
	preview_controller = new MapEditorController(MapEditorController::SymbolEditor, preview_map);
	preview_widget->setController(preview_controller);
	preview_map_view = preview_controller->getMainWidget()->getMapView();
	float zoom_factor = 1;
	if (symbol->getType() == Symbol::Point)
		zoom_factor = 8;
	else if (symbol->getType() == Symbol::Line || symbol->getType() == Symbol::Combined)
		zoom_factor = 2;
	preview_map_view->setZoom(zoom_factor * preview_map_view->getZoom());
	
	properties_widget = symbol->createPropertiesWidget(this);
	
	QVBoxLayout* preview_layout = NULL;
	if (symbol->getType() == Symbol::Point)
	{
		QLabel* template_label = new QLabel(tr("<b>Template</b>: "));
		template_file_label = new QLabel(tr("(none)"));
		QPushButton* load_template_button = new QPushButton(tr("Open..."));
		
		center_template_button = new QToolButton();
		center_template_button->setText(tr("Center template..."));
		center_template_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		center_template_button->setPopupMode(QToolButton::InstantPopup);
		center_template_button->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
		QMenu* center_template_menu = new QMenu(center_template_button);
		center_template_menu->addAction(tr("bounding box on origin"), this, SLOT(centerTemplateBBox()));
		center_template_menu->addAction(tr("center of gravity on origin"), this, SLOT(centerTemplateGravity()));
		center_template_button->setMenu(center_template_menu);
		
		center_template_button->setEnabled(false);
		
		QHBoxLayout* template_layout = new QHBoxLayout();
		template_layout->addWidget(template_label);
		template_layout->addWidget(template_file_label, 1);
		template_layout->addWidget(load_template_button);
		template_layout->addWidget(center_template_button);
		
		preview_layout = new QVBoxLayout();
		preview_layout->setContentsMargins(0, 0, 0, 0);
		preview_layout->addLayout(template_layout);
		preview_layout->addWidget(preview_widget);
		
		connect(load_template_button, SIGNAL(clicked(bool)), this, SLOT(loadTemplateClicked()));
	}
	else
	{
		template_file_label = NULL;
		center_template_button = NULL;
	}
	
	QHBoxLayout* buttons_layout = new QHBoxLayout();
	buttons_layout->setContentsMargins(0, 0, 0, 0);
	buttons_layout->addWidget(cancel_button);
	buttons_layout->addStretch(1);
	buttons_layout->addWidget(ok_button);
	
	QGridLayout* left_layout = new QGridLayout();
	left_layout->setColumnStretch(6,1);
	
	int row = 0, col = 0;
	left_layout->addWidget(symbol_icon_label, row, col++);
	left_layout->addWidget(symbol_text_label, row, col++, 1, 6);
	
	row++; col = 0;
	left_layout->addWidget(properties_widget, row, col, 1, 7);
	
	row++; col = 0;
	left_layout->addLayout(buttons_layout, row, col, 1, 7);
	
	QSplitter* splitter = new QSplitter();
	QWidget* left = new QWidget();
	left->setLayout(left_layout);
	left->layout();
	splitter->addWidget(left);
	splitter->setCollapsible(0, false);
	if (preview_layout != NULL)
	{
		QWidget* right = new QWidget();
		right->setLayout(preview_layout);
		splitter->addWidget(right);
	}
	else
		splitter->addWidget(preview_widget);
	splitter->setCollapsible(1, true);
	
	QBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(splitter);
	setLayout(layout);
	
	connect(cancel_button, SIGNAL(clicked(bool)), this, SLOT(reject()));
	connect(ok_button, SIGNAL(clicked(bool)), this, SLOT(accept()));
	
	updateOkButton();
}

SymbolSettingDialog::~SymbolSettingDialog()
{
	delete properties_widget; // must be deleted before the symbol!
	delete symbol;
}

void SymbolSettingDialog::updatePreview()
{
	symbol_icon_label->setPixmap(QPixmap::fromImage(*symbol->getIcon(source_map, true)));
	
	for (int l = 0; l < preview_map->getNumLayers(); ++l)
		for (int i = 0; i < preview_map->getLayer(l)->getNumObjects(); ++i)
			preview_map->getLayer(l)->getObject(i)->update(true);
}

void SymbolSettingDialog::loadTemplateClicked()
{
	Template* temp = TemplateWidget::showOpenTemplateDialog(this, preview_map_view);
	if (!temp)
		return;
	
	if (preview_map->getNumTemplates() > 0)
	{
		// Delete old template
		preview_map->setTemplateAreaDirty(0);
		preview_map->deleteTemplate(0);
	}
	
	preview_map->setFirstFrontTemplate(1);
	
	preview_map->addTemplate(temp, 0);
	TemplateVisibility* vis = preview_map_view->getTemplateVisibility(temp);
	vis->visible = true;
	vis->opacity = 1;
	preview_map->setTemplateAreaDirty(0);
	
	template_file_label->setText(temp->getTemplateFilename());
	center_template_button->setEnabled(temp->getTemplateType().compare("TemplateImage") == 0);
}

void SymbolSettingDialog::centerTemplateBBox()
{
	assert(preview_map->getNumTemplates() == 1);
	Template* temp = preview_map->getTemplate(0);
	
	QRectF extent = temp->getExtent();
	QPointF center = extent.center();
	MapCoordF map_center_current = temp->templateToMap(center);
	
	preview_map->setTemplateAreaDirty(0);
	temp->setTemplateX(temp->getTemplateX() - qRound64(1000 * map_center_current.getX()));
	temp->setTemplateY(temp->getTemplateY() - qRound64(1000 * map_center_current.getY()));
	preview_map->setTemplateAreaDirty(0);
}

void SymbolSettingDialog::centerTemplateGravity()
{
	assert(preview_map->getNumTemplates() == 1);
	Template* temp = preview_map->getTemplate(0);
	TemplateImage* image = reinterpret_cast<TemplateImage*>(temp);
	
	QColor background_color = QColorDialog::getColor(Qt::white, this, tr("Select background color"));
	if (!background_color.isValid())
		return;
	MapCoordF map_center_current = temp->templateToMap(image->calcCenterOfGravity(background_color.rgb()));
	
	preview_map->setTemplateAreaDirty(0);
	temp->setTemplateX(temp->getTemplateX() - qRound64(1000 * map_center_current.getX()));
	temp->setTemplateY(temp->getTemplateY() - qRound64(1000 * map_center_current.getY()));
	preview_map->setTemplateAreaDirty(0);
}

void SymbolSettingDialog::createPreviewMap()
{
	symbol_icon_label->setPixmap(QPixmap::fromImage(*symbol->getIcon(source_map)));
	
	for (int i = 0; i < (int)preview_objects.size(); ++i)
		preview_map->deleteObject(preview_objects[i], false);
	preview_objects.clear();
	
	if (symbol->getType() == Symbol::Line)
	{
		LineSymbol* line = reinterpret_cast<LineSymbol*>(symbol);
		
		const int num_lines = 15;
		const float min_length = 0.5f;
		const float max_length = 15;
		const float x_offset = -0.5f * max_length;
		const float y_offset = (0.001f * qMax(600, line->getLineWidth())) * 3.5f;
		
		float y_start = 0 - (y_offset * (num_lines - 1));
		
		for (int i = 0; i < num_lines; ++i)
		{
			float length = min_length + (i / (float)(num_lines - 1)) * (max_length - min_length);
			float y = y_start + i * y_offset;
			
			PathObject* path = new PathObject(line);
			path->addCoordinate(0, MapCoordF(x_offset - length, y).toMapCoord());
			path->addCoordinate(1, MapCoordF(x_offset, y).toMapCoord());
			preview_map->addObject(path);
			
			preview_objects.push_back(path);
		}
		
		const int num_circular_lines = 12;
		const float inner_radius = 4;
		const float center_x = 2*inner_radius + 0.5f * max_length;
		const float center_y = 0.5f * y_start;
		
		for (int i = 0; i < num_circular_lines; ++i)
		{
			float angle = (i / (float)num_circular_lines) * 2*M_PI;
			float length = min_length + (i / (float)(num_circular_lines - 1)) * (max_length - min_length);
			
			PathObject* path = new PathObject(line);
			path->addCoordinate(0, ((MapCoordF(sin(angle), -cos(angle)) * inner_radius) + MapCoordF(center_x, center_y)).toMapCoord());
			path->addCoordinate(1, ((MapCoordF(sin(angle), -cos(angle)) * (inner_radius + length)) + MapCoordF(center_x, center_y)).toMapCoord());
			preview_map->addObject(path);
			
			preview_objects.push_back(path);
		}
		
		const float snake_min_x = -1.5f * max_length;
		const float snake_max_x = 1.5f * max_length;
		const float snake_max_y = qMin(0.5f * y_start - inner_radius - max_length, y_start) - 4;
		const float snake_min_y = snake_max_y - 6;
		const int snake_steps = 8;
		
		PathObject* path = new PathObject(line);
		for (int i = 0; i < snake_steps; ++i)
		{
			MapCoord coord(snake_min_x + (i / (float)(snake_steps-1)) * (snake_max_x - snake_min_x), snake_min_y + (i % 2) * (snake_max_y - snake_min_y));
			coord.setDashPoint(true);
			path->addCoordinate(i, coord);
		}
		preview_map->addObject(path);
		preview_objects.push_back(path);
		
		const float curve_min_x = snake_min_x;
		const float curve_max_x = snake_max_x;
		const float curve_max_y = snake_min_y - 4;
		const float curve_min_y = curve_max_y - 6;
		
		path = new PathObject(line);
		MapCoord coord = MapCoord(curve_min_x, curve_min_y);
		coord.setCurveStart(true);
		path->addCoordinate(coord);
		coord = MapCoord(curve_min_x + (curve_max_x - curve_min_x) / 6, curve_max_y);
		path->addCoordinate(coord);
		coord = MapCoord(curve_min_x + 2 * (curve_max_x - curve_min_x) / 6, curve_max_y);
		path->addCoordinate(coord);
		coord = MapCoord(curve_min_x + 3 * (curve_max_x - curve_min_x) / 6, 0.5f * (curve_min_y + curve_max_y));
		coord.setCurveStart(true);
		path->addCoordinate(coord);
		coord = MapCoord(curve_min_x + 4 * (curve_max_x - curve_min_x) / 6, curve_min_y);
		path->addCoordinate(coord);
		coord = MapCoord(curve_min_x + 5 * (curve_max_x - curve_min_x) / 6, curve_min_y);
		path->addCoordinate(coord);
		coord = MapCoord(curve_max_x, curve_max_y);
		path->addCoordinate(coord);
		preview_map->addObject(path);
		preview_objects.push_back(path);
		
		// Debug objects
		
		/*
		// Closed path, rectangular
		PathObject* path;
		MapCoord coord;
		
		path = new PathObject(preview_map, line);
		coord = MapCoord(-20, -10);
		//coord.setCurveStart(true);
		path->addCoordinate(coord);
		coord = MapCoord(20, -10);
		path->addCoordinate(coord);
		coord = MapCoord(20, 10);
		path->addCoordinate(coord);
		coord = MapCoord(-20, 10);
		path->addCoordinate(coord);
		path->setPathClosed(true);
		preview_map->addObject(path);
		preview_objects.push_back(path);*/
		
		// Closed path, curve
		/*PathObject* path;
		MapCoord coord;
		
		path = new PathObject(preview_map, line);
		path->setPathClosed(true);
		coord = MapCoord(-20, 10);
		coord.setCurveStart(true);
		path->addCoordinate(coord);
		coord = MapCoord(-10, 0);
		path->addCoordinate(coord);
		coord = MapCoord(-10, 10);
		path->addCoordinate(coord);
		
		preview_map->addObject(path);
		preview_objects.push_back(path);*/
		
		// Path with holes
		/*PathObject* path;
		MapCoord coord;
		
		path = new PathObject(preview_map, line);
		coord = MapCoord(-20, 10);
		coord.setCurveStart(true);
		path->addCoordinate(coord);
		coord = MapCoord(-10, 0);
		path->addCoordinate(coord);
		coord = MapCoord(0, -10);
		path->addCoordinate(coord);
		coord = MapCoord(10, 10);
		path->addCoordinate(coord);
		coord = MapCoord(20, 10);
		coord.setHolePoint(true);
		path->addCoordinate(coord);
		coord = MapCoord(30, 10);
		path->addCoordinate(coord);
		coord = MapCoord(40, 10);
		path->addCoordinate(coord);

		preview_map->addObject(path);
		preview_objects.push_back(path);*/
	}
	else if (symbol->getType() == Symbol::Area)
	{
		const float half_radius = 8;
		const float offset_y = 0;
		
		PathObject* path = new PathObject(symbol);
		path->addCoordinate(0, MapCoordF(-half_radius, -half_radius + offset_y).toMapCoord());
		path->addCoordinate(1, MapCoordF(half_radius, -half_radius + offset_y).toMapCoord());
		path->addCoordinate(2, MapCoordF(half_radius, half_radius + offset_y).toMapCoord());
		path->addCoordinate(3, MapCoordF(-half_radius, half_radius + offset_y).toMapCoord());
		preview_map->addObject(path);
		
		preview_objects.push_back(path);
	}
	else if (symbol->getType() == Symbol::Text)
	{
		TextSymbol* text_symbol = reinterpret_cast<TextSymbol*>(symbol);
		
		// TODO: Suggestion for the German translation: "Franz, der OL für die Abkürzung\nvon Oldenbourg hält, jagt im komplett\nverwahrlosten Taxi quer durch Bayern\n1234567890"
		const QString string = tr("The quick brown fox\ntakes the routechoice\nto jump over the lazy dog\n1234567890");
		
		TextObject* object = new TextObject(text_symbol);
		object->setAnchorPosition(0, 0);
		object->setText(string);
		object->setHorizontalAlignment(TextObject::AlignHCenter);
		object->setVerticalAlignment(TextObject::AlignVCenter);
		preview_map->addObject(object);
		
		preview_objects.push_back(object);
	}
	else if (symbol->getType() == Symbol::Combined)
	{
		const float radius = 5;
		
		PathObject* path = new PathObject(symbol);
		for (int i = 0; i < 5; ++i)
			path->addCoordinate(i, MapCoord(sin(2*M_PI * i/5.0) * radius, -cos(2*M_PI * i/5.0) * radius));
		path->getPart(0).setClosed(true, false);
		preview_map->addObject(path);
		
		preview_objects.push_back(path);
	}
}

void SymbolSettingDialog::updateOkButton()
{
	ok_button->setEnabled(symbol->getNumberComponent(0)>=0 && !symbol->getName().isEmpty());
}

void SymbolSettingDialog::generalModified()
{
	updateSymbolLabel();
	updateOkButton();
}

void SymbolSettingDialog::updateSymbolLabel()
{
	QString number = symbol->getNumberAsString();
	if (number.isEmpty())
		number = "???";
	QString name = symbol->getName();
	if (name.isEmpty())
		name = "- unnamed -";
	symbol_text_label->setText(number % "  " % name);
}


#include "symbol_setting_dialog.moc"