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


#include "map.h"

#include <assert.h>
#include <algorithm>

#include <QMessageBox>
#include <QFile>
#include <QPainter>
#include <QDebug>

#include "map_color.h"
#include "map_editor.h"
#include "map_widget.h"
#include "util.h"
#include "template.h"
#include "gps_coordinates.h"
#include "object.h"
#include "symbol.h"
#include "symbol_point.h"
#include "symbol_line.h"
#include "symbol_combined.h"
#include "file_format_ocad8.h"

MapLayer::MapLayer(const QString& name, Map* map) : name(name), map(map)
{
}
MapLayer::~MapLayer()
{
	int size = (int)objects.size();
	for (int i = 0; i < size; ++i)
		delete objects[i];
}
void MapLayer::save(QFile* file, Map* map)
{
	saveString(file, name);
	
	int size = (int)objects.size();
	file->write((const char*)&size, sizeof(int));
	
	for (int i = 0; i < size; ++i)
	{
		int save_type = static_cast<int>(objects[i]->getType());
		file->write((const char*)&save_type, sizeof(int));
		objects[i]->save(file);
	}
}
bool MapLayer::load(QFile* file, int version, Map* map)
{
	loadString(file, name);
	
	int size;
	file->read((char*)&size, sizeof(int));
	objects.resize(size);
	
	for (int i = 0; i < size; ++i)
	{
		int save_type;
		file->read((char*)&save_type, sizeof(int));
		objects[i] = Object::getObjectForType(static_cast<Object::Type>(save_type), NULL);
		if (!objects[i])
			return false;
		objects[i]->load(file, version, map);
	}
	return true;
}

int MapLayer::findObjectIndex(Object* object)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i] == object)
			return i;
	}
	assert(false);
	return -1;
}
void MapLayer::setObject(Object* object, int pos, bool delete_old)
{
	map->removeRenderablesOfObject(objects[pos], true);
	if (delete_old)
		delete objects[pos];
	
	objects[pos] = object;
	bool delete_old_renderables = object->getMap() == map;
	object->setMap(map);
	object->update(true, delete_old_renderables);
	map->setObjectsDirty();
}
void MapLayer::addObject(Object* object, int pos)
{
	objects.insert(objects.begin() + pos, object);
	bool delete_old_renderables = object->getMap() == map;
	object->setMap(map);
	object->update(true, delete_old_renderables);
	map->setObjectsDirty();
	
	if (map->getNumObjects() == 1)
		map->updateAllMapWidgets();
}
void MapLayer::deleteObject(int pos, bool remove_only)
{
	map->removeRenderablesOfObject(objects[pos], true);
	if (remove_only)
		objects[pos]->setMap(NULL);
	else
		delete objects[pos];
	objects.erase(objects.begin() + pos);
	map->setObjectsDirty();
	
	if (map->getNumObjects() == 0)
		map->updateAllMapWidgets();
}
bool MapLayer::deleteObject(Object* object, bool remove_only)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i] == object)
		{
			deleteObject(i, remove_only);
			return true;
		}
	}
	return false;
}

void MapLayer::findObjectsAt(MapCoordF coord, float tolerance, bool extended_selection, bool include_hidden_objects, bool include_protected_objects, SelectionInfoVector& out)
{
	int size = objects.size();
	for (int i = 0; i < size; ++i)
	{
		if (!include_hidden_objects && objects[i]->getSymbol()->isHidden())
			continue;
		if (!include_protected_objects && objects[i]->getSymbol()->isProtected())
			continue;
		
		objects[i]->update();
		int selected_type = objects[i]->isPointOnObject(coord, tolerance, extended_selection);
		if (selected_type != (int)Symbol::NoSymbol)
			out.push_back(std::pair<int, Object*>(selected_type, objects[i]));
	}
}
void MapLayer::findObjectsAtBox(MapCoordF corner1, MapCoordF corner2, bool include_hidden_objects, bool include_protected_objects, std::vector< Object* >& out)
{
	QRectF rect = QRectF(corner1.toQPointF(), corner2.toQPointF());
	
	int size = objects.size();
	for (int i = 0; i < size; ++i)
	{
		if (!include_hidden_objects && objects[i]->getSymbol()->isHidden())
			continue;
		if (!include_protected_objects && objects[i]->getSymbol()->isProtected())
			continue;
		
		objects[i]->update();
		if (rect.intersects(objects[i]->getExtent()) && objects[i]->intersectsBox(rect))
			out.push_back(objects[i]);
	}
}

QRectF MapLayer::calculateExtent(bool include_helper_symbols)
{
	QRectF rect;
	
	int i = 0;
	int size = objects.size();
	while (size > i && !rect.isValid())
	{
		if ((include_helper_symbols || !objects[i]->getSymbol()->isHelperSymbol()) && !objects[i]->getSymbol()->isHidden())
		{
			objects[i]->update();
			rect = objects[i]->getExtent();
		}
		++i;
	}
	
	for (; i < size; ++i)
	{
		if ((include_helper_symbols || !objects[i]->getSymbol()->isHelperSymbol()) && !objects[i]->getSymbol()->isHidden())
		{
			objects[i]->update();
			rectInclude(rect, objects[i]->getExtent());
		}
	}
	
	return rect;
}
void MapLayer::scaleAllObjects(double factor)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
		objects[i]->scale(factor);
	
	forceUpdateOfAllObjects();
}
void MapLayer::updateAllObjects(bool remove_old_renderables)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
		objects[i]->update(true, remove_old_renderables);
}
void MapLayer::updateAllObjectsWithSymbol(Symbol* symbol)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i]->getSymbol() != symbol)
			continue;
		
		objects[i]->update(true);
	}
}
void MapLayer::changeSymbolForAllObjects(Symbol* old_symbol, Symbol* new_symbol)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i]->getSymbol() != old_symbol)
			continue;
		
		if (!objects[i]->setSymbol(new_symbol, false))
			deleteObject(i, false);
		else
			objects[i]->update(true);
	}
}
bool MapLayer::deleteAllObjectsWithSymbol(Symbol* symbol)
{
	bool object_deleted = false;
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i]->getSymbol() != symbol)
			continue;
		
		deleteObject(i, false);
		object_deleted = true;
	}
	return object_deleted;
}
bool MapLayer::doObjectsExistWithSymbol(Symbol* symbol)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (objects[i]->getSymbol() == symbol)
			return true;
	}
	return false;
}
void MapLayer::forceUpdateOfAllObjects(Symbol* with_symbol)
{
	int size = objects.size();
	for (int i = size - 1; i >= 0; --i)
	{
		if (with_symbol == NULL || objects[i]->getSymbol() == with_symbol)
			objects[i]->update(true);
	}
}

