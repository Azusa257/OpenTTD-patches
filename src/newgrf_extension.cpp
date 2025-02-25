/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_extension.cpp NewGRF extension support. */

#include "stdafx.h"
#include "newgrf.h"
#include "newgrf_extension.h"
#include "table/sprites.h"

#include "safeguards.h"

/** Action14 feature list */
extern const GRFFeatureInfo _grf_feature_list[] = {
	GRFFeatureInfo("feature_test", 2),
	GRFFeatureInfo("property_mapping", 3),
	GRFFeatureInfo("variable_mapping", 3),
	GRFFeatureInfo("feature_id_mapping", 2),
	GRFFeatureInfo("action5_type_id_mapping", 2),
	GRFFeatureInfo("action0_station_prop1B", 1),
	GRFFeatureInfo("action0_station_disallowed_bridge_pillars", 1),
	GRFFeatureInfo("varaction2_station_var42", 1),
	GRFFeatureInfo("varaction2_station_station_nearby_info_v2", 1),
	GRFFeatureInfo("more_bridge_types", 1),
	GRFFeatureInfo("action0_bridge_prop14", 1),
	GRFFeatureInfo("action0_bridge_pillar_flags", 1),
	GRFFeatureInfo("action0_bridge_availability_flags", 1),
	GRFFeatureInfo("action5_programmable_signals", 1),
	GRFFeatureInfo("action5_no_entry_signals", 1),
	GRFFeatureInfo("action5_misc_gui", 1),
	GRFFeatureInfo("action5_road_waypoints", 1),
	GRFFeatureInfo("action0_railtype_programmable_signals", 1),
	GRFFeatureInfo("action0_railtype_no_entry_signals", 1),
	GRFFeatureInfo("action0_railtype_restricted_signals", 2),
	GRFFeatureInfo("action0_railtype_disable_realistic_braking", 1),
	GRFFeatureInfo("action0_railtype_recolour", 1),
	GRFFeatureInfo("action0_railtype_extra_aspects", 1),
	GRFFeatureInfo("action0_roadtype_extra_flags", 2),
	GRFFeatureInfo("action0_roadtype_collision_mode", 1),
	GRFFeatureInfo("varaction2_railtype_signal_context", 1),
	GRFFeatureInfo("varaction2_railtype_signal_side", 1),
	GRFFeatureInfo("varaction2_railtype_signal_vertical_clearance", 1),
	GRFFeatureInfo("action0_global_extra_station_names", 2),
	GRFFeatureInfo("action0_global_default_object_generate_amount", 1),
	GRFFeatureInfo("action0_global_allow_rocks_in_desert", 1),
	GRFFeatureInfo("action0_signals_programmable_signals", 1),
	GRFFeatureInfo("action0_signals_no_entry_signals", 1),
	GRFFeatureInfo("action0_signals_restricted_signals", 2),
	GRFFeatureInfo("action0_signals_recolour", 1),
	GRFFeatureInfo("action0_signals_extra_aspects", 1),
	GRFFeatureInfo("action0_signals_style", 1),
	GRFFeatureInfo("varaction2_signals_signal_context", 1),
	GRFFeatureInfo("varaction2_signals_signal_side", 1),
	GRFFeatureInfo("varaction2_signals_signal_vertical_clearance", 1),
	GRFFeatureInfo("action3_signals_custom_signal_sprites", 1),
	GRFFeatureInfo("action0_object_use_land_ground", 1),
	GRFFeatureInfo("action0_object_edge_foundation_mode", 2),
	GRFFeatureInfo("action0_object_flood_resistant", 1),
	GRFFeatureInfo("action0_object_viewport_map_tile_type", 1),
	GRFFeatureInfo("road_stops", 9, GFTOF_ROAD_STOPS),
	GRFFeatureInfo("new_landscape", 2),
	GRFFeatureInfo("more_objects_per_grf", 1),
	GRFFeatureInfo("more_action2_ids", 1, GFTOF_MORE_ACTION2_IDS),
	GRFFeatureInfo("town_feature", 1),
	GRFFeatureInfo("town_uncapped_variables", 1),
	GRFFeatureInfo("town_zone_callback", 1, GFTOF_TOWN_ZONE_CALLBACK),
	GRFFeatureInfo("varaction2_towns_town_xy", 1),
	GRFFeatureInfo("more_varaction2_types", 1, GFTOF_MORE_VARACTION2_TYPES),
	GRFFeatureInfo("multi_part_ships", 2, GFTOF_MULTI_PART_SHIPS),
	GRFFeatureInfo("more_stations_per_grf", 1),
	GRFFeatureInfo(),
};

