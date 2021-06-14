/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_sl.cpp Code handling saving and loading of link graphs */

#include "../stdafx.h"
#include "../linkgraph/linkgraph.h"
#include "../linkgraph/linkgraphjob.h"
#include "../linkgraph/linkgraphschedule.h"
#include "../network/network.h"
#include "../settings_internal.h"
#include "saveload.h"

#include "../safeguards.h"

typedef LinkGraph::BaseNode Node;
typedef LinkGraph::BaseEdge Edge;

static uint16 _num_nodes;
static LinkGraph *_linkgraph; ///< Contains the current linkgraph being saved/loaded.
static NodeID _linkgraph_from; ///< Contains the current "from" node being saved/loaded.

class SlLinkgraphEdge : public DefaultSaveLoadHandler<SlLinkgraphEdge, Node> {
public:
	inline static const SaveLoad description[] = {
		SLE_CONDNULL(4, SL_MIN_VERSION, SLV_191), // distance
		    SLE_VAR(Edge, capacity,                 SLE_UINT32),
		    SLE_VAR(Edge, usage,                    SLE_UINT32),
		    SLE_VAR(Edge, last_unrestricted_update, SLE_INT32),
		SLE_CONDVAR(Edge, last_restricted_update,   SLE_INT32, SLV_187, SL_MAX_VERSION),
		    SLE_VAR(Edge, next_edge,                SLE_UINT16),
	};

	void Save(Node *bn) const override
	{
		for (NodeID to = _linkgraph_from; to != INVALID_NODE; to = _linkgraph->edges[_linkgraph_from][to].next_edge) {
			SlObject(&_linkgraph->edges[_linkgraph_from][to], this->GetDescription());
		}
	}

	void Load(Node *bn) const override
	{
		uint16 max_size = _linkgraph->Size();

		if (IsSavegameVersionBefore(SLV_191)) {
			/* We used to save the full matrix ... */
			for (NodeID to = 0; to < max_size; ++to) {
				SlObject(&_linkgraph->edges[_linkgraph_from][to], this->GetDescription());
			}
			return;
		}

		/* ... but as that wasted a lot of space we save a sparse matrix now. */
		for (NodeID to = _linkgraph_from; to != INVALID_NODE; to = _linkgraph->edges[_linkgraph_from][to].next_edge) {
			if (to >= max_size) SlErrorCorrupt("Link graph structure overflow");
			SlObject(&_linkgraph->edges[_linkgraph_from][to], this->GetDescription());
		}
	}
};

class SlLinkgraphNode : public DefaultSaveLoadHandler<SlLinkgraphNode, LinkGraph> {
public:
	inline static const SaveLoad description[] = {
		SLE_CONDVAR(Node, xy,          SLE_UINT32, SLV_191, SL_MAX_VERSION),
		    SLE_VAR(Node, supply,      SLE_UINT32),
		    SLE_VAR(Node, demand,      SLE_UINT32),
		    SLE_VAR(Node, station,     SLE_UINT16),
		    SLE_VAR(Node, last_update, SLE_INT32),
		SLEG_STRUCTLIST(SlLinkgraphEdge),
	};

	void Save(LinkGraph *lg) const override
	{
		_linkgraph = lg;

		for (NodeID from = 0; from < lg->Size(); ++from) {
			_linkgraph_from = from;
			SlObject(&lg->nodes[from], this->GetDescription());
		}
	}

	void Load(LinkGraph *lg) const override
	{
		_linkgraph = lg;

		lg->Init(_num_nodes);
		for (NodeID from = 0; from < _num_nodes; ++from) {
			_linkgraph_from = from;
			SlObject(&lg->nodes[from], this->GetDescription());
		}
	}
};

/**
 * Get a SaveLoad array for a link graph.
 * @return SaveLoad array for link graph.
 */
SaveLoadTable GetLinkGraphDesc()
{
	static const SaveLoad link_graph_desc[] = {
		 SLE_VAR(LinkGraph, last_compression, SLE_INT32),
		SLEG_VAR(_num_nodes,                  SLE_UINT16),
		 SLE_VAR(LinkGraph, cargo,            SLE_UINT8),
		SLEG_STRUCTLIST(SlLinkgraphNode),
	};
	return link_graph_desc;
}

/**
 * Proxy to reuse LinkGraph to save/load a LinkGraphJob.
 * One of the members of a LinkGraphJob is a LinkGraph, but SLEG_STRUCT()
 * doesn't allow us to select a member. So instead, we add a bit of glue to
 * accept a LinkGraphJob, get the LinkGraph, and use that to call the
 * save/load routines for a regular LinkGraph.
 */
class SlLinkgraphJobProxy : public DefaultSaveLoadHandler<SlLinkgraphJobProxy, LinkGraphJob> {
public:
	inline static const SaveLoad description[] = {{}}; // Needed to keep DefaultSaveLoadHandler happy.
	SaveLoadTable GetDescription() const override { return GetLinkGraphDesc(); }

	void Save(LinkGraphJob *lgj) const override
	{
		SlObject(const_cast<LinkGraph *>(&lgj->Graph()), GetLinkGraphDesc());
	}

	void Load(LinkGraphJob *lgj) const override
	{
		SlObject(const_cast<LinkGraph *>(&lgj->Graph()), GetLinkGraphDesc());
	}
};

/**
 * Get a SaveLoad array for a link graph job. The settings struct is derived from
 * the global settings saveload array. The exact entries are calculated when the function
 * is called the first time.
 * It's necessary to keep a copy of the settings for each link graph job so that you can
 * change the settings while in-game and still not mess with current link graph runs.
 * Of course the settings have to be saved and loaded, too, to avoid desyncs.
 * @return Array of SaveLoad structs.
 */