// ### MapColorSet ###

Map::MapColorSet::MapColorSet(QObject *parent) : QObject(parent)
{
	ref_count = 1;
}
void Map::MapColorSet::addReference()
{
	++ref_count;
}
void Map::MapColorSet::dereference()
{
	--ref_count;
	if (ref_count == 0)
	{
		int size = colors.size();
		for (int i = 0; i < size; ++i)
			delete colors[i];
		this->deleteLater();
	}
}

// ### Map ###

bool Map::static_initialized = false;
MapColor Map::covering_white;
MapColor Map::covering_red;
MapColor Map::undefined_symbol_color;
LineSymbol* Map::covering_white_line;
LineSymbol* Map::covering_red_line;
LineSymbol* Map::undefined_line;
PointSymbol* Map::undefined_point;
CombinedSymbol* Map::covering_combined_line;

Map::Map() : renderables(this), selection_renderables(this)
{
	if (!static_initialized)
		initStatic();
	
	color_set = NULL;
	object_undo_manager.setOwner(this);
	gps_projection_parameters = NULL;
	
	clear();
}
Map::~Map()
{
	int size = symbols.size();
	for (int i = 0; i < size; ++i)
		delete symbols[i];
	
	size = templates.size();
	for (int i = 0; i < size; ++i)
		delete templates[i];
	
	size = layers.size();
	for (int i = 0; i < size; ++i)
		delete layers[i];
	
	/*size = views.size();
	for (int i = size; i >= 0; --i)
		delete views[i];*/
	
	color_set->dereference();
	
	delete gps_projection_parameters;
}

bool Map::saveTo(const QString& path, MapEditorController* map_editor)
{
	assert(map_editor && "Preserving the widget&view information without retrieving it from a MapEditorController is not implemented yet!");
	
    const Format *format = FileFormats.findFormatForFilename(path);
    if (!format) format = FileFormats.findFormat(FileFormats.defaultFormat());

    if (!format || !format->supportsExport())
    {
		if (format)
			QMessageBox::warning(NULL, tr("Error"), tr("Cannot export the map as\n\"%1\"\nbecause saving as %2 (.%3) is not supported.").arg(path).arg(format->description()).arg(format->fileExtension()));
        else
			QMessageBox::warning(NULL, tr("Error"), tr("Cannot export the map as\n\"%1\"\nbecause the format is unknown.").arg(path));
		return false;
    }

    Exporter *exporter = NULL;
    // Wrap everything in a try block, so we can gracefully recover if the importer balks.
    try {
        // Create an importer instance for this file and map.
        exporter = format->createExporter(path, this, map_editor->main_view);

        // Run the first pass.
        exporter->doExport();

        // Display any warnings.
        if (!exporter->warnings().empty())
        {
            QString warnings = "";
            for (std::vector<QString>::const_iterator it = exporter->warnings().begin(); it != exporter->warnings().end(); ++it) {
                if (!warnings.isEmpty())
					warnings += '\n';
				warnings += *it;
            }
			QMessageBox msgBox(QMessageBox::Warning, tr("Warning"), tr("The map export generated warnings."), QMessageBox::Ok);
			msgBox.setDetailedText(warnings);
			msgBox.exec();
        }
    }
    catch (std::exception &e)
    {
        qDebug() << "Exception:" << e.what();
    }
    if (exporter) delete exporter;


	colors_dirty = false;
	symbols_dirty = false;
	templates_dirty = false;
	objects_dirty = false;
	unsaved_changes = false;
	
	objectUndoManager().notifyOfSave();
	
	return true;
}
bool Map::loadFrom(const QString& path, MapEditorController* map_editor, bool load_symbols_only)
{
	MapView *view = new MapView(this);

	// Ensure the file exists and is readable.
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(NULL, tr("Error"), tr("Cannot open file:\n%1\nfor reading.").arg(path));
		return false;
	}
	
	// Delete previous objects
	clear();

	// Read a block at the beginning of the file, that we can use for magic number checking.
	unsigned char buffer[256];
	size_t total_read = file.read((char *)buffer, 256);
	file.close();

	bool import_complete = false;
	QString error_msg = tr("Invalid file type.");
	Q_FOREACH(const Format *format, FileFormats.formats())
	{
		// If the format supports import, and thinks it can understand the file header, then proceed.
		if (format->supportsImport() && format->understands(buffer, total_read))
		{
			Importer *importer = NULL;
			// Wrap everything in a try block, so we can gracefully recover if the importer balks.
			try {
				// Create an importer instance for this file and map.
				importer = format->createImporter(path, this, view);

				// Run the first pass.
				importer->doImport(load_symbols_only);

				// Are there any actions the user must take to complete the import?
				if (!importer->actions().empty())
				{
					// TODO: prompt the user to resolve the action items. All-in-one dialog.
				}

				// Finish the import.
				importer->finishImport();

				// Display any warnings.
				if (!importer->warnings().empty())
				{
					QString warnings = "";
					for (std::vector<QString>::const_iterator it = importer->warnings().begin(); it != importer->warnings().end(); ++it) {
						if (!warnings.isEmpty())
							warnings += '\n';
						warnings += *it;
					}
					QMessageBox msgBox(QMessageBox::Warning, tr("Warning"), tr("The map import generated warnings."), QMessageBox::Ok);
					msgBox.setDetailedText(warnings);
					msgBox.exec();
				}

				import_complete = true;
			}
			catch (std::exception &e)
			{
				qDebug() << "Exception:" << e.what();
				error_msg = e.what();
			}
			if (importer) delete importer;
		}
		// If the last importer finished successfully
		if (import_complete) break;
	}
	
	if (map_editor)
		map_editor->main_view = view;
	else
		delete view;	// TODO: HACK. Better not create the view at all in this case!

	if (!import_complete)
	{
		QMessageBox::warning(NULL, tr("Error"), tr("Cannot open file:\n%1\n\n%2").arg(path).arg(error_msg));
		return false;
	}

	// Post processing
	for (unsigned int i = 0; i < symbols.size(); ++i)
	{
		if (!symbols[i]->loadFinished(this))
		{
			QMessageBox::warning(NULL, tr("Error"), tr("Problem while opening file:\n%1\n\nError during symbol post-processing.").arg(path));
			return false;
		}
	}

	// Update all objects without trying to remove their renderables first, this gives a significant speedup when loading large files
	updateAllObjects(false);

	return true;
}

