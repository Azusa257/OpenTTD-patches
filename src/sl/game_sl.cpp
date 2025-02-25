/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_sl.cpp Handles the saveload part of the GameScripts */

#include "../stdafx.h"
#include "../debug.h"
#include "saveload.h"
#include "../string_func.h"

#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../network/network.h"
#include "../game/game_instance.hpp"
#include "../game/game_text.hpp"

#include "../safeguards.h"

static std::string _game_saveload_name;
static int         _game_saveload_version;
static std::string _game_saveload_settings;
static bool        _game_saveload_is_random;

static const SaveLoad _game_script[] = {
	   SLEG_SSTR(_game_saveload_name,         SLE_STR),
	   SLEG_SSTR(_game_saveload_settings,     SLE_STR),
	    SLEG_VAR(_game_saveload_version,   SLE_UINT32),
	    SLEG_VAR(_game_saveload_is_random,   SLE_BOOL),
};

static void SaveReal_GSDT(int *index_ptr)
{
	GameConfig *config = GameConfig::GetConfig();

	if (config->HasScript()) {
		_game_saveload_name = config->GetName();
		_game_saveload_version = config->GetVersion();
	} else {
		/* No GameScript is configured for this so store an empty string as name. */
		_game_saveload_name.clear();
		_game_saveload_version = -1;
	}

	_game_saveload_is_random = config->IsRandom();
	_game_saveload_settings = config->SettingsToString();

	SlObject(nullptr, _game_script);
	Game::Save();
}

static void Load_GSDT()
{
	/* Free all current data */
	GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME)->Change(std::nullopt);

	if ((CompanyID)SlIterateArray() == (CompanyID)-1) return;

	_game_saveload_version = -1;
	SlObject(nullptr, _game_script);

	if (_game_mode == GM_MENU || (_networking && !_network_server)) {
		GameInstance::LoadEmpty();
		if ((CompanyID)SlIterateArray() != (CompanyID)-1) SlErrorCorrupt("Too many GameScript configs");
		return;
	}

	GameConfig *config = GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME);
	if (!_game_saveload_name.empty()) {
		config->Change(_game_saveload_name, _game_saveload_version, false, _game_saveload_is_random);
		if (!config->HasScript()) {
			/* No version of the GameScript available that can load the data. Try to load the
			 * latest version of the GameScript instead. */
			config->Change(_game_saveload_name, -1, false, _game_saveload_is_random);
			if (!config->HasScript()) {
				if (_game_saveload_name.compare("%_dummy") != 0) {
					DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", _game_saveload_name.c_str(), _game_saveload_version);
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				} else {
					DEBUG(script, 0, "The savegame had no GameScript available at the time of saving.");
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				}
			} else {
				DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", _game_saveload_name.c_str(), _game_saveload_version);
				DEBUG(script, 0, "The latest version of that GameScript has been loaded instead, but it'll not get the savegame data as it's incompatible.");
			}
			/* Make sure the GameScript doesn't get the saveload data, as it was not the
			 *  writer of the saveload data in the first place */
			_game_saveload_version = -1;
		}
	}

	config->StringToSettings(_game_saveload_settings);

	/* Load the GameScript saved data */
	config->SetToLoadData(GameInstance::Load(_game_saveload_version));

	if ((CompanyID)SlIterateArray() != (CompanyID)-1) SlErrorCorrupt("Too many GameScript configs");
}

static void Save_GSDT()
{
	SlSetArrayIndex(0);
	SlAutolength((AutolengthProc *)SaveReal_GSDT, nullptr);
}

extern GameStrings *_current_data;

static std::string _game_saveload_string;
static uint _game_saveload_strings;

static const SaveLoad _game_language_header[] = {
	SLEG_SSTR(_game_saveload_string, SLE_STR),
	 SLEG_VAR(_game_saveload_strings, SLE_UINT32),
};

static const SaveLoad _game_language_string[] = {
	SLEG_SSTR(_game_saveload_string, SLE_STR | SLF_ALLOW_CONTROL),
};

static void SaveReal_GSTR(const LanguageStrings *ls)
{
	_game_saveload_string  = ls->language.c_str();
	_game_saveload_strings = (uint)ls->lines.size();

	SlObject(nullptr, _game_language_header);
	for (const auto &i : ls->lines) {
		_game_saveload_string = i.c_str();
		SlObject(nullptr, _game_language_string);
	}
}

static void Load_GSTR()
{
	delete _current_data;
	_current_data = new GameStrings();

	while (SlIterateArray() != -1) {
		_game_saveload_string.clear();
		SlObject(nullptr, _game_language_header);

		LanguageStrings ls(_game_saveload_string);
		for (uint i = 0; i < _game_saveload_strings; i++) {
			SlObject(nullptr, _game_language_string);
			ls.lines.emplace_back(_game_saveload_string);
		}

		_current_data->raw_strings.push_back(std::move(ls));
	}

	/* If there were no strings in the savegame, set GameStrings to nullptr */
	if (_current_data->raw_strings.size() == 0) {
		delete _current_data;
		_current_data = nullptr;
		return;
	}

	_current_data->Compile();
	ReconsiderGameScriptLanguage();
}

static void Save_GSTR()
{
	if (_current_data == nullptr) return;

	for (uint i = 0; i < _current_data->raw_strings.size(); i++) {
		SlSetArrayIndex(i);
		SlAutolength((AutolengthProc *)SaveReal_GSTR, &_current_data->raw_strings[i]);
	}
}

static const ChunkHandler game_chunk_handlers[] = {
	{ 'GSTR', Save_GSTR, Load_GSTR, nullptr, nullptr, CH_ARRAY },
	{ 'GSDT', Save_GSDT, Load_GSDT, nullptr, nullptr, CH_ARRAY },
};

extern const ChunkHandlerTable _game_chunk_handlers(game_chunk_handlers);
