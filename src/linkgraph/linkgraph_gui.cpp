/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_gui.cpp Implementation of linkgraph overlay GUI. */

#include "../stdafx.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../date_func.h"
#include "../viewport_func.h"
#include "../zoom_func.h"
#include "../smallmap_gui.h"
#include "../zoom_func.h"
#include "../landscape.h"
#include "../core/geometry_func.hpp"
#include "../widgets/link_graph_legend_widget.h"

#include "table/strings.h"

#include "../3rdparty/cpp-btree/btree_map.h"

#include <algorithm>

#include "../safeguards.h"

/**
 * Colours for the various "load" states of links. Ordered from "unused" to
 * "overloaded".
 */
const uint8 LinkGraphOverlay::LINK_COLOURS[][12] = {
{
	0x0f, 0xd1, 0xd0, 0x57,
	0x55, 0x53, 0xbf, 0xbd,
	0xba, 0xb9, 0xb7, 0xb5
},
{
	0x0f, 0xd1, 0xd0, 0x57,
	0x55, 0x53, 0x96, 0x95,
	0x94, 0x93, 0x92, 0x91
},
{
	0x0f, 0x0b, 0x09, 0x07,
	0x05, 0x03, 0xbf, 0xbd,
	0xba, 0xb9, 0xb7, 0xb5
},
{
	0x0f, 0x0b, 0x0a, 0x09,
	0x08, 0x07, 0x06, 0x05,
	0x04, 0x03, 0x02, 0x01
}
};

/**
 * Get a DPI for the widget we will be drawing to.
 * @param dpi DrawPixelInfo to fill with the desired dimensions.
 */
void LinkGraphOverlay::GetWidgetDpi(DrawPixelInfo *dpi, uint margin) const
{
	const NWidgetBase *wi = this->window->GetWidget<NWidgetBase>(this->widget_id);
	dpi->left = dpi->top = -(int)margin;
	dpi->width = wi->current_x + 2 * margin;
	dpi->height = wi->current_y + 2 * margin;
}

bool LinkGraphOverlay::CacheStillValid() const
{
	if (this->window->viewport) {
		const Viewport *vp = this->window->viewport;
		Rect region { vp->virtual_left, vp->virtual_top,
				vp->virtual_left + vp->virtual_width, vp->virtual_top + vp->virtual_height };
		return (region.left >= this->cached_region.left &&
				region.right <= this->cached_region.right &&
				region.top >= this->cached_region.top &&
				region.bottom <= this->cached_region.bottom);
	} else {
		return true;
	}
}

void LinkGraphOverlay::MarkStationViewportLinksDirty(const Station *st)
{
	if (this->window->viewport) {
		Viewport *vp = this->window->viewport;
		const Point pt = RemapCoords2(TileX(st->xy) * TILE_SIZE, TileY(st->xy) * TILE_SIZE);
		const int padding = ScaleByZoom(3 * this->scale, vp->zoom);
		MarkViewportDirty(vp, pt.x - padding, pt.y - padding, pt.x + padding, pt.y - padding, VMDF_NOT_LANDSCAPE);

		const int block_radius = ScaleByZoom(10, vp->zoom);
		for (LinkList::iterator i(this->cached_links.begin()); i != this->cached_links.end(); ++i) {
			if (i->from_id == st->index) {
				const Station *stb = Station::GetIfValid(i->to_id);
				if (stb == nullptr) continue;
				MarkViewportLineDirty(vp, pt, RemapCoords2(TileX(stb->xy) * TILE_SIZE, TileY(stb->xy) * TILE_SIZE), block_radius, VMDF_NOT_LANDSCAPE);
			} else if (i->to_id == st->index) {
			const Station *sta = Station::GetIfValid(i->from_id);
			if (sta == nullptr) continue;
				MarkViewportLineDirty(vp, RemapCoords2(TileX(sta->xy) * TILE_SIZE, TileY(sta->xy) * TILE_SIZE), pt, block_radius, VMDF_NOT_LANDSCAPE);
			}
		}
	}
}

/**
 * Rebuild the cache and recalculate which links and stations to be shown.
 */