void Map::clear()
{
	if (color_set)
		color_set->dereference();
	color_set = new MapColorSet(this);
	
	int size = symbols.size();
	for (int i = 0; i < size; ++i)
		delete symbols[i];
	symbols.clear();
	
	size = templates.size();
	for (int i = 0; i < size; ++i)
		delete templates[i];
	templates.clear();
	first_front_template = 0;
	
	size = layers.size();
	for (int i = 0; i < size; ++i)
		delete layers[i];
	layers.clear();
	
	layers.push_back(new MapLayer(tr("default layer"), this));
	current_layer_index = 0;
	
	object_selection.clear();
	first_selected_object = NULL;
	
	widgets.clear();
	object_undo_manager.clear();
	
	map_notes = "";
	
	print_params_set = false;
	image_template_use_meters_per_pixel = true;
	image_template_meters_per_pixel = 0;
	image_template_dpi = 0;
	image_template_scale = 0;
	gps_projection_params_set = false;
	delete gps_projection_parameters;
	gps_projection_parameters = new GPSProjectionParameters();
	
	colors_dirty = false;
	symbols_dirty = false;
	templates_dirty = false;
	objects_dirty = false;
	unsaved_changes = false;
}

void Map::draw(QPainter* painter, QRectF bounding_box, bool force_min_size, float scaling, bool show_helper_symbols, float opacity)
{
	// Update the renderables of all objects marked as dirty
	updateObjects();
	
	// The actual drawing
	renderables.draw(painter, bounding_box, force_min_size, scaling, show_helper_symbols, opacity);
}
void Map::drawTemplates(QPainter* painter, QRectF bounding_box, int first_template, int last_template, bool draw_untransformed_parts, const QRect& untransformed_dirty_rect, MapWidget* widget, MapView* view)
{
	bool really_draw_untransformed_parts = draw_untransformed_parts && widget;

	for (int i = first_template; i <= last_template; ++i)
	{
		Template* temp = getTemplate(i);
		if ((view && !view->isTemplateVisible(temp)) || !temp->isTemplateValid())
			continue;
		float scale = (view ? view->getZoom() : 1) * std::max(temp->getTemplateScaleX(), temp->getTemplateScaleY());
		
		QRectF view_rect;
		if (temp->getTemplateRotation() != 0)
			view_rect = QRectF(-9e42, -9e42, 9e42, 9e42);	// TODO: transform base_view_rect (map coords) using template transform to template coords
		else
		{
			view_rect.setLeft((bounding_box.x() / temp->getTemplateScaleX()) - temp->getTemplateX() / 1000.0);
			view_rect.setTop((bounding_box.y() / temp->getTemplateScaleY()) - temp->getTemplateY() / 1000.0);
			view_rect.setRight((bounding_box.right() / temp->getTemplateScaleX()) - temp->getTemplateX() / 1000.0);
			view_rect.setBottom((bounding_box.bottom() / temp->getTemplateScaleY()) - temp->getTemplateY() / 1000.0);
		}
		
		if (really_draw_untransformed_parts)
			painter->save();
		painter->save();
		temp->applyTemplateTransform(painter);
		temp->drawTemplate(painter, view_rect, scale, view ? view->getTemplateVisibility(temp)->opacity : 1);
		painter->restore();
		
		if (really_draw_untransformed_parts)
		{
			painter->resetMatrix();
			temp->drawTemplateUntransformed(painter, untransformed_dirty_rect, widget);
			painter->restore();
		}
	}
}
void Map::updateObjects()
{
	// TODO: It maybe would be better if the objects entered themselves into a separate list when they get dirty so not all objects have to be traversed here
	int size = layers.size();
	for (int l = 0; l < size; ++l)
	{
		MapLayer* layer = layers[l];
		int obj_size = layer->getNumObjects();
		for (int i = 0; i < obj_size; ++i)
		{
			Object* object = layer->getObject(i);
			if (!object->update())
				continue;
		}
	}
}
void Map::removeRenderablesOfObject(Object* object, bool mark_area_as_dirty)
{
	renderables.removeRenderablesOfObject(object, mark_area_as_dirty);
	if (isObjectSelected(object))
		removeSelectionRenderables(object);
}
void Map::insertRenderablesOfObject(Object* object)
{
	renderables.insertRenderablesOfObject(object);
	if (isObjectSelected(object))
		addSelectionRenderables(object);
}

void Map::getSelectionToSymbolCompatibility(Symbol* symbol, bool& out_compatible, bool& out_different)
{
	out_compatible = symbol != NULL && (getNumSelectedObjects() > 0);
	out_different = false;
	
	if (symbol)
	{
		ObjectSelection::const_iterator it_end = selectedObjectsEnd();
		for (ObjectSelection::const_iterator it = selectedObjectsBegin(); it != it_end; ++it)
		{
			if (!symbol->isTypeCompatibleTo(*it))
			{
				out_compatible = false;
				out_different = true;
				return;
			}
			else if (symbol != (*it)->getSymbol())
				out_different = true;
		}
	}
}

void Map::includeSelectionRect(QRectF& rect)
{
	ObjectSelection::const_iterator it_end = selectedObjectsEnd();
	ObjectSelection::const_iterator it = selectedObjectsBegin();
	if (it != it_end)
		rectIncludeSafe(rect, (*it)->getExtent());
	else
		return;
	++it;
	
	while (it != it_end)
	{
		rectInclude(rect, (*it)->getExtent());
		++it;
	}
}
void Map::drawSelection(QPainter* painter, bool force_min_size, MapWidget* widget, RenderableContainer* replacement_renderables, bool draw_normal)
{
	const float selection_opacity_factor = draw_normal ? 1 : 0.4f;
	
	MapView* view = widget->getMapView();
	
	painter->save();
	painter->translate(widget->width() / 2.0 + view->getDragOffset().x(), widget->height() / 2.0 + view->getDragOffset().y());
	view->applyTransform(painter);
	
	if (!replacement_renderables)
		replacement_renderables = &selection_renderables;
	replacement_renderables->draw(painter, view->calculateViewedRect(widget->viewportToView(widget->rect())), force_min_size, view->calculateFinalZoomFactor(), true, selection_opacity_factor, !draw_normal);
	
	painter->restore();
}

