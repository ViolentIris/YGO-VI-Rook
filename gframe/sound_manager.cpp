#include "sound_manager.h"
#include "myfilesystem.h"
#if defined(YGOPRO_USE_MINIAUDIO) && defined(YGOPRO_MINIAUDIO_SUPPORT_OPUS_VORBIS)
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#endif
#ifdef IRRKLANG_STATIC
#include "../ikpmp3/ikpMP3.h"
#endif

namespace ygo {

SoundManager soundManager;

bool SoundManager::Init() {
#ifdef YGOPRO_USE_AUDIO
	bgm_scene = -1;
	previous_bgm_scene = -1;
	RefreshBGMList();
	bgm_process = false;
	rnd.seed(std::random_device()());
#ifdef YGOPRO_USE_MINIAUDIO
	engineConfig = ma_engine_config_init();
#ifdef YGOPRO_MINIAUDIO_SUPPORT_OPUS_VORBIS
	ma_decoding_backend_vtable* pCustomBackendVTables[] =
	{
		ma_decoding_backend_libvorbis,
		ma_decoding_backend_libopus
	};
	resourceManagerConfig = ma_resource_manager_config_init();
	resourceManagerConfig.ppCustomDecodingBackendVTables = pCustomBackendVTables;
	resourceManagerConfig.customDecodingBackendCount = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
	resourceManagerConfig.pCustomDecodingBackendUserData = NULL;
	if(ma_resource_manager_init(&resourceManagerConfig, &resourceManager) != MA_SUCCESS) {
		return false;
	}
	engineConfig.pResourceManager = &resourceManager;
#endif
	if(ma_engine_init(&engineConfig, &engineSound) != MA_SUCCESS || ma_engine_init(&engineConfig, &engineMusic) != MA_SUCCESS) {
		return false;
	} else {
		return true;
	}
#endif // YGOPRO_USE_MINIAUDIO
#ifdef YGOPRO_USE_IRRKLANG
	engineSound = irrklang::createIrrKlangDevice();
	engineMusic = irrklang::createIrrKlangDevice();
	if(!engineSound || !engineMusic) {
		return false;
	} else {
#ifdef IRRKLANG_STATIC
		irrklang::ikpMP3Init(engineMusic);
#endif
		return true;
	}
#endif // YGOPRO_USE_IRRKLANG
#endif // YGOPRO_USE_AUDIO
	return false;
}
void SoundManager::RefreshBGMList() {
#ifdef YGOPRO_USE_AUDIO
	RefershBGMDir(L"", BGM_DUEL);
	RefershBGMDir(L"duel", BGM_DUEL);
	RefershBGMDir(L"menu", BGM_MENU);
	RefershBGMDir(L"deck", BGM_DECK);
	RefershBGMDir(L"advantage", BGM_ADVANTAGE);
	RefershBGMDir(L"disadvantage", BGM_DISADVANTAGE);
	RefershBGMDir(L"win", BGM_WIN);
	RefershBGMDir(L"lose", BGM_LOSE);
#endif
}
void SoundManager::RefershBGMDir(std::wstring path, int scene) {
#ifdef YGOPRO_USE_AUDIO
	std::wstring search = L"./sound/BGM/" + path;
	FileSystem::TraversalDir(search.c_str(), [this, &path, scene](const wchar_t* name, bool isdir) {
		if(!isdir && (
			IsExtension(name, L".mp3")
#if defined(YGOPRO_MINIAUDIO_SUPPORT_OPUS_VORBIS) || defined(YGOPRO_USE_IRRKLANG)
			|| IsExtension(name, L".ogg")
#endif
			)) {
			std::wstring filename = path + L"/" + name;
			BGMList[BGM_ALL].push_back(filename);
			BGMList[scene].push_back(filename);
		}
	});
#endif // YGOPRO_USE_AUDIO
}
void SoundManager::PlaySound(wchar_t* sound) {
#ifdef YGOPRO_USE_AUDIO
	if(!mainGame->chkEnableSound->isChecked())
		return;
	SetSoundVolume(mainGame->gameConf.sound_volume);
#ifdef YGOPRO_USE_MINIAUDIO
	ma_sound *usingSoundEffectPointer = nullptr;
	for(int i = 0; i < 10; i++) {
		if(playingSoundEffect[i] && !ma_sound_is_playing(playingSoundEffect[i])) {
			ma_sound_uninit(playingSoundEffect[i]);
			if(usingSoundEffectPointer) {
				free(playingSoundEffect[i]);
				playingSoundEffect[i] = nullptr;
			} else {
				usingSoundEffectPointer = playingSoundEffect[i];
			}
		}
		if(!playingSoundEffect[i] && !usingSoundEffectPointer) {
			usingSoundEffectPointer = playingSoundEffect[i] = (ma_sound*)malloc(sizeof(ma_sound));
		}
	}
	if (!usingSoundEffectPointer) {
		// force to stop the first sound
		usingSoundEffectPointer = playingSoundEffect[0];
		ma_sound_uninit(usingSoundEffectPointer);
	}
	ma_sound_init_from_file_w(&engineSound, sound, MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM, nullptr, nullptr, usingSoundEffectPointer);
	ma_sound_start(usingSoundEffectPointer);
#endif
#ifdef YGOPRO_USE_IRRKLANG
		char csound[1024];
		BufferIO::EncodeUTF8(sound, csound);
	engineSound->play2D(csound);
#endif
#endif
}
void SoundManager::PlaySoundEffect(int sound) {
#ifdef YGOPRO_USE_AUDIO
	if(!mainGame->chkEnableSound->isChecked())
		return;
	char soundName[32];
	switch(sound) {
	case SOUND_SUMMON: {
		strcpy(soundName, "summon");
		break;
	}
	case SOUND_SPECIAL_SUMMON: {
		strcpy(soundName, "specialsummon");
		break;
	}
	case SOUND_ACTIVATE: {
		strcpy(soundName, "activate");
		break;
	}
	case SOUND_SET: {
		strcpy(soundName, "set");
		break;
	}
	case SOUND_FILP: {
		strcpy(soundName, "flip");
		break;
	}
	case SOUND_REVEAL: {
		strcpy(soundName, "reveal");
		break;
	}
	case SOUND_EQUIP: {
		strcpy(soundName, "equip");
		break;
	}
	case SOUND_DESTROYED: {
		strcpy(soundName, "destroyed");
		break;
	}
	case SOUND_BANISHED: {
		strcpy(soundName, "banished");
		break;
	}
	case SOUND_TOKEN: {
		strcpy(soundName, "token");
		break;
	}
	case SOUND_NEGATE: {
		strcpy(soundName, "negate");
		break;
	}
	case SOUND_ATTACK: {
		strcpy(soundName, "attack");
		break;
	}
	case SOUND_DIRECT_ATTACK: {
		strcpy(soundName, "directattack");
		break;
	}
	case SOUND_DRAW: {
		strcpy(soundName, "draw");
		break;
	}
	case SOUND_SHUFFLE: {
		strcpy(soundName, "shuffle");
		break;
	}
	case SOUND_DAMAGE: {
		strcpy(soundName, "damage");
		break;
	}
	case SOUND_RECOVER: {
		strcpy(soundName, "gainlp");
		break;
	}
	case SOUND_COUNTER_ADD: {
		strcpy(soundName, "addcounter");
		break;
	}
	case SOUND_COUNTER_REMOVE: {
		strcpy(soundName, "removecounter");
		break;
	}
	case SOUND_COIN: {
		strcpy(soundName, "coinflip");
		break;
	}
	case SOUND_DICE: {
		strcpy(soundName, "diceroll");
		break;
	}
	case SOUND_NEXT_TURN: {
		strcpy(soundName, "nextturn");
		break;
	}
	case SOUND_PHASE: {
		strcpy(soundName, "phase");
		break;
	}
	case SOUND_MENU: {
		strcpy(soundName, "menu");
		break;
	}
	case SOUND_BUTTON: {
		strcpy(soundName, "button");
		break;
	}
	case SOUND_INFO: {
		strcpy(soundName, "info");
		break;
	}
	case SOUND_QUESTION: {
		strcpy(soundName, "question");
		break;
	}
	case SOUND_CARD_PICK: {
		strcpy(soundName, "cardpick");
		break;
	}
	case SOUND_CARD_DROP: {
		strcpy(soundName, "carddrop");
		break;
	}
	case SOUND_PLAYER_ENTER: {
		strcpy(soundName, "playerenter");
		break;
	}
	case SOUND_CHAT: {
		strcpy(soundName, "chatmessage");
		break;
	}
	default:
		break;
	}
	char soundPath[40];
	std::snprintf(soundPath, 40, "./sound/%s.wav", soundName);
	SetSoundVolume(mainGame->gameConf.sound_volume);
#ifdef YGOPRO_USE_MINIAUDIO
	ma_engine_play_sound(&engineSound, soundPath, nullptr);
#endif
#ifdef YGOPRO_USE_IRRKLANG
	engineSound->play2D(soundPath);
#endif
#endif // YGOPRO_USE_AUDIO
}
void SoundManager::PlayDialogSound(irr::gui::IGUIElement * element) {
#ifdef YGOPRO_USE_AUDIO
	if(element == mainGame->wMessage) {
		PlaySoundEffect(SOUND_INFO);
	} else if(element == mainGame->wQuery) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wSurrender) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wOptions) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wANAttribute) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wANCard) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wANNumber) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wANRace) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wReplaySave) {
		PlaySoundEffect(SOUND_QUESTION);
	} else if(element == mainGame->wFTSelect) {
		PlaySoundEffect(SOUND_QUESTION);
	}
