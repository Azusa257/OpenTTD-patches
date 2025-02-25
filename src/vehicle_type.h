/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_type.h Types related to vehicles. */

#ifndef VEHICLE_TYPE_H
#define VEHICLE_TYPE_H

#include "core/enum_type.hpp"

/** The type all our vehicle IDs have. */
typedef uint32 VehicleID;

static const int GROUND_ACCELERATION = 9800; ///< Acceleration due to gravity, 9.8 m/s^2

/** Available vehicle types. It needs to be 8bits, because we save and load it as such */
enum VehicleType : byte {
	VEH_BEGIN,

	VEH_TRAIN = VEH_BEGIN,        ///< %Train vehicle type.
	VEH_ROAD,                     ///< Road vehicle type.
	VEH_SHIP,                     ///< %Ship vehicle type.
	VEH_AIRCRAFT,                 ///< %Aircraft vehicle type.

	VEH_COMPANY_END,              ///< Last company-ownable type.

	VEH_EFFECT = VEH_COMPANY_END, ///< Effect vehicle type (smoke, explosions, sparks, bubbles)
	VEH_DISASTER,                 ///< Disaster vehicle type.

	VEH_END,
	VEH_INVALID = 0xFF,           ///< Non-existing type of vehicle.
};
DECLARE_POSTFIX_INCREMENT(VehicleType)
/** Helper information for extract tool. */
template <> struct EnumPropsT<VehicleType> : MakeEnumPropsT<VehicleType, byte, VEH_TRAIN, VEH_END, VEH_INVALID, 3> {};

struct Vehicle;
struct Train;
struct RoadVehicle;
struct Ship;
struct Aircraft;
struct EffectVehicle;
struct DisasterVehicle;

/** Base vehicle class. */
struct BaseVehicle
{
	VehicleType type; ///< Type of vehicle
};

static const VehicleID INVALID_VEHICLE = 0xFFFFF; ///< Constant representing a non-existing vehicle.

/** Pathfinding option states */
enum VehiclePathFinders {
	// Original PathFinder (OPF) used to be 0
	VPF_NPF  = 1, ///< New PathFinder
	VPF_YAPF = 2, ///< Yet Another PathFinder
};

/** Flags to add to p1 for goto depot commands. */
enum DepotCommand {
	DEPOT_SELL          = (1U << 25), ///< Go to depot and sell order
	DEPOT_CANCEL        = (1U << 26), ///< Cancel depot/service order
	DEPOT_SPECIFIC      = (1U << 27), ///< Send vehicle to specific depot
	DEPOT_SERVICE       = (1U << 28), ///< The vehicle will leave the depot right after arrival (service only)
	DEPOT_MASS_SEND     = (1U << 29), ///< Tells that it's a mass send to depot command (type in VLW flag)
	DEPOT_DONT_CANCEL   = (1U << 30), ///< Don't cancel current goto depot command if any
	DEPOT_LOCATE_HANGAR = (1U << 31), ///< Find another airport if the target one lacks a hangar
	DEPOT_COMMAND_MASK  = 0x7FU << 25,
};

static const uint MAX_LENGTH_VEHICLE_NAME_CHARS = 128; ///< The maximum length of a vehicle name in characters including '\0'

/** The length of a vehicle in tile units. */
static const uint VEHICLE_LENGTH = 8;

/**
 * The different types of breakdowns
 *
 * Aircraft have totally different breakdowns, so we use aliases to make things clearer
 */
enum BreakdownType {
	BREAKDOWN_CRITICAL  = 0, ///< Old style breakdown (black smoke)
	BREAKDOWN_EM_STOP   = 1, ///< Emergency stop
	BREAKDOWN_LOW_SPEED = 2, ///< Lower max speed
	BREAKDOWN_LOW_POWER = 3, ///< Power reduction
	BREAKDOWN_RV_CRASH  = 4, ///< Train hit road vehicle
	BREAKDOWN_BRAKE_OVERHEAT = 5, ///< Train brakes overheated due to excessive slope or speed change

	BREAKDOWN_AIRCRAFT_SPEED      = BREAKDOWN_CRITICAL,  ///< Lower speed until the next airport
	BREAKDOWN_AIRCRAFT_DEPOT      = BREAKDOWN_EM_STOP,   ///< We have to visit a depot at the next airport
	BREAKDOWN_AIRCRAFT_EM_LANDING = BREAKDOWN_LOW_SPEED, ///< Emergency landing at the closest airport (with hangar!) we can find
};

/** Vehicle acceleration models. */
enum AccelerationModel {
	AM_ORIGINAL,
	AM_REALISTIC,
};

/** Train braking models. */
enum TrainBrakingModel {
	TBM_ORIGINAL,
	TBM_REALISTIC,
};

/** Train realistic braking aspect limited mode. */
enum TrainRealisticBrakingAspectLimitedMode {
	TRBALM_OFF,
	TRBALM_ON,
};

/** Visualisation contexts of vehicles and engines. */
enum EngineImageType {
	EIT_ON_MAP     = 0x00,  ///< Vehicle drawn in viewport.
	EIT_IN_DEPOT   = 0x10,  ///< Vehicle drawn in depot.
	EIT_IN_DETAILS = 0x11,  ///< Vehicle drawn in vehicle details, refit window, ...
	EIT_IN_LIST    = 0x12,  ///< Vehicle drawn in vehicle list, group list, ...
	EIT_PURCHASE   = 0x20,  ///< Vehicle drawn in purchase list, autoreplace gui, ...
	EIT_PREVIEW    = 0x21,  ///< Vehicle drawn in preview window, news, ...
};

static const uint32 VEHICLE_NAME_NO_GROUP = 0x80000000; ///< String constant to not include the vehicle's group name, if using the long name format

#endif /* VEHICLE_TYPE_H */