void Map::addObjectToSelection(Object* object, bool emit_selection_changed)
{
	assert(!isObjectSelected(object));
	object_selection.insert(object);
	addSelectionRenderables(object);
	if (!first_selected_object)
		first_selected_object = object;
	if (emit_selection_changed)
		emit(objectSelectionChanged());
}
void Map::removeObjectFromSelection(Object* object, bool emit_selection_changed)
{
	bool removed = object_selection.remove(object);
	assert(removed && "Map::removeObjectFromSelection: object was not selected!");
	removeSelectionRenderables(object);
	if (first_selected_object == object)
		first_selected_object = object_selection.isEmpty() ? NULL : *object_selection.begin();
	if (emit_selection_changed)
		emit(objectSelectionChanged());
}
bool Map::removeSymbolFromSelection(Symbol* symbol, bool emit_selection_changed)
{
	bool removed_at_least_one_object = false;
	ObjectSelection::iterator it_end = object_selection.end();
	for (ObjectSelection::iterator it = object_selection.begin(); it != it_end; )
	{
		if ((*it)->getSymbol() != symbol)
		{
			++it;
			continue;
		}
		
		removed_at_least_one_object = true;
		removeSelectionRenderables(*it);
		Object* removed_object = *it;
		it = object_selection.erase(it);
		if (first_selected_object == removed_object)
			first_selected_object = object_selection.isEmpty() ? NULL : *object_selection.begin();
	}
	if (emit_selection_changed && removed_at_least_one_object)
		emit(objectSelectionChanged());
	return removed_at_least_one_object;
}
bool Map::isObjectSelected(Object* object)
{
	return object_selection.contains(object);
}
bool Map::toggleObjectSelection(Object* object, bool emit_selection_changed)
{
	if (isObjectSelected(object))
	{
		removeObjectFromSelection(object, emit_selection_changed);
		return false;
	}
	else
	{
		addObjectToSelection(object, emit_selection_changed);
		return true;
	}
}
void Map::clearObjectSelection(bool emit_selection_changed)
{
	selection_renderables.clear();
	object_selection.clear();
	first_selected_object = NULL;
	
	if (emit_selection_changed)
		emit(objectSelectionChanged());
}
void Map::emitSelectionChanged()
{
	emit(objectSelectionChanged());
}
void Map::emitSelectionEdited()
{
	emit(selectedObjectEdited());
}

void Map::addMapWidget(MapWidget* widget)
{
	widgets.push_back(widget);
}
void Map::removeMapWidget(MapWidget* widget)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
	{
		if (widgets[i] == widget)
		{
			widgets.erase(widgets.begin() + i);
			return;
		}
	}
	assert(false);
}
void Map::updateAllMapWidgets()
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->updateEverything();
}

/*void Map::addMapView(MapView* view)
{
	views.push_back(view);
}
void Map::removeMapView(MapView* view)
{
	for (int i = 0; i < (int)views.size(); ++i)
	{
		if (views[i] == view)
		{
			views.erase(views.begin() + i);
			return;
		}
	}
	assert(false);
}*/

void Map::setDrawingBoundingBox(QRectF map_coords_rect, int pixel_border, bool do_update)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->setDrawingBoundingBox(map_coords_rect, pixel_border, do_update);
}
void Map::clearDrawingBoundingBox()
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->clearDrawingBoundingBox();
}

void Map::setActivityBoundingBox(QRectF map_coords_rect, int pixel_border, bool do_update)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->setActivityBoundingBox(map_coords_rect, pixel_border, do_update);
}
void Map::clearActivityBoundingBox()
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->clearActivityBoundingBox();
}

void Map::updateDrawing(QRectF map_coords_rect, int pixel_border)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->updateDrawing(map_coords_rect, pixel_border);
}

void Map::setColor(MapColor* color, int pos)
{
	color_set->colors[pos] = color;
	color->priority = pos;
	
	emit(colorChanged(pos, color));
}
MapColor* Map::addColor(int pos)
{
	MapColor* new_color = new MapColor();
	new_color->name = tr("New color");
	new_color->priority = pos;
	new_color->c = 0;
	new_color->m = 0;
	new_color->y = 0;
	new_color->k = 1;
	new_color->opacity = 1;
	new_color->updateFromCMYK();
	
	color_set->colors.insert(color_set->colors.begin() + pos, new_color);
	adjustColorPriorities(pos + 1, color_set->colors.size() - 1);
	checkIfFirstColorAdded();
	emit(colorAdded(pos, new_color));
	return new_color;
}
void Map::addColor(MapColor* color, int pos)
{
	color_set->colors.insert(color_set->colors.begin() + pos, color);
	adjustColorPriorities(pos + 1, color_set->colors.size() - 1);
	checkIfFirstColorAdded();
	emit(colorAdded(pos, color));
	color->priority = pos;
}
void Map::deleteColor(int pos)
{
	MapColor* temp = color_set->colors[pos];
	delete color_set->colors[pos];
	color_set->colors.erase(color_set->colors.begin() + pos);
	adjustColorPriorities(pos, color_set->colors.size() - 1);
	
	if (getNumColors() == 0)
	{
		// That was the last color - the help text in the map widget(s) should be updated
		updateAllMapWidgets();
	}
	
	int size = (int)symbols.size();
	for (int i = 0; i < size; ++i)
		symbols[i]->colorDeleted(this, pos, temp);
	emit(colorDeleted(pos, temp));
}
int Map::findColorIndex(MapColor* color)
{
	int size = (int)color_set->colors.size();
	for (int i = 0; i < size; ++i)
	{
		if (color_set->colors[i] == color)
			return i;
	}
	return -1;
}
void Map::setColorsDirty()
{
	setHasUnsavedChanges();
	colors_dirty = true;
}

void Map::useColorsFrom(Map* map)
{
	color_set->dereference();
	color_set = map->color_set;
	color_set->addReference();
}
bool Map::isColorUsedByASymbol(MapColor* color)
{
	int size = (int)symbols.size();
	for (int i = 0; i < size; ++i)
	{
		if (symbols[i]->containsColor(color))
			return true;
	}
	return false;
}

void Map::adjustColorPriorities(int first, int last)
{
	// TODO: delete or update RenderStates with these colors
	for (int i = first; i <= last; ++i)
		color_set->colors[i]->priority = i;
}

void Map::addSelectionRenderables(Object* object)
{
	object->update(false);
	selection_renderables.insertRenderablesOfObject(object);
}
void Map::updateSelectionRenderables(Object* object)
{
	removeSelectionRenderables(object);
	addSelectionRenderables(object);
}
void Map::removeSelectionRenderables(Object* object)
{
	selection_renderables.removeRenderablesOfObject(object, false);
}

