/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog_sl.cpp Code handling saving and loading of gamelog data */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/gamelog_sl_compat.h"

#include "../gamelog_internal.h"
#include "../fios.h"
#include "../load_check.h"
#include "../string_func.h"

#include "../safeguards.h"

namespace upstream_sl {

class SlGamelogMode : public DefaultSaveLoadHandler<SlGamelogMode, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, mode.mode,         SLE_UINT8),
		SLE_VAR(LoggedChange, mode.landscape,    SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_mode_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_MODE) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_MODE) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static char old_revision_text[GAMELOG_REVISION_LENGTH];
static std::string revision_test;

class SlGamelogRevision : public DefaultSaveLoadHandler<SlGamelogRevision, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLEG_CONDARR("revision.text", old_revision_text, SLE_UINT8, GAMELOG_REVISION_LENGTH, SL_MIN_VERSION,     SLV_STRING_GAMELOG),
		SLEG_CONDSSTR("revision.text",    revision_test, SLE_STR,                            SLV_STRING_GAMELOG, SL_MAX_VERSION),
		SLE_VAR(LoggedChange, revision.newgrf,   SLE_UINT32),
		SLE_VAR(LoggedChange, revision.slver,    SLE_UINT16),
		SLE_VAR(LoggedChange, revision.modified, SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_revision_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_REVISION) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_REVISION) return;
		SlObject(lc, this->GetLoadDescription());
		if (IsSavegameVersionBefore(SLV_STRING_GAMELOG)) {
			lc->revision.text = stredup(old_revision_text, lastof(old_revision_text));
		} else {
			lc->revision.text = stredup(revision_test.c_str());
		}
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogOldver : public DefaultSaveLoadHandler<SlGamelogOldver, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, oldver.type,       SLE_UINT32),
		SLE_VAR(LoggedChange, oldver.version,    SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_oldver_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_OLDVER) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_OLDVER) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogSetting : public DefaultSaveLoadHandler<SlGamelogSetting, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_STR(LoggedChange, setting.name,      SLE_STR,    128),
		SLE_VAR(LoggedChange, setting.oldval,    SLE_INT32),
		SLE_VAR(LoggedChange, setting.newval,    SLE_INT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_setting_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_SETTING) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_SETTING) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfadd : public DefaultSaveLoadHandler<SlGamelogGrfadd, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfadd.grfid,      SLE_UINT32    ),
		SLE_ARR(LoggedChange, grfadd.md5sum,     SLE_UINT8,  16),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfadd_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFADD) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFADD) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfrem : public DefaultSaveLoadHandler<SlGamelogGrfrem, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfrem.grfid,      SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfrem_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFREM) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFREM) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfcompat : public DefaultSaveLoadHandler<SlGamelogGrfcompat, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfcompat.grfid,   SLE_UINT32    ),
		SLE_ARR(LoggedChange, grfcompat.md5sum,  SLE_UINT8,  16),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfcompat_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFCOMPAT) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFCOMPAT) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfparam : public DefaultSaveLoadHandler<SlGamelogGrfparam, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfparam.grfid,    SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfparam_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFPARAM) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFPARAM) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfmove : public DefaultSaveLoadHandler<SlGamelogGrfmove, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfmove.grfid,     SLE_UINT32),
		SLE_VAR(LoggedChange, grfmove.offset,    SLE_INT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfmove_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFMOVE) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFMOVE) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfbug : public DefaultSaveLoadHandler<SlGamelogGrfbug, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(LoggedChange, grfbug.data,       SLE_UINT64),
		SLE_VAR(LoggedChange, grfbug.grfid,      SLE_UINT32),
		SLE_VAR(LoggedChange, grfbug.bug,        SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfbug_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFBUG) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFBUG) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static bool _is_emergency_save = true;