void LinkGraphOverlay::RebuildCache(bool incremental)
{
	if (!incremental) {
		this->dirty = false;
		this->cached_links.clear();
		this->cached_stations.clear();
		this->last_update_number = GetWindowUpdateNumber();
	}
	if (this->company_mask == 0) return;

	DrawPixelInfo dpi;
	bool cache_all = false;
	if (this->window->viewport) {
		const Viewport *vp = this->window->viewport;
		const int pixel_margin = 256;
		const int vp_margin = ScaleByZoom(pixel_margin, vp->zoom);
		this->GetWidgetDpi(&dpi, pixel_margin);
		this->cached_region = Rect({ vp->virtual_left - vp_margin, vp->virtual_top - vp_margin,
				vp->virtual_left + vp->virtual_width + vp_margin, vp->virtual_top + vp->virtual_height + vp_margin });
	} else {
		this->GetWidgetDpi(&dpi);
		cache_all = true;
	}

	struct LinkCacheItem {
		Point from_pt;
		Point to_pt;
		LinkProperties prop;
	};
	btree::btree_map<std::pair<StationID, StationID>, LinkCacheItem> link_cache_map;
	std::vector<StationID> incremental_station_exclude;
	std::vector<std::pair<StationID, StationID>> incremental_link_exclude;

	if (incremental) {
		incremental_station_exclude.reserve(this->cached_stations.size());
		for (StationSupplyList::iterator i(this->cached_stations.begin()); i != this->cached_stations.end(); ++i) {
			incremental_station_exclude.push_back(i->id);
		}
		incremental_link_exclude.reserve(this->cached_links.size());
		for (LinkList::iterator i(this->cached_links.begin()); i != this->cached_links.end(); ++i) {
			incremental_link_exclude.push_back(std::make_pair(i->from_id, i->to_id));
		}
	}

	auto AddLinks = [&](const Station *from, const Station *to, Point from_pt, Point to_pt, btree::btree_map<std::pair<StationID, StationID>, LinkCacheItem>::iterator insert_iter) {
		LinkCacheItem *item = nullptr;
		for (CargoID c : SetCargoBitIterator(this->cargo_mask)) {
			if (!CargoSpec::Get(c)->IsValid()) continue;
			const GoodsEntry &ge = from->goods[c];
			if (!LinkGraph::IsValidID(ge.link_graph) ||
					ge.link_graph != to->goods[c].link_graph) {
				continue;
			}
			const LinkGraph &lg = *LinkGraph::Get(ge.link_graph);
			ConstEdge edge = lg.GetConstEdge(ge.node, to->goods[c].node);
			if (edge.Capacity() > 0) {
				if (!item) {
					auto iter = link_cache_map.insert(insert_iter, std::make_pair(std::make_pair(from->index, to->index), LinkCacheItem()));
					item = &(iter->second);
					item->from_pt = from_pt;
					item->to_pt = to_pt;
				}
				this->AddStats(c, lg.Monthly(edge.Capacity()), lg.Monthly(edge.Usage()),
						ge.flows.GetFlowVia(to->index),
						edge.TravelTime(),
						from->owner == OWNER_NONE || to->owner == OWNER_NONE,
						item->prop);
			}
		}
	};

	const size_t previous_cached_stations_count = this->cached_stations.size();
	for (const Station *sta : Station::Iterate()) {
		if (sta->rect.IsEmpty()) continue;

		if (incremental && std::binary_search(incremental_station_exclude.begin(), incremental_station_exclude.end(), sta->index)) continue;

		Point pta = this->GetStationMiddle(sta);

		StationID from = sta->index;

		uint supply = 0;
		for (CargoID c : SetCargoBitIterator(this->cargo_mask)) {
			if (!CargoSpec::Get(c)->IsValid()) continue;
			if (!LinkGraph::IsValidID(sta->goods[c].link_graph)) continue;
			const LinkGraph &lg = *LinkGraph::Get(sta->goods[c].link_graph);

			ConstNode from_node = lg[sta->goods[c].node];
			supply += lg.Monthly(from_node.Supply());
			lg.IterateEdgesFromNode(from_node.GetNodeID(), [&](NodeID from_id, NodeID to_id, ConstEdge edge) {
				StationID to = lg[to_id].Station();
				assert(from != to);
				if (!Station::IsValidID(to)) return;

				const Station *stb = Station::Get(to);
				assert(sta != stb);

				/* Show links between stations of selected companies or "neutral" ones like oilrigs. */
				if (stb->owner != OWNER_NONE && sta->owner != OWNER_NONE && !HasBit(this->company_mask, stb->owner)) return;
				if (stb->rect.IsEmpty()) return;

				if (incremental && std::binary_search(incremental_station_exclude.begin(), incremental_station_exclude.end(), to)) return;
				if (incremental && std::binary_search(incremental_link_exclude.begin(), incremental_link_exclude.end(), std::make_pair(from, to))) return;

				auto key = std::make_pair(from, to);
				auto iter = link_cache_map.lower_bound(key);
				if (iter != link_cache_map.end() && !(link_cache_map.key_comp()(key, iter->first))) {
					return;
				}

				Point ptb = this->GetStationMiddle(stb);

				if (!cache_all && !this->IsLinkVisible(pta, ptb, &dpi)) return;

				AddLinks(sta, stb, pta, ptb, iter);
			});
		}
		if (cache_all || this->IsPointVisible(pta, &dpi)) {
			this->cached_stations.push_back({ from, supply, pta });
		}
	}

	const size_t previous_cached_links_count = this->cached_links.size();
	this->cached_links.reserve(this->cached_links.size() + link_cache_map.size());
	for (auto &iter : link_cache_map) {
		this->cached_links.push_back({ iter.first.first, iter.first.second, iter.second.from_pt, iter.second.to_pt, iter.second.prop });
	}

	if (previous_cached_stations_count > 0) {
		std::inplace_merge(this->cached_stations.begin(), this->cached_stations.begin() + previous_cached_stations_count, this->cached_stations.end(),
				[](const StationSupplyInfo &a, const StationSupplyInfo &b) {
					return a.id < b.id;
				});
	}
	if (previous_cached_links_count > 0) {
		std::inplace_merge(this->cached_links.begin(), this->cached_links.begin() + previous_cached_links_count, this->cached_links.end(),
				[](const LinkInfo &a, const LinkInfo &b) {
					return std::make_pair(a.from_id, a.to_id) < std::make_pair(b.from_id, b.to_id);
				});
	}
}