void Map::initStatic()
{
	static_initialized = true;
	
	// Covering colors and symbols
	covering_white.opacity = 1000;	// HACK: (almost) always opaque, even if multiplied by opacity factors
	covering_white.r = 1;
	covering_white.g = 1;
	covering_white.b = 1;
	covering_white.updateFromRGB();
	covering_white.priority = MapColor::CoveringWhite;
	
	covering_red.opacity = 1000;
	covering_red.r = 1;
	covering_red.g = 0;
	covering_red.b = 0;
	covering_red.updateFromRGB();
	covering_red.priority = MapColor::CoveringRed;
	
	covering_white_line = new LineSymbol();
	covering_white_line->setColor(&covering_white);
	covering_white_line->setLineWidth(3);
	
	covering_red_line = new LineSymbol();
	covering_red_line->setColor(&covering_red);
	covering_red_line->setLineWidth(0.1);
	
	covering_combined_line = new CombinedSymbol();
	covering_combined_line->setNumParts(2);
	covering_combined_line->setPart(0, covering_white_line);
	covering_combined_line->setPart(1, covering_red_line);
	
	// Undefined symbols
	undefined_symbol_color.opacity = 1;
	undefined_symbol_color.r = 0.5f;
	undefined_symbol_color.g = 0.5f;
	undefined_symbol_color.b = 0.5f;
	undefined_symbol_color.updateFromRGB();
	undefined_symbol_color.priority = MapColor::Undefined;
	
	undefined_line = new LineSymbol();
	undefined_line->setColor(&undefined_symbol_color);
	undefined_line->setLineWidth(1);
	undefined_line->setIsHelperSymbol(true);

	undefined_point = new PointSymbol();
	undefined_point->setInnerRadius(100);
	undefined_point->setInnerColor(&undefined_symbol_color);
	undefined_point->setIsHelperSymbol(true);
}

void Map::addSymbol(Symbol* symbol, int pos)
{
	symbols.insert(symbols.begin() + pos, symbol);
	checkIfFirstSymbolAdded();
	emit(symbolAdded(pos, symbol));
	setSymbolsDirty();
}
void Map::moveSymbol(int from, int to)
{
	symbols.insert(symbols.begin() + to, symbols[from]);
	if (from > to)
		++from;
	symbols.erase(symbols.begin() + from);
	// TODO: emit(symbolChanged(pos, symbol)); ?
	setSymbolsDirty();
}

void Map::sortSymbols(bool (*cmp)(Symbol *, Symbol *)) {
    if (!cmp) return;
    std::stable_sort(symbols.begin(), symbols.end(), cmp);
    // TODO: emit(symbolChanged(pos, symbol)); ? s/b same choice as for above, in moveSymbol()
    setSymbolsDirty();
}

void Map::setSymbol(Symbol* symbol, int pos)
{
	changeSymbolForAllObjects(symbols[pos], symbol);
	
	int size = (int)symbols.size();
	for (int i = 0; i < size; ++i)
	{
		if (i == pos)
			continue;
		
		if (symbols[i]->symbolChanged(symbols[pos], symbol))
			updateAllObjectsWithSymbol(symbols[i]);
	}
	
	// Change the symbol
	Symbol* old_symbol = symbols[pos];
	delete old_symbol;
	symbols[pos] = symbol;
	
	emit(symbolChanged(pos, symbol, old_symbol));
	setSymbolsDirty();
}
void Map::deleteSymbol(int pos)
{
	if (deleteAllObjectsWithSymbol(symbols[pos]))
		object_undo_manager.clear();
	
	int size = (int)symbols.size();
	for (int i = 0; i < size; ++i)
	{
		if (i == pos)
			continue;
		
		if (symbols[i]->symbolChanged(symbols[pos], NULL))
			updateAllObjectsWithSymbol(symbols[i]);
	}
	
	// Delete the symbol
	Symbol* temp = symbols[pos];
	delete symbols[pos];
	symbols.erase(symbols.begin() + pos);
	
	if (getNumSymbols() == 0)
	{
		// That was the last symbol - the help text in the map widget(s) should be updated
		updateAllMapWidgets();
	}
	
	emit(symbolDeleted(pos, temp));
	setSymbolsDirty();
}
int Map::findSymbolIndex(Symbol* symbol)
{
	int size = (int)symbols.size();
	for (int i = 0; i < size; ++i)
	{
		if (symbols[i] == symbol)
			return i;
	}
	
	if (symbol == undefined_point)
		return -2;
	else if (symbol == undefined_line)
		return -3;
	
	assert(false);
	return -1;
}
void Map::setSymbolsDirty()
{
	setHasUnsavedChanges();
	symbols_dirty = true;
}

void Map::scaleAllSymbols(double factor)
{
	int size = getNumSymbols();
	for (int i = 0; i < size; ++i)
	{
		Symbol* symbol = getSymbol(i);
		
		symbol->scale(factor);
		symbol->getIcon(this, true);
		
		emit(symbolChanged(i, symbol, symbol));
	}
	updateAllObjects();
	
	setSymbolsDirty();
}

int Map::getTemplateNumber(Template* temp) const
{
	for (int i = 0; i < (int)templates.size(); ++i)
	{
		if (templates[i] == temp)
			return i;
	}
	assert(false);
	return -1;
}
void Map::setTemplate(Template* temp, int pos)
{
	templates[pos] = temp;
	emit(templateChanged(pos, templates[pos]));
}
void Map::addTemplate(Template* temp, int pos)
{
	templates.insert(templates.begin() + pos, temp);
	checkIfFirstTemplateAdded();
	
	emit(templateAdded(pos, temp));
}
void Map::deleteTemplate(int pos)
{
	TemplateVector::iterator it = templates.begin() + pos;
	Template* temp = *it;
	
	// Delete visibility information for the template from all views - get the views indirectly by iterating over all widgets
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->getMapView()->deleteTemplateVisibility(temp);
	
	templates.erase(it);
	
	if (getNumTemplates() == 0)
	{
		// That was the last tempate - the help text in the map widget(s) should maybe be updated (if there are no objects)
		updateAllMapWidgets();
	}
	
	emit(templateDeleted(pos, temp));
	delete temp;
}
void Map::setTemplateAreaDirty(Template* temp, QRectF area, int pixel_border)
{
	bool front_cache = findTemplateIndex(temp) >= getFirstFrontTemplate();	// TODO: is there a better way to find out if that is a front or back template?
	
	for (int i = 0; i < (int)widgets.size(); ++i)
		if (widgets[i]->getMapView()->isTemplateVisible(temp))
			widgets[i]->markTemplateCacheDirty(widgets[i]->getMapView()->calculateViewBoundingBox(area), pixel_border, front_cache);
}
void Map::setTemplateAreaDirty(int i)
{
	if (i == -1)
		return;	// no assert here as convenience, so setTemplateAreaDirty(-1) can be called without effect for the map layer
	assert(i >= 0 && i < (int)templates.size());
	
	templates[i]->setTemplateAreaDirty();
}
int Map::findTemplateIndex(Template* temp)
{
	int size = (int)templates.size();
	for (int i = 0; i < size; ++i)
	{
		if (templates[i] == temp)
			return i;
	}
	assert(false);
	return -1;
}
void Map::setTemplatesDirty()
{
	setHasUnsavedChanges();
	templates_dirty = true;
}
void Map::emitTemplateChanged(Template* temp)
{
	emit(templateChanged(findTemplateIndex(temp), temp));
}