/** Action14 remappable feature list */
extern const GRFFeatureMapDefinition _grf_remappable_features[] = {
	GRFFeatureMapDefinition(GSF_ROADSTOPS, "road_stops"),
	GRFFeatureMapDefinition(GSF_NEWLANDSCAPE, "new_landscape"),
	GRFFeatureMapDefinition(GSF_FAKE_TOWNS, "town"),
	GRFFeatureMapDefinition(),
};


/** Action14 Action0 remappable property list */
extern const GRFPropertyMapDefinition _grf_action0_remappable_properties[] = {
	GRFPropertyMapDefinition(GSF_INVALID, A0RPI_ID_EXTENSION, "id_extension"),
	GRFPropertyMapDefinition(GSF_STATIONS, A0RPI_STATION_MIN_BRIDGE_HEIGHT, "station_min_bridge_height"),
	GRFPropertyMapDefinition(GSF_STATIONS, A0RPI_STATION_DISALLOWED_BRIDGE_PILLARS, "station_disallowed_bridge_pillars"),
	GRFPropertyMapDefinition(GSF_BRIDGES, A0RPI_BRIDGE_MENU_ICON, "bridge_menu_icon"),
	GRFPropertyMapDefinition(GSF_BRIDGES, A0RPI_BRIDGE_PILLAR_FLAGS, "bridge_pillar_flags"),
	GRFPropertyMapDefinition(GSF_BRIDGES, A0RPI_BRIDGE_AVAILABILITY_FLAGS, "bridge_availability_flags"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_ENABLE_PROGRAMMABLE_SIGNALS, "railtype_enable_programmable_signals"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_ENABLE_NO_ENTRY_SIGNALS, "railtype_enable_no_entry_signals"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_ENABLE_RESTRICTED_SIGNALS, "railtype_enable_restricted_signals"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_DISABLE_REALISTIC_BRAKING, "railtype_disable_realistic_braking"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_ENABLE_SIGNAL_RECOLOUR, "railtype_enable_signal_recolour"),
	GRFPropertyMapDefinition(GSF_RAILTYPES, A0RPI_RAILTYPE_EXTRA_ASPECTS, "railtype_extra_aspects"),
	GRFPropertyMapDefinition(GSF_ROADTYPES, A0RPI_ROADTYPE_EXTRA_FLAGS, "roadtype_extra_flags"),
	GRFPropertyMapDefinition(GSF_ROADTYPES, A0RPI_ROADTYPE_COLLISION_MODE, "roadtype_collision_mode"),
	GRFPropertyMapDefinition(GSF_TRAMTYPES, A0RPI_ROADTYPE_EXTRA_FLAGS, "roadtype_extra_flags"),
	GRFPropertyMapDefinition(GSF_TRAMTYPES, A0RPI_ROADTYPE_COLLISION_MODE, "roadtype_collision_mode"),
	GRFPropertyMapDefinition(GSF_GLOBALVAR, A0RPI_GLOBALVAR_EXTRA_STATION_NAMES, "global_extra_station_names"),
	GRFPropertyMapDefinition(GSF_GLOBALVAR, A0RPI_GLOBALVAR_EXTRA_STATION_NAMES_PROBABILITY, "global_extra_station_names_probability"),
	GRFPropertyMapDefinition(GSF_GLOBALVAR, A0RPI_GLOBALVAR_LIGHTHOUSE_GENERATE_AMOUNT, "global_lighthouse_generate_amount"),
	GRFPropertyMapDefinition(GSF_GLOBALVAR, A0RPI_GLOBALVAR_TRANSMITTER_GENERATE_AMOUNT, "global_transmitter_generate_amount"),
	GRFPropertyMapDefinition(GSF_GLOBALVAR, A0RPI_GLOBALVAR_ALLOW_ROCKS_DESERT, "global_allow_rocks_in_desert"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_ENABLE_PROGRAMMABLE_SIGNALS, "signals_enable_programmable_signals"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_ENABLE_NO_ENTRY_SIGNALS, "signals_enable_no_entry_signals"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_ENABLE_RESTRICTED_SIGNALS, "signals_enable_restricted_signals"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_ENABLE_SIGNAL_RECOLOUR, "signals_enable_signal_recolour"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_EXTRA_ASPECTS, "signals_extra_aspects"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_NO_DEFAULT_STYLE, "signals_no_default_style"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_DEFINE_STYLE, "signals_define_style"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_NAME, "signals_style_name"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_NO_ASPECT_INCREASE, "signals_style_no_aspect_increase"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_ALWAYS_RESERVE_THROUGH, "signals_style_always_reserve_through"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_LOOKAHEAD_EXTRA_ASPECTS, "signals_style_lookahead_extra_aspects"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_LOOKAHEAD_SINGLE_SIGNAL_ONLY, "signals_style_lookahead_single_signal_only"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_SEMAPHORE_ENABLED, "signals_style_semaphore_enabled"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_ELECTRIC_ENABLED, "signals_style_electric_enabled"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_OPPOSITE_SIDE, "signals_style_opposite_side"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_COMBINED_NORMAL_SHUNT, "signals_style_combined_normal_shunt"),
	GRFPropertyMapDefinition(GSF_SIGNALS, A0RPI_SIGNALS_STYLE_REALISTIC_BRAKING_ONLY, "signals_style_realistic_braking_only"),
	GRFPropertyMapDefinition(GSF_OBJECTS, A0RPI_OBJECT_USE_LAND_GROUND, "object_use_land_ground"),
	GRFPropertyMapDefinition(GSF_OBJECTS, A0RPI_OBJECT_EDGE_FOUNDATION_MODE, "object_edge_foundation_mode"),
	GRFPropertyMapDefinition(GSF_OBJECTS, A0RPI_OBJECT_FLOOD_RESISTANT, "object_flood_resistant"),
	GRFPropertyMapDefinition(GSF_OBJECTS, A0RPI_OBJECT_VIEWPORT_MAP_TYPE, "object_viewport_map_tile_type"),
	GRFPropertyMapDefinition(GSF_OBJECTS, A0RPI_OBJECT_VIEWPORT_MAP_SUBTYPE, "object_viewport_map_tile_subtype"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_CLASS_ID, "roadstop_class_id"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_STOP_TYPE, "roadstop_stop_type"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_STOP_NAME, "roadstop_stop_name"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_CLASS_NAME, "roadstop_class_name"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_DRAW_MODE, "roadstop_draw_mode"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_TRIGGER_CARGOES, "roadstop_random_trigger_cargoes"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_ANIMATION_INFO, "roadstop_animation_info"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_ANIMATION_SPEED, "roadstop_animation_speed"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_ANIMATION_TRIGGERS, "roadstop_animation_triggers"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_CALLBACK_MASK, "roadstop_callback_mask"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_GENERAL_FLAGS, "roadstop_general_flags"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_MIN_BRIDGE_HEIGHT, "roadstop_min_bridge_height"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_DISALLOWED_BRIDGE_PILLARS, "roadstop_disallowed_bridge_pillars"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_COST_MULTIPLIERS, "roadstop_cost_multipliers"),
	GRFPropertyMapDefinition(GSF_ROADSTOPS, A0RPI_ROADSTOP_HEIGHT, "roadstop_height"),
	GRFPropertyMapDefinition(GSF_NEWLANDSCAPE, A0RPI_NEWLANDSCAPE_ENABLE_RECOLOUR, "newlandscape_enable_recolour"),
	GRFPropertyMapDefinition(GSF_NEWLANDSCAPE, A0RPI_NEWLANDSCAPE_ENABLE_DRAW_SNOWY_ROCKS, "newlandscape_enable_draw_snowy_rocks"),
	GRFPropertyMapDefinition(),
};