#endif // YGOPRO_USE_AUDIO
}
bool SoundManager::IsPlayingMusic(wchar_t* music) {
#ifdef YGOPRO_USE_MINIAUDIO
	if(music) {
		return !mywcsncasecmp(currentPlayingMusic, music, 1024) && ma_sound_is_playing(&soundBGM);
	} else {
		return ma_sound_is_playing(&soundBGM);
	}
#endif
#ifdef YGOPRO_USE_IRRKLANG
	if(music) {
		char cmusic[1024];
		BufferIO::EncodeUTF8(music, cmusic);
		return engineMusic->isCurrentlyPlaying(cmusic);
	} else {
		return soundBGM && !soundBGM->isFinished();
	}
#endif
	return false;
}
void SoundManager::PlayMusic(wchar_t* music, bool loop) {
#ifdef YGOPRO_USE_AUDIO
	if(!mainGame->chkEnableMusic->isChecked())
		return;
	if(!IsPlayingMusic(music)) {
		StopBGM();
	SetMusicVolume(mainGame->gameConf.music_volume);
#ifdef YGOPRO_USE_MINIAUDIO
		BufferIO::CopyWStr(music, currentPlayingMusic, 1024);
		ma_sound_init_from_file_w(&engineMusic, music, MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM, nullptr, nullptr, &soundBGM);
		ma_sound_set_looping(&soundBGM, loop);
		ma_sound_start(&soundBGM);
#endif
#ifdef YGOPRO_USE_IRRKLANG
		char cmusic[1024];
		BufferIO::EncodeUTF8(music, cmusic);
		soundBGM = engineMusic->play2D(cmusic, loop, false, true);
#endif
	}
#endif
}
void SoundManager::PlayBGM(int scene) {
#ifdef YGOPRO_USE_AUDIO
	if(!mainGame->chkEnableMusic->isChecked() || bgm_process)
		return;
	if(!mainGame->chkMusicMode->isChecked())
		scene = BGM_ALL;
	if((scene != bgm_scene) && (bgm_scene != BGM_CUSTOM) || (scene != previous_bgm_scene) && (bgm_scene == BGM_CUSTOM) || !IsPlayingMusic()) {
		int count = BGMList[scene].size();
		if(count <= 0)
			return;
		bgm_scene = scene;
		int bgm = (count > 1) ? std::uniform_int_distribution<>(0, count - 1)(rnd) : 0;
		auto name = BGMList[scene][bgm].c_str();
		wchar_t BGMName[1024];
		myswprintf(BGMName, L"./sound/BGM/%ls", name);
		PlayMusic(BGMName, false);
	}
#endif
}
void SoundManager::PlayCustomBGM(wchar_t* BGMName) {
#ifdef YGOPRO_USE_AUDIO
	if(!mainGame->chkEnableMusic->isChecked() || !mainGame->chkMusicMode->isChecked() || bgm_process || IsPlayingMusic(BGMName))
		return;
	bgm_process = true;
	int pscene = bgm_scene;
	if (pscene != BGM_CUSTOM)
		previous_bgm_scene = pscene;
	bgm_scene = BGM_CUSTOM;
	PlayMusic(BGMName, false);
	bgm_process = false;
#endif
}
void SoundManager::PlayCustomSound(wchar_t* SoundName) {
	PlaySound(SoundName);
}
void SoundManager::StopBGM() {
#ifdef YGOPRO_USE_MINIAUDIO
	if(!currentPlayingMusic[0])
		return;
	memset(currentPlayingMusic, 0, sizeof(currentPlayingMusic));
	ma_sound_uninit(&soundBGM);
#endif
#ifdef YGOPRO_USE_IRRKLANG
	engineMusic->stopAllSounds();
#endif
}
void SoundManager::StopSound() {
#ifdef YGOPRO_USE_MINIAUDIO
	for(int i = 0; i < 10; i++) {
		if(playingSoundEffect[i]) {
			ma_sound_uninit(playingSoundEffect[i]);
			free(playingSoundEffect[i]);
			playingSoundEffect[i] = nullptr;
		}
	}
#endif
#ifdef YGOPRO_USE_IRRKLANG
	engineSound->stopAllSounds();
#endif
}
void SoundManager::SetSoundVolume(double volume) {
#ifdef YGOPRO_USE_MINIAUDIO
	ma_engine_set_volume(&engineSound, volume);
#endif
#ifdef YGOPRO_USE_IRRKLANG
	engineSound->setSoundVolume(volume);
#endif
}
void SoundManager::SetMusicVolume(double volume) {
#ifdef YGOPRO_USE_MINIAUDIO
	ma_engine_set_volume(&engineMusic, volume);
#endif
#ifdef YGOPRO_USE_IRRKLANG
	engineMusic->setSoundVolume(volume);
#endif
}
}