int Map::getNumObjects()
{
	int num_objects = 0;
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		num_objects += layers[i]->getNumObjects();
	return num_objects;
}
int Map::addObject(Object* object, int layer_index)
{
	MapLayer* layer = layers[(layer_index < 0) ? current_layer_index : layer_index];
	int object_index = layer->getNumObjects();
	layer->addObject(object, object_index);
	
	return object_index;
}
void Map::deleteObject(Object* object, bool remove_only)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
	{
		if (layers[i]->deleteObject(object, remove_only))
			return;
	}
	assert(false);
}
void Map::setObjectsDirty()
{
	setHasUnsavedChanges();
	objects_dirty = true;
}

QRectF Map::calculateExtent(bool include_helper_symbols, bool include_templates, MapView* view)
{
	QRectF rect;
	
	// Objects
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		rectIncludeSafe(rect, layers[i]->calculateExtent(include_helper_symbols));
	
	// Templates
	if (include_templates)
	{
		size = templates.size();
		for (int i = 0; i < size; ++i)
		{
			if (view && !view->isTemplateVisible(templates[i]))
				continue;
			
			QRectF template_extent = templates[i]->getExtent();
			rectInclude(rect, templates[i]->templateToMap(template_extent.topLeft()));
			rectInclude(rect, templates[i]->templateToMap(template_extent.topRight()));
			rectInclude(rect, templates[i]->templateToMap(template_extent.bottomRight()));
			rectInclude(rect, templates[i]->templateToMap(template_extent.bottomLeft()));
		}
	}
	
	return rect;
}
void Map::setObjectAreaDirty(QRectF map_coords_rect)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->markObjectAreaDirty(map_coords_rect);
}
void Map::findObjectsAt(MapCoordF coord, float tolerance, bool extended_selection, bool include_hidden_objects, bool include_protected_objects, SelectionInfoVector& out)
{
	getCurrentLayer()->findObjectsAt(coord, tolerance, extended_selection, include_hidden_objects, include_protected_objects, out);
}
void Map::findObjectsAtBox(MapCoordF corner1, MapCoordF corner2, bool include_hidden_objects, bool include_protected_objects, std::vector< Object* >& out)
{
	getCurrentLayer()->findObjectsAtBox(corner1, corner2, include_hidden_objects, include_protected_objects, out);
}

void Map::scaleAllObjects(double factor)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		layers[i]->scaleAllObjects(factor);
}
void Map::updateAllObjects(bool remove_old_renderables)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		layers[i]->updateAllObjects(remove_old_renderables);
}
void Map::updateAllObjectsWithSymbol(Symbol* symbol)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		layers[i]->updateAllObjectsWithSymbol(symbol);
}
void Map::changeSymbolForAllObjects(Symbol* old_symbol, Symbol* new_symbol)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		layers[i]->changeSymbolForAllObjects(old_symbol, new_symbol);
}
bool Map::deleteAllObjectsWithSymbol(Symbol* symbol)
{
	bool object_deleted = false;
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		object_deleted = object_deleted || layers[i]->deleteAllObjectsWithSymbol(symbol);
	return object_deleted;
}
bool Map::doObjectsExistWithSymbol(Symbol* symbol)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
	{
		if (layers[i]->doObjectsExistWithSymbol(symbol))
			return true;
	}
	return false;
}
void Map::forceUpdateOfAllObjects(Symbol* with_symbol)
{
	int size = layers.size();
	for (int i = 0; i < size; ++i)
		layers[i]->forceUpdateOfAllObjects(with_symbol);
}

void Map::setGPSProjectionParameters(const GPSProjectionParameters& params)
{
	*gps_projection_parameters = params;
	gps_projection_parameters->update();
	gps_projection_params_set = true;
	emit(gpsProjectionParametersChanged());
	
	setHasUnsavedChanges();
}

void Map::setPrintParameters(int orientation, int format, float dpi, bool show_templates, bool center, float left, float top, float width, float height)
{
	if ((print_orientation != orientation) || (print_format != format) || (print_dpi != dpi) || (print_show_templates != show_templates) ||
		(print_center != center) || (print_area_left != left) || (print_area_top != top) || (print_area_width != width) || (print_area_height != height))
		setHasUnsavedChanges();

	print_orientation = orientation;
	print_format = format;
	print_dpi = dpi;
	print_show_templates = show_templates;
	print_center = center;
	print_area_left = left;
	print_area_top = top;
	print_area_width = width;
	print_area_height = height;
	
	print_params_set = true;
}
void Map::getPrintParameters(int& orientation, int& format, float& dpi, bool& show_templates, bool& center, float& left, float& top, float& width, float& height)
{
	orientation = print_orientation;
	format = print_format;
	dpi = print_dpi;
	show_templates = print_show_templates;
	center = print_center;
	left = print_area_left;
	top = print_area_top;
	width = print_area_width;
	height = print_area_height;
}

void Map::setImageTemplateDefaults(bool use_meters_per_pixel, double meters_per_pixel, double dpi, double scale)
{
	image_template_use_meters_per_pixel = use_meters_per_pixel;
	image_template_meters_per_pixel = meters_per_pixel;
	image_template_dpi = dpi;
	image_template_scale = scale;
}
void Map::getImageTemplateDefaults(bool& use_meters_per_pixel, double& meters_per_pixel, double& dpi, double& scale)
{
	use_meters_per_pixel = image_template_use_meters_per_pixel;
	meters_per_pixel = image_template_meters_per_pixel;
	dpi = image_template_dpi;
	scale = image_template_scale;
}