/** Action14 Action2 remappable variable list */
extern const GRFVariableMapDefinition _grf_action2_remappable_variables[] = {
	GRFVariableMapDefinition(GSF_STATIONS, A2VRI_STATION_INFO_NEARBY_TILES_V2, "station_station_info_nearby_tiles_v2"),
	GRFVariableMapDefinition(GSF_OBJECTS, A2VRI_OBJECT_FOUNDATION_SLOPE, "object_foundation_tile_slope"),
	GRFVariableMapDefinition(GSF_OBJECTS, A2VRI_OBJECT_FOUNDATION_SLOPE_CHANGE, "object_foundation_change_tile_slope"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x40, "roadstop_view"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x41, "roadstop_type"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x42, "roadstop_terrain_type"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x43, "roadstop_road_type"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x44, "roadstop_tram_type"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x45, "roadstop_town_zone"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x46, "roadstop_town_distance_squared"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x47, "roadstop_company_info"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x49, "roadstop_animation_frame"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x50, "roadstop_misc_info"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x66, "roadstop_animation_frame_nearby_tiles"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x67, "roadstop_land_info_nearby_tiles"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x68, "roadstop_road_stop_info_nearby_tiles"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x6A, "roadstop_road_stop_grfid_nearby_tiles"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, 0x6B, "roadstop_road_info_nearby_tiles"),
	GRFVariableMapDefinition(GSF_ROADSTOPS, A2VRI_ROADSTOP_INFO_NEARBY_TILES_V2, "roadstop_road_stop_info_nearby_tiles_v2"),
	GRFVariableMapDefinition(GSF_RAILTYPES, A2VRI_RAILTYPE_SIGNAL_RESTRICTION_INFO, "railtype_signal_restriction_info"),
	GRFVariableMapDefinition(GSF_RAILTYPES, A2VRI_RAILTYPE_SIGNAL_CONTEXT, "railtype_signal_context"),
	GRFVariableMapDefinition(GSF_RAILTYPES, A2VRI_RAILTYPE_SIGNAL_SIDE, "railtype_signal_side"),
	GRFVariableMapDefinition(GSF_RAILTYPES, A2VRI_RAILTYPE_SIGNAL_VERTICAL_CLEARANCE, "railtype_signal_vertical_clearance"),
	GRFVariableMapDefinition(GSF_SIGNALS, A2VRI_SIGNALS_SIGNAL_RESTRICTION_INFO, "signals_signal_restriction_info"),
	GRFVariableMapDefinition(GSF_SIGNALS, A2VRI_SIGNALS_SIGNAL_CONTEXT, "signals_signal_context"),
	GRFVariableMapDefinition(GSF_SIGNALS, A2VRI_SIGNALS_SIGNAL_STYLE, "signals_signal_style"),
	GRFVariableMapDefinition(GSF_SIGNALS, A2VRI_SIGNALS_SIGNAL_SIDE, "signals_signal_side"),
	GRFVariableMapDefinition(GSF_SIGNALS, A2VRI_SIGNALS_SIGNAL_VERTICAL_CLEARANCE, "signals_signal_vertical_clearance"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_HOUSE_COUNT, "town_house_count"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_POPULATION, "town_population"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_ZONE_0, "town_zone_0_radius_square"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_ZONE_1, "town_zone_1_radius_square"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_ZONE_2, "town_zone_2_radius_square"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_ZONE_3, "town_zone_3_radius_square"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_ZONE_4, "town_zone_4_radius_square"),
	GRFVariableMapDefinition(GSF_FAKE_TOWNS, A2VRI_TOWNS_XY, "town_xy"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x40, "newlandscape_terrain_type"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x41, "newlandscape_tile_slope"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x42, "newlandscape_tile_height"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x43, "newlandscape_tile_hash"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x44, "newlandscape_landscape_type"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x45, "newlandscape_ground_info"),
	GRFVariableMapDefinition(GSF_NEWLANDSCAPE, 0x60, "newlandscape_land_info_nearby_tiles"),
	GRFVariableMapDefinition(),
};