class SlGamelogEmergency : public DefaultSaveLoadHandler<SlGamelogEmergency, LoggedChange> {
public:
	/* We need to store something, so store a "true" value. */
	inline static const SaveLoad description[] = {
		SLEG_CONDVAR("is_emergency_save", _is_emergency_save, SLE_BOOL, SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_emergency_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_EMERGENCY) return;

		_is_emergency_save = true;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_EMERGENCY) return;

		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogAction : public DefaultSaveLoadHandler<SlGamelogAction, LoggedAction> {
public:
	inline static const SaveLoad description[] = {
		SLE_SAVEBYTE(LoggedChange, ct),
		SLEG_STRUCT("mode", SlGamelogMode),
		SLEG_STRUCT("revision", SlGamelogRevision),
		SLEG_STRUCT("oldver", SlGamelogOldver),
		SLEG_STRUCT("setting", SlGamelogSetting),
		SLEG_STRUCT("grfadd", SlGamelogGrfadd),
		SLEG_STRUCT("grfrem", SlGamelogGrfrem),
		SLEG_STRUCT("grfcompat", SlGamelogGrfcompat),
		SLEG_STRUCT("grfparam", SlGamelogGrfparam),
		SLEG_STRUCT("grfmove", SlGamelogGrfmove),
		SLEG_STRUCT("grfbug", SlGamelogGrfbug),
		SLEG_STRUCT("emergency", SlGamelogEmergency),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_action_sl_compat;

	void Save(LoggedAction *la) const override
	{
		SlSetStructListLength(la->changes.size());

		for (LoggedChange &lc : la->changes) {
			assert((uint)lc.ct < GLCT_END);
			SlObject(&lc, this->GetDescription());
		}
	}

	void Load(LoggedAction *la) const override
	{
		la->changes.clear();

		if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
			byte type;
			while ((type = SlReadByte()) != GLCT_NONE) {
				if (type >= GLCT_END) SlErrorCorrupt("Invalid gamelog change type");
				GamelogChangeType ct = (GamelogChangeType)type;

				la->changes.push_back({});
				LoggedChange *lc = &la->changes.back();
				lc->ct = ct;

				SlObject(lc, this->GetLoadDescription());
			}
			return;
		}

		size_t length = SlGetStructListLength(UINT32_MAX);
		la->changes.reserve(length);

		for (size_t i = 0; i < length; i++) {
			la->changes.push_back({});
			LoggedChange *lc = &la->changes.back();

			lc->ct = (GamelogChangeType)SlReadByte();
			SlObject(lc, this->GetLoadDescription());
		}
	}

	void LoadCheck(LoggedAction *la) const override { this->Load(la); }
};

static const SaveLoad _gamelog_desc[] = {
	SLE_CONDVAR(LoggedAction, at,            SLE_UINT8,   SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(LoggedAction, tick, SLE_FILE_U16 | SLE_VAR_U64, SL_MIN_VERSION, SLV_U64_TICK_COUNTER),
	SLE_CONDVAR(LoggedAction, tick, SLE_UINT64,                 SLV_U64_TICK_COUNTER, SL_MAX_VERSION),
	SLEG_STRUCTLIST("action", SlGamelogAction),
};

struct GLOGChunkHandler : ChunkHandler {
	GLOGChunkHandler() : ChunkHandler('GLOG', CH_TABLE) {}

	void LoadCommon(std::vector<LoggedAction> &gamelog_actions) const
	{
		assert(gamelog_actions.empty());

		const std::vector<SaveLoad> slt = SlCompatTableHeader(_gamelog_desc, _gamelog_sl_compat);

		if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
			byte type;
			while ((type = SlReadByte()) != GLAT_NONE) {
				if (type >= GLAT_END) SlErrorCorrupt("Invalid gamelog action type");

				LoggedAction &la = gamelog_actions.emplace_back();

				la.at = (GamelogActionType)type;
				SlObject(&la, slt);
			}
			return;
		}

		while (SlIterateArray() != -1) {
			LoggedAction &la = gamelog_actions.emplace_back();

			SlObject(&la, slt);
		}
	}

	void Save() const override
	{
		SlTableHeader(_gamelog_desc);

		uint i = 0;
		for (LoggedAction &la : _gamelog_actions) {
			SlSetArrayIndex(i);
			SlObject(&la, _gamelog_desc);
			i++;
		}
	}

	void Load() const override
	{
		this->LoadCommon(_gamelog_actions);
	}

	void LoadCheck(size_t) const override
	{
		this->LoadCommon(_load_check_data.gamelog_actions);
	}
};

static const GLOGChunkHandler GLOG;
static const ChunkHandlerRef gamelog_chunk_handlers[] = {
	GLOG,
};

extern const ChunkHandlerTable _gamelog_chunk_handlers(gamelog_chunk_handlers);

}