void Map::setHasUnsavedChanges(bool has_unsaved_changes)
{
	if (!has_unsaved_changes)
	{
		colors_dirty = false;
		symbols_dirty = false;
		templates_dirty = false;
		objects_dirty = false;
		unsaved_changes = false;
	}
	else
	{
		if (!unsaved_changes)
		{
			emit(gotUnsavedChanges());
			unsaved_changes = true;
		}
	}
}

void Map::checkIfFirstColorAdded()
{
	if (getNumColors() == 1)
	{
		// This is the first color - the help text in the map widget(s) should be updated
		updateAllMapWidgets();
	}
}
void Map::checkIfFirstSymbolAdded()
{
	if (getNumSymbols() == 1)
	{
		// This is the first symbol - the help text in the map widget(s) should be updated
		updateAllMapWidgets();
	}
}
void Map::checkIfFirstTemplateAdded()
{
	if (getNumTemplates() == 1)
	{
		// This is the first template - the help text in the map widget(s) should be updated
		updateAllMapWidgets();
	}
}

// ### MapView ###

const double screen_pixel_per_mm = 4.999838577;	// TODO: make configurable (by specifying screen diameter in inch + resolution, or dpi)
// Calculation: pixel_height / ( sqrt((c^2)/(aspect^2 + 1)) * 2.54 )

const double MapView::zoom_in_limit = 512;
const double MapView::zoom_out_limit = 1 / 16.0;

MapView::MapView(Map* map) : map(map)
{
	zoom = 1;
	rotation = 0;
	position_x = 0;
	position_y = 0;
	view_x = 0;
	view_y = 0;
	update();
	
	//map->addMapView(this);
}
MapView::~MapView()
{
	//map->removeMapView(this);
	
	foreach (TemplateVisibility* vis, template_visibilities)
		delete vis;
}

void MapView::save(QFile* file)
{
	file->write((const char*)&zoom, sizeof(double));
	file->write((const char*)&rotation, sizeof(double));
	file->write((const char*)&position_x, sizeof(qint64));
	file->write((const char*)&position_y, sizeof(qint64));
	file->write((const char*)&view_x, sizeof(int));
	file->write((const char*)&view_y, sizeof(int));
	file->write((const char*)&drag_offset, sizeof(QPoint));
	
	int num_template_visibilities = template_visibilities.size();
	file->write((const char*)&num_template_visibilities, sizeof(int));
	QHash<Template*, TemplateVisibility*>::const_iterator it = template_visibilities.constBegin();
	while (it != template_visibilities.constEnd())
	{
		int pos = map->getTemplateNumber(it.key());
		file->write((const char*)&pos, sizeof(int));
		
		file->write((const char*)&it.value()->visible, sizeof(bool));
		file->write((const char*)&it.value()->opacity, sizeof(float));
		
		++it;
	}
}
void MapView::load(QFile* file)
{
	file->read((char*)&zoom, sizeof(double));
	file->read((char*)&rotation, sizeof(double));
	file->read((char*)&position_x, sizeof(qint64));
	file->read((char*)&position_y, sizeof(qint64));
	file->read((char*)&view_x, sizeof(int));
	file->read((char*)&view_y, sizeof(int));
	file->read((char*)&drag_offset, sizeof(QPoint));
	update();
	
	int num_template_visibilities;
	file->read((char*)&num_template_visibilities, sizeof(int));
	
	for (int i = 0; i < num_template_visibilities; ++i)
	{
		int pos;
		file->read((char*)&pos, sizeof(int));
		
		TemplateVisibility* vis = getTemplateVisibility(map->getTemplate(pos));
		file->read((char*)&vis->visible, sizeof(bool));
		file->read((char*)&vis->opacity, sizeof(float));
	}
}

void MapView::addMapWidget(MapWidget* widget)
{
	widgets.push_back(widget);
	
	map->addMapWidget(widget);
}
void MapView::removeMapWidget(MapWidget* widget)
{
	for (int i = 0; i < (int)widgets.size(); ++i)
	{
		if (widgets[i] == widget)
		{
			widgets.erase(widgets.begin() + i);
			return;
		}
	}
	assert(false);
	
	map->removeMapWidget(widget);
}
void MapView::updateAllMapWidgets()
{
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->updateEverything();
}

double MapView::lengthToPixel(qint64 length)
{
	return zoom * screen_pixel_per_mm * (length / 1000.0);
}
qint64 MapView::pixelToLength(double pixel)
{
	return qRound64(1000 * pixel / (zoom * screen_pixel_per_mm));
}

QRectF MapView::calculateViewedRect(QRectF view_rect)
{
	QPointF min = view_rect.topLeft();
	QPointF max = view_rect.bottomRight();
	MapCoordF top_left = viewToMapF(min.x(), min.y());
	MapCoordF top_right = viewToMapF(max.x(), min.y());
	MapCoordF bottom_right = viewToMapF(max.x(), max.y());
	MapCoordF bottom_left = viewToMapF(min.x(), max.y());
	
	QRectF result = QRectF(top_left.getX(), top_left.getY(), 0, 0);
	rectInclude(result, QPointF(top_right.getX(), top_right.getY()));
	rectInclude(result, QPointF(bottom_right.getX(), bottom_right.getY()));
	rectInclude(result, QPointF(bottom_left.getX(), bottom_left.getY()));
	
	return QRectF(result.left() - 0.001, result.top() - 0.001, result.width() + 0.002, result.height() + 0.002);
}
QRectF MapView::calculateViewBoundingBox(QRectF map_rect)
{
	QPointF min = map_rect.topLeft();
	MapCoord map_min = MapCoord(min.x(), min.y());
	QPointF max = map_rect.bottomRight();
	MapCoord map_max = MapCoord(max.x(), max.y());
	
	QPointF top_left;
	mapToView(map_min, top_left.rx(), top_left.ry());
	QPointF bottom_right;
	mapToView(map_max, bottom_right.rx(), bottom_right.ry());
	
	MapCoord map_top_right = MapCoord(max.x(), min.y());
	QPointF top_right;
	mapToView(map_top_right, top_right.rx(), top_right.ry());
	
	MapCoord map_bottom_left = MapCoord(min.x(), max.y());
	QPointF bottom_left;
	mapToView(map_bottom_left, bottom_left.rx(), bottom_left.ry());
	
	QRectF result = QRectF(top_left.x(), top_left.y(), 0, 0);
	rectInclude(result, QPointF(top_right.x(), top_right.y()));
	rectInclude(result, QPointF(bottom_right.x(), bottom_right.y()));
	rectInclude(result, QPointF(bottom_left.x(), bottom_left.y()));
	
	return QRectF(result.left() - 1, result.top() - 1, result.width() + 2, result.height() + 2);
}