/**
 * Determine if a certain point is inside the given DPI, with some lee way.
 * @param pt Point we are looking for.
 * @param dpi Visible area.
 * @param padding Extent of the point.
 * @return If the point or any of its 'extent' is inside the dpi.
 */
inline bool LinkGraphOverlay::IsPointVisible(Point pt, const DrawPixelInfo *dpi, int padding) const
{
	return pt.x > dpi->left - padding && pt.y > dpi->top - padding &&
			pt.x < dpi->left + dpi->width + padding &&
			pt.y < dpi->top + dpi->height + padding;
}

/**
 * Determine if a certain link crosses through the area given by the dpi with some lee way.
 * @param pta First end of the link.
 * @param ptb Second end of the link.
 * @param dpi Visible area.
 * @param padding Width or thickness of the link.
 * @return If the link or any of its "thickness" is visible. This may return false positives.
 */
inline bool LinkGraphOverlay::IsLinkVisible(Point pta, Point ptb, const DrawPixelInfo *dpi, int padding) const
{
	const int left = dpi->left - padding;
	const int right = dpi->left + dpi->width + padding;
	const int top = dpi->top - padding;
	const int bottom = dpi->top + dpi->height + padding;

	/*
	 * This method is an implementation of the Cohen-Sutherland line-clipping algorithm.
	 * See: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
	 */

	const uint8 INSIDE = 0; // 0000
	const uint8 LEFT   = 1; // 0001
	const uint8 RIGHT  = 2; // 0010
	const uint8 BOTTOM = 4; // 0100
	const uint8 TOP    = 8; // 1000

	int x0 = pta.x;
	int y0 = pta.y;
	int x1 = ptb.x;
	int y1 = ptb.y;

	auto out_code = [&](int x, int y) -> unsigned char {
		uint8 out = INSIDE;
		if (x < left) {
			out |= LEFT;
		} else if (x > right) {
			out |= RIGHT;
		}
		if (y < top) {
			out |= TOP;
		} else if (y > bottom) {
			out |= BOTTOM;
		}
		return out;
	};

	uint8 c0 = out_code(x0, y0);
	uint8 c1 = out_code(x1, y1);

	while (true) {
		if (c0 == 0 || c1 == 0) return true;
		if ((c0 & c1) != 0) return false;

		if (c0 & TOP) {           // point 0 is above the clip window
			x0 = x0 + (int)(((int64) (x1 - x0)) * ((int64) (top - y0)) / ((int64) (y1 - y0)));
			y0 = top;
		} else if (c0 & BOTTOM) { // point 0 is below the clip window
			x0 = x0 + (int)(((int64) (x1 - x0)) * ((int64) (bottom - y0)) / ((int64) (y1 - y0)));
			y0 = bottom;
		} else if (c0 & RIGHT) {  // point 0 is to the right of clip window
			y0 = y0 + (int)(((int64) (y1 - y0)) * ((int64) (right - x0)) / ((int64) (x1 - x0)));
			x0 = right;
		} else if (c0 & LEFT) {   // point 0 is to the left of clip window
			y0 = y0 + (int)(((int64) (y1 - y0)) * ((int64) (left - x0)) / ((int64) (x1 - x0)));
			x0 = left;
		}

		c0 = out_code(x0, y0);
	}

	NOT_REACHED();
}

/**
 * Add information from a given pair of link stat and flow stat to the given
 * link properties. The shown usage or plan is always the maximum of all link
 * stats involved.
 * @param new_cap Capacity of the new link.
 * @param new_usg Usage of the new link.
 * @param new_plan Planned flow for the new link.
 * @param new_shared If the new link is shared.
 * @param cargo LinkProperties to write the information to.
 */