extern const GRFNameOnlyVariableMapDefinition _grf_action2_internal_variable_names[] = {
	GRFNameOnlyVariableMapDefinition(A2VRI_VEHICLE_CURRENT_SPEED_SCALED, "current speed scaled"),
	GRFNameOnlyVariableMapDefinition(A2VRI_ROADSTOP_INFO_NEARBY_TILES_EXT, "68 (extended)"),
	GRFNameOnlyVariableMapDefinition(),
};

/** Action14 Action5 remappable type list */
extern const Action5TypeRemapDefinition _grf_action5_remappable_types[] = {
	Action5TypeRemapDefinition("programmable_signals", A5BLOCK_ALLOW_OFFSET, SPR_PROGSIGNAL_BASE, 1, 32, "Programmable pre-signal graphics"),
	Action5TypeRemapDefinition("no_entry_signals", A5BLOCK_ALLOW_OFFSET, SPR_EXTRASIGNAL_BASE, 1, 16, "No-entry signal graphics"),
	Action5TypeRemapDefinition("misc_gui", A5BLOCK_ALLOW_OFFSET, SPR_MISC_GUI_BASE, 1, 1, "Miscellaneous GUI graphics"),
	Action5TypeRemapDefinition("road_waypoints", A5BLOCK_ALLOW_OFFSET, SPR_ROAD_WAYPOINTS_BASE, 1, 4, "Road waypoints"),
	Action5TypeRemapDefinition(),
};