void MapView::applyTransform(QPainter* painter)
{
	QTransform world_transform;
	// NOTE: transposing the matrix here ...
	world_transform.setMatrix(map_to_view.get(0, 0), map_to_view.get(1, 0), map_to_view.get(2, 0),
							  map_to_view.get(0, 1), map_to_view.get(1, 1), map_to_view.get(2, 1),
							  map_to_view.get(0, 2), map_to_view.get(1, 2), map_to_view.get(2, 2));
	painter->setWorldTransform(world_transform, true);
}

void MapView::setDragOffset(QPoint offset)
{
	drag_offset = offset;
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->setDragOffset(drag_offset);
}
void MapView::completeDragging(QPoint offset)
{
	drag_offset = QPoint(0, 0);
	qint64 move_x = -pixelToLength(offset.x());
	qint64 move_y = -pixelToLength(offset.y());
	
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->completeDragging(offset, move_x, move_y);
	
	position_x += move_x;
	position_y += move_y;
	update();
}

bool MapView::zoomSteps(float num_steps, bool preserve_cursor_pos, QPointF cursor_pos_view)
{
	num_steps = 0.5f * num_steps;
	
	if (num_steps > 0)
	{
		// Zooming in - adjust camera position so the cursor stays at the same position on the map
		if (getZoom() >= zoom_in_limit)
			return false;
		
		bool set_to_limit = false;
		double zoom_to = pow(2, (log10(getZoom()) / LOG2) + num_steps);
		double zoom_factor = zoom_to / getZoom();
		if (getZoom() * zoom_factor > zoom_in_limit)
		{
			zoom_factor = zoom_in_limit / getZoom();
			set_to_limit = true;
		}
		
		MapCoordF mouse_pos_map(0, 0);
		MapCoordF mouse_pos_to_view_center(0, 0);
		if (preserve_cursor_pos)
		{
			mouse_pos_map = viewToMapF(cursor_pos_view);
			mouse_pos_to_view_center = MapCoordF(getPositionX()/1000.0 - mouse_pos_map.getX(), getPositionY()/1000.0 - mouse_pos_map.getY());
			mouse_pos_to_view_center = MapCoordF(mouse_pos_to_view_center.getX() * 1 / zoom_factor, mouse_pos_to_view_center.getY() * 1 / zoom_factor);
		}
		
		setZoom(set_to_limit ? zoom_in_limit : (getZoom() * zoom_factor));
		if (preserve_cursor_pos)
		{
			setPositionX(qRound64(1000 * (mouse_pos_map.getX() + mouse_pos_to_view_center.getX())));
			setPositionY(qRound64(1000 * (mouse_pos_map.getY() + mouse_pos_to_view_center.getY())));
		}
	}
	else
	{
		// Zooming out
		if (getZoom() <= zoom_out_limit)
			return false;
		
		bool set_to_limit = false;
		double zoom_to = pow(2, (log10(getZoom()) / LOG2) + num_steps);
		double zoom_factor = zoom_to / getZoom();
		if (getZoom() * zoom_factor < zoom_out_limit)
		{
			zoom_factor = zoom_out_limit / getZoom();
			set_to_limit = true;
		}
		
		MapCoordF mouse_pos_map(0, 0);
		MapCoordF mouse_pos_to_view_center(0, 0);
		if (preserve_cursor_pos)
		{
			mouse_pos_map = viewToMapF(cursor_pos_view);
			mouse_pos_to_view_center = MapCoordF(getPositionX()/1000.0 - mouse_pos_map.getX(), getPositionY()/1000.0 - mouse_pos_map.getY());
			mouse_pos_to_view_center = MapCoordF(mouse_pos_to_view_center.getX() * 1 / zoom_factor, mouse_pos_to_view_center.getY() * 1 / zoom_factor);
		}
		
		setZoom(set_to_limit ? zoom_out_limit : (getZoom() * zoom_factor));
		
		if (preserve_cursor_pos)
		{
			setPositionX(qRound64(1000 * (mouse_pos_map.getX() + mouse_pos_to_view_center.getX())));
			setPositionY(qRound64(1000 * (mouse_pos_map.getY() + mouse_pos_to_view_center.getY())));
		}
	}
	return true;
}

void MapView::setZoom(float value)
{
	float zoom_factor = value / zoom;
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->zoom(zoom_factor);
	
	zoom = value;
	update();
	
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->updateZoomLabel();
}
void MapView::setPositionX(qint64 value)
{
	qint64 offset = value - position_x;
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->moveView(offset, 0);
	
	position_x = value;
	update();
}
void MapView::setPositionY(qint64 value)
{
	qint64 offset = value - position_y;
	for (int i = 0; i < (int)widgets.size(); ++i)
		widgets[i]->moveView(0, offset);
	
	position_y = value;
	update();
}

void MapView::update()
{
	double cosr = cos(rotation);
	double sinr = sin(rotation);
	
	double final_zoom = lengthToPixel(1000);
	
	// Create map_to_view
	map_to_view.setSize(3, 3);
	map_to_view.set(0, 0, final_zoom * cosr);
	map_to_view.set(0, 1, final_zoom * (-sinr));
	map_to_view.set(1, 0, final_zoom * sinr);
	map_to_view.set(1, 1, final_zoom * cosr);
	map_to_view.set(0, 2, -final_zoom*(position_x/1000.0)*cosr + final_zoom*(position_y/1000.0)*sinr - view_x);
	map_to_view.set(1, 2, -final_zoom*(position_x/1000.0)*sinr - final_zoom*(position_y/1000.0)*cosr - view_y);
	map_to_view.set(2, 0, 0);
	map_to_view.set(2, 1, 0);
	map_to_view.set(2, 2, 1);
	
	// Create view_to_map
	map_to_view.invert(view_to_map);
}

bool MapView::isTemplateVisible(Template* temp)
{
	if (template_visibilities.contains(temp))
	{
		TemplateVisibility* vis = template_visibilities.value(temp);
		return vis->visible && vis->opacity > 0;
	}
	else
		return false;
}
TemplateVisibility* MapView::getTemplateVisibility(Template* temp)
{
	if (!template_visibilities.contains(temp))
	{
		TemplateVisibility* vis = new TemplateVisibility();
		template_visibilities.insert(temp, vis);
		return vis;
	}
	else
		return template_visibilities.value(temp);
}
void MapView::deleteTemplateVisibility(Template* temp)
{
	delete template_visibilities.value(temp);
	template_visibilities.remove(temp);
}

#include "map.moc"