/* static */ void LinkGraphOverlay::AddStats(CargoID new_cargo, uint new_cap, uint new_usg, uint new_plan, uint32 time, bool new_shared, LinkProperties &cargo)
{
	/* multiply the numbers by 32 in order to avoid comparing to 0 too often. */
	if (cargo.capacity == 0 ||
			cargo.Usage() * 32 / (cargo.capacity + 1) < std::max(new_usg, new_plan) * 32 / (new_cap + 1)) {
		cargo.cargo = new_cargo;
		cargo.capacity = new_cap;
		cargo.usage = new_usg;
		cargo.planned = new_plan;
		cargo.time = time;
	}
	if (new_shared) cargo.shared = true;
}

void LinkGraphOverlay::RefreshDrawCache()
{
	for (StationSupplyList::iterator i(this->cached_stations.begin()); i != this->cached_stations.end(); ++i) {
		const Station *st = Station::GetIfValid(i->id);
		if (st == nullptr) continue;

		i->pt = this->GetStationMiddle(st);
	}
	for (LinkList::iterator i(this->cached_links.begin()); i != this->cached_links.end(); ++i) {
		const Station *sta = Station::GetIfValid(i->from_id);
		if (sta == nullptr) continue;
		const Station *stb = Station::GetIfValid(i->to_id);
		if (stb == nullptr) continue;

		i->from_pt = this->GetStationMiddle(sta);
		i->to_pt = this->GetStationMiddle(stb);
	}
}

/**
 * Prepare to draw the linkgraph overlay or some part of it.
 */
void LinkGraphOverlay::PrepareDraw()
{
	if (this->dirty) {
		this->RebuildCache();
	}
	if (this->last_update_number != GetWindowUpdateNumber()) {
		this->last_update_number = GetWindowUpdateNumber();
		this->RefreshDrawCache();
	}
}

/**
 * Draw the linkgraph overlay or some part of it, in the area given.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::Draw(const DrawPixelInfo *dpi) const
{
	this->DrawLinks(dpi);
	this->DrawStationDots(dpi);
}

/**
 * Draw the cached links or part of them into the given area.
 * @param dpi Area to be drawn to.
 */
void LinkGraphOverlay::DrawLinks(const DrawPixelInfo *dpi) const
{
	int width = ScaleGUITrad(this->scale);
	for (const auto &i : this->cached_links) {
		if (!this->IsLinkVisible(i.from_pt, i.to_pt, dpi, width + 2)) continue;
		if (!Station::IsValidID(i.from_id)) continue;
		if (!Station::IsValidID(i.to_id)) continue;
		this->DrawContent(dpi, i.from_pt, i.to_pt, i.prop);
	}
}

/**
 * Draw one specific link.
 * @param pta Source of the link.
 * @param ptb Destination of the link.
 * @param cargo Properties of the link.
 */
void LinkGraphOverlay::DrawContent(const DrawPixelInfo *dpi, Point pta, Point ptb, const LinkProperties &cargo) const
{
	uint usage_or_plan = std::min(cargo.capacity * 2 + 1, cargo.Usage());
	int colour = LinkGraphOverlay::LINK_COLOURS[_settings_client.gui.linkgraph_colours][usage_or_plan * lengthof(LinkGraphOverlay::LINK_COLOURS[0]) / (cargo.capacity * 2 + 2)];
	int width = ScaleGUITrad(this->scale);
	int dash = cargo.shared ? width * 4 : 0;

	/* Move line a bit 90° against its dominant direction to prevent it from
	 * being hidden below the grey line. */
	int side = _settings_game.vehicle.road_side ? 1 : -1;
	if (abs(pta.x - ptb.x) < abs(pta.y - ptb.y)) {
		int offset_x = (pta.y > ptb.y ? 1 : -1) * side * width;
		GfxDrawLine(dpi, pta.x + offset_x, pta.y, ptb.x + offset_x, ptb.y, colour, width, dash);
	} else {
		int offset_y = (pta.x < ptb.x ? 1 : -1) * side * width;
		GfxDrawLine(dpi, pta.x, pta.y + offset_y, ptb.x, ptb.y + offset_y, colour, width, dash);
	}

	GfxDrawLine(dpi, pta.x, pta.y, ptb.x, ptb.y, _colour_gradient[COLOUR_GREY][1], width);
}

/**
 * Draw dots for stations into the smallmap. The dots' sizes are determined by the amount of
 * cargo produced there, their colours by the type of cargo produced.
 */