SaveLoadTable GetLinkGraphJobDesc()
{
	static std::vector<SaveLoad> saveloads;
	static const char *prefix = "linkgraph.";

	static const SaveLoad job_desc[] = {
		SLE_VAR(LinkGraphJob, join_date,        SLE_INT32),
		SLE_VAR(LinkGraphJob, link_graph.index, SLE_UINT16),
		SLEG_STRUCT(SlLinkgraphJobProxy),
	};

	/* The member offset arithmetic below is only valid if the types in question
	 * are standard layout types. Otherwise, it would be undefined behaviour. */
	static_assert(std::is_standard_layout<LinkGraphSettings>::value, "LinkGraphSettings needs to be a standard layout type");

	/* We store the offset of each member of the #LinkGraphSettings in the
	 * extra data of the saveload struct. Use it together with the address
	 * of the settings struct inside the job to find the final memory address. */
	static SaveLoadAddrProc * const proc = [](void *b, size_t extra) -> void * { return const_cast<void *>(static_cast<const void *>(reinterpret_cast<const char *>(std::addressof(static_cast<LinkGraphJob *>(b)->settings)) + extra)); };

	/* Build the SaveLoad array on first call and don't touch it later on */
	if (saveloads.size() == 0) {
		GetSettingSaveLoadByPrefix(prefix, saveloads);

		for (auto &sl : saveloads) {
			sl.address_proc = proc;
		}

		for (auto &sld : job_desc) {
			saveloads.push_back(sld);
		}
	}

	return saveloads;
}

/**
 * Get a SaveLoad array for the link graph schedule.
 * @return SaveLoad array for the link graph schedule.
 */
SaveLoadTable GetLinkGraphScheduleDesc()
{
	static const SaveLoad schedule_desc[] = {
		SLE_REFLIST(LinkGraphSchedule, schedule, REF_LINK_GRAPH),
		SLE_REFLIST(LinkGraphSchedule, running,  REF_LINK_GRAPH_JOB),
	};
	return schedule_desc;
}

/**
 * Spawn the threads for running link graph calculations.
 * Has to be done after loading as the cargo classes might have changed.
 */
void AfterLoadLinkGraphs()
{
	if (IsSavegameVersionBefore(SLV_191)) {
		for (LinkGraph *lg : LinkGraph::Iterate()) {
			for (NodeID node_id = 0; node_id < lg->Size(); ++node_id) {
				const Station *st = Station::GetIfValid((*lg)[node_id].Station());
				if (st != nullptr) (*lg)[node_id].UpdateLocation(st->xy);
			}
		}

		for (LinkGraphJob *lgj : LinkGraphJob::Iterate()) {
			LinkGraph *lg = &(const_cast<LinkGraph &>(lgj->Graph()));
			for (NodeID node_id = 0; node_id < lg->Size(); ++node_id) {
				const Station *st = Station::GetIfValid((*lg)[node_id].Station());
				if (st != nullptr) (*lg)[node_id].UpdateLocation(st->xy);
			}
		}
	}

	LinkGraphSchedule::instance.SpawnAll();

	if (!_networking || _network_server) {
		AfterLoad_LinkGraphPauseControl();
	}
}

/**
 * Save all link graphs.
 */
static void Save_LGRP()
{
	for (LinkGraph *lg : LinkGraph::Iterate()) {
		_num_nodes = lg->Size();

		SlSetArrayIndex(lg->index);
		SlObject(lg, GetLinkGraphDesc());
	}
}

/**
 * Load all link graphs.
 */
static void Load_LGRP()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		LinkGraph *lg = new (index) LinkGraph();
		SlObject(lg, GetLinkGraphDesc());
	}
}

/**
 * Save all link graph jobs.
 */
static void Save_LGRJ()
{
	for (LinkGraphJob *lgj : LinkGraphJob::Iterate()) {
		_num_nodes = lgj->Size();

		SlSetArrayIndex(lgj->index);
		SlObject(lgj, GetLinkGraphJobDesc());
	}
}

/**
 * Load all link graph jobs.
 */
static void Load_LGRJ()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		LinkGraphJob *lgj = new (index) LinkGraphJob();
		SlObject(lgj, GetLinkGraphJobDesc());
	}
}

/**
 * Save the link graph schedule.
 */
static void Save_LGRS()
{
	SlObject(&LinkGraphSchedule::instance, GetLinkGraphScheduleDesc());
}

/**
 * Load the link graph schedule.
 */
static void Load_LGRS()
{
	SlObject(&LinkGraphSchedule::instance, GetLinkGraphScheduleDesc());
}

/**
 * Substitute pointers in link graph schedule.
 */
static void Ptrs_LGRS()
{
	SlObject(&LinkGraphSchedule::instance, GetLinkGraphScheduleDesc());
}

static const ChunkHandler linkgraph_chunk_handlers[] = {
	{ 'LGRP', Save_LGRP, Load_LGRP, nullptr,   nullptr, CH_ARRAY },
	{ 'LGRJ', Save_LGRJ, Load_LGRJ, nullptr,   nullptr, CH_ARRAY },
	{ 'LGRS', Save_LGRS, Load_LGRS, Ptrs_LGRS, nullptr, CH_RIFF  }
};

extern const ChunkHandlerTable _linkgraph_chunk_handlers(linkgraph_chunk_handlers);