void LinkGraphOverlay::DrawStationDots(const DrawPixelInfo *dpi) const
{
	int width = ScaleGUITrad(this->scale);
	for (const auto &i : this->cached_stations) {
		const Point &pt = i.pt;
		if (!this->IsPointVisible(pt, dpi, 3 * width)) continue;

		const Station *st = Station::GetIfValid(i.id);
		if (st == nullptr) continue;

		uint r = width * 2 + width * 2 * std::min<uint>(200, i.quantity) / 200;

		LinkGraphOverlay::DrawVertex(dpi, pt.x, pt.y, r,
				_colour_gradient[st->owner != OWNER_NONE ?
						(Colours)Company::Get(st->owner)->colour : COLOUR_GREY][5],
				_colour_gradient[COLOUR_GREY][1]);
	}
}

/**
 * Draw a square symbolizing a producer of cargo.
 * @param x X coordinate of the middle of the vertex.
 * @param y Y coordinate of the middle of the vertex.
 * @param size Y and y extend of the vertex.
 * @param colour Colour with which the vertex will be filled.
 * @param border_colour Colour for the border of the vertex.
 */
/* static */ void LinkGraphOverlay::DrawVertex(const DrawPixelInfo *dpi, int x, int y, int size, int colour, int border_colour)
{
	size--;
	int w1 = size / 2;
	int w2 = size / 2 + size % 2;

	GfxFillRect(dpi, x - w1, y - w1, x + w2, y + w2, colour);

	w1++;
	w2++;
	GfxDrawLine(dpi, x - w1, y - w1, x + w2, y - w1, border_colour);
	GfxDrawLine(dpi, x - w1, y + w2, x + w2, y + w2, border_colour);
	GfxDrawLine(dpi, x - w1, y - w1, x - w1, y + w2, border_colour);
	GfxDrawLine(dpi, x + w2, y - w1, x + w2, y + w2, border_colour);
}

bool LinkGraphOverlay::ShowTooltip(Point pt, TooltipCloseCondition close_cond)
{
	for (LinkList::const_reverse_iterator i(this->cached_links.rbegin()); i != this->cached_links.rend(); ++i) {
		if (!Station::IsValidID(i->from_id)) continue;
		if (!Station::IsValidID(i->to_id)) continue;

		Point pta = i->from_pt;
		Point ptb = i->to_pt;

		/* Check the distance from the cursor to the line defined by the two stations. */
		auto check_distance = [&]() -> bool {
			int64 a = ((ptb.x - pta.x) * (pta.y - pt.y) - (pta.x - pt.x) * (ptb.y - pta.y));
			int64 b = ((ptb.x - pta.x) * (ptb.x - pta.x) + (ptb.y - pta.y) * (ptb.y - pta.y));
			if (b == 0) return false;
			return ((a * a) / b) <= 16;
		};
		const auto &link = i->prop;
		if ((link.Usage() > 0 || (_ctrl_pressed && link.capacity > 0)) &&
				pt.x + 2 >= std::min(pta.x, ptb.x) &&
				pt.x - 2 <= std::max(pta.x, ptb.x) &&
				pt.y + 2 >= std::min(pta.y, ptb.y) &&
				pt.y - 2 <= std::max(pta.y, ptb.y) &&
				check_distance()) {

			static char buf[1024];
			char *buf_end = buf;
			buf[0] = 0;

			auto add_travel_time = [&](uint32 time) {
				if (time > 0) {
					if (_settings_time.time_in_minutes) {
						SetDParam(0, STR_TIMETABLE_MINUTES);
						SetDParam(1, time / _settings_time.ticks_per_minute);
						buf_end = GetString(buf_end, STR_LINKGRAPH_STATS_TOOLTIP_TIME_EXTENSION_GENERAL, lastof(buf));
					} else {
						SetDParam(0, time / (DAY_TICKS * _settings_game.economy.day_length_factor));
						buf_end = GetString(buf_end, STR_LINKGRAPH_STATS_TOOLTIP_TIME_EXTENSION, lastof(buf));
					}
				}
			};

			if (_ctrl_pressed) {
				SetDParam(0, link.cargo);
				SetDParam(1, link.capacity);
				buf_end = GetString(buf_end, STR_LINKGRAPH_STATS_TOOLTIP_CAPACITY, lastof(buf));
				add_travel_time(link.time);
			}

			/* Fill buf with more information if this is a bidirectional link. */
			uint32 back_time = 0;
			for (LinkList::const_reverse_iterator j = std::next(i); j != this->cached_links.rend(); ++j) {
				if (j->from_id == i->to_id && j->to_id == i->from_id) {
					back_time = j->prop.time;
					if (j->prop.Usage() > 0 || (_ctrl_pressed && j->prop.capacity > 0)) {
						if (_ctrl_pressed) buf_end = strecat(buf_end, "\n", lastof(buf));
						SetDParam(0, j->prop.cargo);
						SetDParam(1, j->prop.Usage());
						SetDParam(2, j->prop.Usage() * 100 / (j->prop.capacity + 1));
						buf_end = GetString(buf_end, STR_LINKGRAPH_STATS_TOOLTIP_RETURN_EXTENSION, lastof(buf));
						if (_ctrl_pressed) {
							SetDParam(0, j->prop.cargo);
							SetDParam(1, j->prop.capacity);
							buf_end = GetString(buf_end, STR_LINKGRAPH_STATS_TOOLTIP_CAPACITY, lastof(buf));
							add_travel_time(back_time);
						}
					}
					break;
				}
			}
			if (!_ctrl_pressed) {
				/* Add information about the travel time if known. */
				add_travel_time(link.time ? (back_time ? ((link.time + back_time) / 2) : link.time) : back_time);
			}

			SetDParam(0, link.cargo);
			SetDParam(1, link.Usage());
			SetDParam(2, i->from_id);
			SetDParam(3, i->to_id);
			SetDParam(4, link.Usage() * 100 / (link.capacity + 1));
			SetDParamStr(5, buf);
			GuiShowTooltips(this->window, STR_LINKGRAPH_STATS_TOOLTIP, 0, nullptr, close_cond);
			return true;
		}
	}
	GuiShowTooltips(this->window, STR_NULL, 0, nullptr, close_cond);
	return false;
}

/**
 * Determine the middle of a station in the current window.
 * @param st The station we're looking for.
 * @return Middle point of the station in the current window.
 */
Point LinkGraphOverlay::GetStationMiddle(const Station *st) const
{
	if (this->window->viewport != nullptr) {
		return GetViewportStationMiddle(this->window->viewport, st);
	} else {
		/* assume this is a smallmap */
		return static_cast<const SmallMapWindow *>(this->window)->GetStationMiddle(st);
	}
}

/**
 * Set a new cargo mask and rebuild the cache.
 * @param cargo_mask New cargo mask.
 */
void LinkGraphOverlay::SetCargoMask(CargoTypes cargo_mask)
{
	this->cargo_mask = cargo_mask;
	this->RebuildCache();
	this->window->GetWidget<NWidgetBase>(this->widget_id)->SetDirty(this->window);
}

/**
 * Set a new company mask and rebuild the cache.
 * @param company_mask New company mask.
 */
void LinkGraphOverlay::SetCompanyMask(CompanyMask company_mask)
{
	this->company_mask = company_mask;
	this->RebuildCache();
	this->window->GetWidget<NWidgetBase>(this->widget_id)->SetDirty(this->window);
}

/** Make a number of rows with buttons for each company for the linkgraph legend window. */
NWidgetBase *MakeCompanyButtonRowsLinkGraphGUI(int *biggest_index)
{
	return MakeCompanyButtonRows(biggest_index, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST, COLOUR_GREY, 3, STR_NULL);
}

NWidgetBase *MakeSaturationLegendLinkGraphGUI(int *biggest_index)
{
	NWidgetVertical *panel = new NWidgetVertical(NC_EQUALSIZE);
	for (uint i = 0; i < lengthof(LinkGraphOverlay::LINK_COLOURS[0]); ++i) {
		NWidgetBackground * wid = new NWidgetBackground(WWT_PANEL, COLOUR_DARK_GREEN, i + WID_LGL_SATURATION_FIRST);
		wid->SetMinimalSize(50, 0);
		wid->SetMinimalTextLines(1, 0, FS_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		panel->Add(wid);
	}
	*biggest_index = WID_LGL_SATURATION_LAST;
	return panel;
}

NWidgetBase *MakeCargoesLegendLinkGraphGUI(int *biggest_index)
{
	uint num_cargo = static_cast<uint>(_sorted_cargo_specs.size());
	static const uint ENTRIES_PER_COL = 5;
	NWidgetHorizontal *panel = new NWidgetHorizontal(NC_EQUALSIZE);
	NWidgetVertical *col = nullptr;

	for (uint i = 0; i < num_cargo; ++i) {
		if (i % ENTRIES_PER_COL == 0) {
			if (col != nullptr) panel->Add(col);
			col = new NWidgetVertical(NC_EQUALSIZE);
		}
		NWidgetBackground * wid = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, i + WID_LGL_CARGO_FIRST);
		wid->SetMinimalSize(25, 0);
		wid->SetMinimalTextLines(1, 0, FS_SMALL);
		wid->SetFill(1, 1);
		wid->SetResize(0, 0);
		col->Add(wid);
	}
	/* Fill up last row */
	for (uint i = num_cargo; i < Ceil(num_cargo, ENTRIES_PER_COL); ++i) {
		NWidgetSpacer *spc = new NWidgetSpacer(25, 0);
		spc->SetMinimalTextLines(1, 0, FS_SMALL);
		spc->SetFill(1, 1);
		spc->SetResize(0, 0);
		col->Add(spc);
	}
	panel->Add(col);
	*biggest_index = WID_LGL_CARGO_LAST;
	return panel;
}


static const NWidgetPart _nested_linkgraph_legend_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_LGL_CAPTION), SetDataTip(STR_LINKGRAPH_LEGEND_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect), SetPIP(0, WidgetDimensions::unscaled.framerect.Horizontal(), 0),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_SATURATION),
				NWidgetFunction(MakeSaturationLegendLinkGraphGUI),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_COMPANIES),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCompanyButtonRowsLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_COMPANIES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_LGL_CARGOES),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidgetFunction(MakeCargoesLegendLinkGraphGUI),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_ALL), SetDataTip(STR_LINKGRAPH_LEGEND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_LGL_CARGOES_NONE), SetDataTip(STR_LINKGRAPH_LEGEND_NONE, STR_NULL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer()
};

static_assert(WID_LGL_SATURATION_LAST - WID_LGL_SATURATION_FIRST ==
		lengthof(LinkGraphOverlay::LINK_COLOURS[0]) - 1);

static WindowDesc _linkgraph_legend_desc(
	WDP_AUTO, "toolbar_linkgraph", 0, 0,
	WC_LINKGRAPH_LEGEND, WC_NONE,
	0,
	_nested_linkgraph_legend_widgets, lengthof(_nested_linkgraph_legend_widgets)
);

/**
 * Open a link graph legend window.
 */
void ShowLinkGraphLegend()
{
	AllocateWindowDescFront<LinkGraphLegendWindow>(&_linkgraph_legend_desc, 0);
}

LinkGraphLegendWindow::LinkGraphLegendWindow(WindowDesc *desc, int window_number) : Window(desc)
{
	this->num_cargo = _sorted_cargo_specs.size();

	this->InitNested(window_number);
	this->InvalidateData(0);
	this->SetOverlay(GetMainWindow()->viewport->overlay);
}

/**
 * Set the overlay belonging to this menu and import its company/cargo settings.
 * @param overlay New overlay for this menu.
 */
void LinkGraphLegendWindow::SetOverlay(LinkGraphOverlay *overlay) {
	this->overlay = overlay;
	CompanyMask companies = this->overlay->GetCompanyMask();
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (!this->IsWidgetDisabled(WID_LGL_COMPANY_FIRST + c)) {
			this->SetWidgetLoweredState(WID_LGL_COMPANY_FIRST + c, HasBit(companies, c));
		}
	}
	CargoTypes cargoes = this->overlay->GetCargoMask();
	for (uint c = 0; c < this->num_cargo; c++) {
		this->SetWidgetLoweredState(WID_LGL_CARGO_FIRST + c, HasBit(cargoes, _sorted_cargo_specs[c]->Index()));
	}
}

void LinkGraphLegendWindow::UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
{
	if (IsInsideMM(widget, WID_LGL_SATURATION_FIRST, WID_LGL_SATURATION_LAST + 1)) {
		StringID str = STR_NULL;
		if (widget == WID_LGL_SATURATION_FIRST) {
			str = STR_LINKGRAPH_LEGEND_UNUSED;
		} else if (widget == WID_LGL_SATURATION_LAST) {
			str = STR_LINKGRAPH_LEGEND_OVERLOADED;
		} else if (widget == (WID_LGL_SATURATION_LAST + WID_LGL_SATURATION_FIRST) / 2) {
			str = STR_LINKGRAPH_LEGEND_SATURATED;
		}
		if (str != STR_NULL) {
			Dimension dim = GetStringBoundingBox(str, FS_SMALL);
			dim.width += padding.width;
			dim.height += padding.height;
			*size = maxdim(*size, dim);
		}
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		Dimension dim = GetStringBoundingBox(cargo->abbrev, FS_SMALL);
		dim.width += padding.width;
		dim.height += padding.height;
		*size = maxdim(*size, dim);
	}
}

void LinkGraphLegendWindow::DrawWidget(const Rect &r, int widget) const
{
	Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
	if (this->IsWidgetLowered(widget)) br = br.Translate(WidgetDimensions::scaled.pressed, WidgetDimensions::scaled.pressed);
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return;
		CompanyID cid = (CompanyID)(widget - WID_LGL_COMPANY_FIRST);
		Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, CenterBounds(br.left, br.right, sprite_size.width), CenterBounds(br.top, br.bottom, sprite_size.height));
	}
	if (IsInsideMM(widget, WID_LGL_SATURATION_FIRST, WID_LGL_SATURATION_LAST + 1)) {
		uint8 colour = LinkGraphOverlay::LINK_COLOURS[_settings_client.gui.linkgraph_colours][widget - WID_LGL_SATURATION_FIRST];
		GfxFillRect(br, colour);
		StringID str = STR_NULL;
		if (widget == WID_LGL_SATURATION_FIRST) {
			str = STR_LINKGRAPH_LEGEND_UNUSED;
		} else if (widget == WID_LGL_SATURATION_LAST) {
			str = STR_LINKGRAPH_LEGEND_OVERLOADED;
		} else if (widget == (WID_LGL_SATURATION_LAST + WID_LGL_SATURATION_FIRST) / 2) {
			str = STR_LINKGRAPH_LEGEND_SATURATED;
		}
		if (str != STR_NULL) {
			DrawString(br.left, br.right, CenterBounds(br.top, br.bottom, FONT_HEIGHT_SMALL), str, GetContrastColour(colour) | TC_FORCED, SA_HOR_CENTER, false, FS_SMALL);
		}
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		GfxFillRect(br, cargo->legend_colour);
		DrawString(br.left, br.right, CenterBounds(br.top, br.bottom, FONT_HEIGHT_SMALL), cargo->abbrev, GetContrastColour(cargo->legend_colour, 73), SA_HOR_CENTER, false, FS_SMALL);
	}
}

bool LinkGraphLegendWindow::OnTooltip(Point pt, int widget, TooltipCloseCondition close_cond)
{
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) {
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_SELECT_COMPANIES, 0, nullptr, close_cond);
		} else {
			uint64 params[2];
			CompanyID cid = (CompanyID)(widget - WID_LGL_COMPANY_FIRST);
			params[0] = STR_LINKGRAPH_LEGEND_SELECT_COMPANIES;
			params[1] = cid;
			GuiShowTooltips(this, STR_LINKGRAPH_LEGEND_COMPANY_TOOLTIP, 2, params, close_cond);
		}
		return true;
	}
	if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		const CargoSpec *cargo = _sorted_cargo_specs[widget - WID_LGL_CARGO_FIRST];
		GuiShowTooltips(this, cargo->name, 0, nullptr, close_cond);
		return true;
	}
	return false;
}

/**
 * Update the overlay with the new company selection.
 */
void LinkGraphLegendWindow::UpdateOverlayCompanies()
{
	uint32 mask = 0;
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (this->IsWidgetDisabled(c + WID_LGL_COMPANY_FIRST)) continue;
		if (!this->IsWidgetLowered(c + WID_LGL_COMPANY_FIRST)) continue;
		SetBit(mask, c);
	}
	this->overlay->SetCompanyMask(mask);
}

/**
 * Update the overlay with the new cargo selection.
 */
void LinkGraphLegendWindow::UpdateOverlayCargoes()
{
	CargoTypes mask = 0;
	for (uint c = 0; c < num_cargo; c++) {
		if (!this->IsWidgetLowered(c + WID_LGL_CARGO_FIRST)) continue;
		SetBit(mask, _sorted_cargo_specs[c]->Index());
	}
	this->overlay->SetCargoMask(mask);
}

void LinkGraphLegendWindow::OnClick(Point pt, int widget, int click_count)
{
	/* Check which button is clicked */
	if (IsInsideMM(widget, WID_LGL_COMPANY_FIRST, WID_LGL_COMPANY_LAST + 1)) {
		if (!this->IsWidgetDisabled(widget)) {
			this->ToggleWidgetLoweredState(widget);
			this->UpdateOverlayCompanies();
		}
	} else if (widget == WID_LGL_COMPANIES_ALL || widget == WID_LGL_COMPANIES_NONE) {
		for (uint c = 0; c < MAX_COMPANIES; c++) {
			if (this->IsWidgetDisabled(c + WID_LGL_COMPANY_FIRST)) continue;
			this->SetWidgetLoweredState(WID_LGL_COMPANY_FIRST + c, widget == WID_LGL_COMPANIES_ALL);
		}
		this->UpdateOverlayCompanies();
		this->SetDirty();
	} else if (IsInsideMM(widget, WID_LGL_CARGO_FIRST, WID_LGL_CARGO_LAST + 1)) {
		this->ToggleWidgetLoweredState(widget);
		this->UpdateOverlayCargoes();
	} else if (widget == WID_LGL_CARGOES_ALL || widget == WID_LGL_CARGOES_NONE) {
		for (uint c = 0; c < this->num_cargo; c++) {
			this->SetWidgetLoweredState(WID_LGL_CARGO_FIRST + c, widget == WID_LGL_CARGOES_ALL);
		}
		this->UpdateOverlayCargoes();
	}
	this->SetDirty();
}

/**
 * Invalidate the data of this window if the cargoes or companies have changed.
 * @param data ignored
 * @param gui_scope ignored
 */
void LinkGraphLegendWindow::OnInvalidateData(int data, bool gui_scope)
{
	if (this->num_cargo != _sorted_cargo_specs.size()) {
		delete this;
		return;
	}

	/* Disable the companies who are not active */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		this->SetWidgetDisabledState(i + WID_LGL_COMPANY_FIRST, !Company::IsValidID(i));
	}
}
