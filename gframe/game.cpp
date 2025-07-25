#include "config.h"
#include "game.h"
#include "myfilesystem.h"
#include "image_manager.h"
#include "data_manager.h"
#include "deck_manager.h"
#include "sound_manager.h"
#include "replay.h"
#include "materials.h"
#include "duelclient.h"
#include "netserver.h"
#include "single_mode.h"
#include <sstream>
#include <thread>
#include <regex>

unsigned short PRO_VERSION = 0x1361;

bool delay_swap = false;
int swap_player = 0;

namespace ygo {

Game* mainGame;

void DuelInfo::Clear() {
	isStarted = false;
	isInDuel = false;
	isFinished = false;
	isReplay = false;
	isReplaySkiping = false;
	isFirst = false;
	isTag = false;
	isSingleMode = false;
	is_shuffling = false;
	tag_player[0] = false;
	tag_player[1] = false;
	isReplaySwapped = false;
	lp[0] = 0;
	lp[1] = 0;
	start_lp = 0;
	duel_rule = 0;
	turn = 0;
	curMsg = 0;
	hostname[0] = 0;
	clientname[0] = 0;
	hostname_tag[0] = 0;
	clientname_tag[0] = 0;
	strLP[0][0] = 0;
	strLP[1][0] = 0;
	vic_string = 0;
	player_type = 0;
	time_player = 0;
	time_limit = 0;
	time_left[0] = 0;
	time_left[1] = 0;
}

bool Game::Initialize() {
	srand(time(0));
	initUtils();
	LoadConfig();
	irr::SIrrlichtCreationParameters params = irr::SIrrlichtCreationParameters();
	params.AntiAlias = gameConf.antialias;
	PRO_VERSION = gameConf.game_version;
	if(gameConf.use_d3d)
		params.DriverType = irr::video::EDT_DIRECT3D9;
	else
		params.DriverType = irr::video::EDT_OPENGL;
	params.WindowSize = irr::core::dimension2d<u32>(gameConf.window_width, gameConf.window_height);
	device = irr::createDeviceEx(params);
	if(!device) {
		ErrorLog("Failed to create Irrlicht Engine device!");
		return false;
	}
	linePatternD3D = 0;
	linePatternGL = 0x0f0f;
	waitFrame = 0;
	signalFrame = 0;
	showcard = 0;
	is_attacking = false;
	lpframe = 0;
	lpcstring = 0;
	always_chain = false;
	ignore_chain = false;
	chain_when_avail = false;
	is_building = false;
	menuHandler.prev_operation = 0;
	menuHandler.prev_sel = -1;
	deckBuilder.prev_operation = 0;
	deckBuilder.prev_sel = -1;
	memset(&dInfo, 0, sizeof(DuelInfo));
	memset(chatTiming, 0, sizeof(chatTiming));
	deckManager.LoadLFList();
	driver = device->getVideoDriver();
	driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
	driver->setTextureCreationFlag(irr::video::ETCF_OPTIMIZED_FOR_QUALITY, true);
	imageManager.SetDevice(device);
	if(!imageManager.Initial()) {
		ErrorLog("Failed to load textures!");
		return false;
	}
	dataManager.FileSystem = device->getFileSystem();
	LoadExpansions();
	if(!dataManager.LoadDB(L"cards.cdb")) {
		ErrorLog("Failed to load card database (cards.cdb)!");
		return false;
	}
	if(!dataManager.LoadStrings("strings.conf")) {
		ErrorLog("Failed to load strings!");
		return false;
	}
	dataManager.LoadStrings("./expansions/strings.conf");
	env = device->getGUIEnvironment();
	numFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, 16);
	adFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, 12);
	lpcFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, 48);
	guiFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.textfont, gameConf.textfontsize);
	textFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.textfont, gameConf.textfontsize);
	if(!numFont || !textFont) {
		ErrorLog("Failed to load font(s)!");
		return false;
	}
	smgr = device->getSceneManager();
	device->setWindowCaption(L"YGO-VI-Rook");
	device->setResizable(true);
	if(gameConf.window_maximized)
		device->maximizeWindow();
#ifdef _WIN32
	irr::video::SExposedVideoData exposedData = driver->getExposedVideoData();
	if(gameConf.use_d3d)
		hWnd = reinterpret_cast<HWND>(exposedData.D3D9.HWnd);
	else
		hWnd = reinterpret_cast<HWND>(exposedData.OpenGLWin32.HWnd);
	/*if(hWnd) {
		LONG style = GetWindowLong(hWnd, GWL_STYLE);
		style |= WS_MINIMIZEBOX;
		SetWindowLong(hWnd, GWL_STYLE, style);
		SendMessage(hWnd, WM_NCPAINT, 1, 0);
	}*/
#endif
	SetWindowsIcon();
	//main menu
	wchar_t strbuf[256];
	myswprintf(strbuf, L"YGO-VI-Rook Version:%X.0%X.%X", PRO_VERSION >> 12, (PRO_VERSION >> 4) & 0xff, PRO_VERSION & 0xf);
	wMainMenu = env->addWindow(rect<s32>(370, 200, 650, 485), false, strbuf);
	wMainMenu->getCloseButton()->setVisible(false);
	wMainMenu->setDraggable(false);
	btnLanMode = env->addButton(rect<s32>(10, 30, 270, 60), wMainMenu, BUTTON_LAN_MODE, dataManager.GetSysString(1200));
	btnSingleMode = env->addButton(rect<s32>(10, 65, 270, 95), wMainMenu, BUTTON_SINGLE_MODE, dataManager.GetSysString(1201));
	btnReplayMode = env->addButton(rect<s32>(10, 100, 270, 130), wMainMenu, BUTTON_REPLAY_MODE, dataManager.GetSysString(1202));
	btnDeckEdit = env->addButton(rect<s32>(10, 135, 270, 165), wMainMenu, BUTTON_DECK_EDIT, dataManager.GetSysString(1204));
	btnModeExit = env->addButton(rect<s32>(10, 240, 270, 270), wMainMenu, BUTTON_MODE_EXIT, dataManager.GetSysString(1210));
	btnOther = env->addButton(rect<s32>(10, 170, 270, 200), wMainMenu, BUTTON_OTHER, dataManager.GetSysString(1422));
	btnSYS = env->addButton(rect<s32>(10, 205, 270, 235), wMainMenu, BUTTON_SYS, dataManager.GetSysString(1207));
	//other
	wOther = env->addWindow(rect<s32>(370, 185, 650, 430), false, dataManager.GetSysString(1422));
	wOther->getCloseButton()->setVisible(false);
	wOther->setVisible(false);
	wOther->setDraggable(false);
	btnYST = env->addButton(rect<s32>(10, 30, 270, 60), wOther, BUTTON_YST, dataManager.GetSysString(1423));
	btnKANabell = env->addButton(rect<s32>(10, 65, 270, 95), wOther, BUTTON_KANABELL, dataManager.GetSysString(1424));
	btnOCNK = env->addButton(rect<s32>(10, 100, 270, 130), wOther, BUTTON_OCNK, dataManager.GetSysString(1425));
	btnGIT = env->addButton(rect<s32>(10, 135, 270, 165), wOther, BUTTON_GIT, dataManager.GetSysString(1426));
	btnDV = env->addButton(rect<s32>(10, 170, 270, 200), wOther, BUTTON_DV, dataManager.GetSysString(1288));
	btnOtherExit = env->addButton(rect<s32>(10, 205, 270, 235), wOther, BUTTON_OTHER_EXIT, dataManager.GetSysString(1210));
	//system setting
	wSystem = env->addWindow(rect<s32>(215, 160, 815, 365), false, dataManager.GetSysString(1207));
	wSystem->getCloseButton()->setVisible(false);
	wSystem->setVisible(false);
	wSystem->setDraggable(false);
	chkAutoSearch = env->addCheckBox(false, rect<s32>(30, 25, 260, 50), wSystem, CHECKBOX_AUTO_SEARCH, dataManager.GetSysString(1358));
	chkAutoSearch->setChecked(gameConf.auto_search_limit >= 0);
	chkMRandom = env->addCheckBox(false, rect<s32>(30, 55, 260, 80), wSystem, CHECKBOX_RDM, dataManager.GetSysString(1437));
	chkMRandom->setChecked(gameConf.random != 0);
	chkD3D = env->addCheckBox(false, rect<s32>(30, 85, 260, 110), wSystem, CHECKBOX_D3D, dataManager.GetSysString(1205));
	chkD3D->setChecked(gameConf.use_d3d != 0);
	chkMultiKeywords = env->addCheckBox(false, rect<s32>(270, 25, 430, 50), wSystem, CHECKBOX_MULTI_KEYWORDS, dataManager.GetSysString(1378));
	chkMultiKeywords->setChecked(gameConf.search_multiple_keywords > 0);
	chkRegex = env->addCheckBox(false, rect<s32>(270, 55, 430, 80), wSystem, CHECKBOX_REGEX, dataManager.GetSysString(1379));
	chkRegex->setChecked(gameConf.search_regex > 0);
	env->addStaticText(dataManager.GetSysString(1206), rect<s32>(270, 88, 426, 113), false, false, wSystem);
	cbFont = env->addComboBox(rect<s32>(427, 85, 590, 110), wSystem, COMBOBOX_FONT);
	btnHeadS = env->addButton(rect<s32>(30, 120, 200, 150), wSystem, BUTTON_HDS, dataManager.GetSysString(1439));
	btnCoverS = env->addButton(rect<s32>(215, 120, 385, 150), wSystem, BUTTON_CRS, dataManager.GetSysString(1441));
	btnBgS = env->addButton(rect<s32>(400, 120, 570, 150), wSystem, BUTTON_BGS, dataManager.GetSysString(1447));
	btnSystemExit = env->addButton(rect<s32>(200, 160, 400, 190), wSystem, BUTTON_SYS_EXIT, dataManager.GetSysString(1210));
	RefreshFont();
	//Head Select
	wHDS = env->addWindow(rect<s32>(342, 235, 682, 395), false, dataManager.GetSysString(1440));
	wHDS->getCloseButton()->setVisible(false);
	wHDS->setVisible(false);
	wHDS->setDraggable(false);
	cbHDS = env->addComboBox(rect<s32>(20, 40, 200, 85), wHDS, COMBOBOX_HDS);
	cbHDS->setMaxSelectionRows(10);
	imgHead = env->addImage(rect<s32>(210, 25, 330, 145), wHDS);
	imgHead->setImage(imageManager.tHead[0]);	
	imgHead->setScaleImage(true);
	imgHead->setUseAlphaChannel(true);
	btnHDSOK = env->addButton(rect<s32>(20, 105, 105, 135), wHDS, BUTTON_HDS_OK, dataManager.GetSysString(1211));
	btnHDSExit = env->addButton(rect<s32>(115, 105, 200, 135), wHDS, BUTTON_HDS_EXIT, dataManager.GetSysString(1210));
	RefreshHDS();
	//Cover Select
	wCRS = env->addWindow(rect<s32>(269.5, 187.5, 774.5, 452.5), false, dataManager.GetSysString(1442));
	wCRS->getCloseButton()->setVisible(false);
	wCRS->setVisible(false);
	wCRS->setDraggable(false);
	cbCRS = env->addComboBox(rect<s32>(170, 182.5, 300, 227.5), wCRS, COMBOBOX_CRS);
	cbCRS->setMaxSelectionRows(10);
	imgCover = env->addImage(rect<s32>(320, 20, 490, 250), wCRS);
	imgCover->setImage(imageManager.tCover[0]);	
	imgCover->setScaleImage(true);
	imgCover->setUseAlphaChannel(true);
	btnCoverOK1 = env->addButton(rect<s32>(30, 47.5, 160, 92.5), wCRS, BUTTON_CRS_OK1, dataManager.GetSysString(1443));
	btnCoverOK2 = env->addButton(rect<s32>(170, 47.5, 300, 92.5), wCRS, BUTTON_CRS_OK2, dataManager.GetSysString(1444));
	btnCoverOK3 = env->addButton(rect<s32>(30, 117.5, 160, 162.5), wCRS, BUTTON_CRS_OK3, dataManager.GetSysString(1445));
	btnCoverOK4 = env->addButton(rect<s32>(170, 117.5, 300, 162.5), wCRS, BUTTON_CRS_OK4, dataManager.GetSysString(1446));
	btnCRSExit = env->addButton(rect<s32>(30, 182.5, 160, 222.5), wCRS, BUTTON_CRS_EXIT, dataManager.GetSysString(1210));
	RefreshCRS();
	//Background Select
	wBGS = env->addWindow(rect<s32>(132, 100, 882, 500), false, dataManager.GetSysString(1448));
	wBGS->getCloseButton()->setVisible(false);
	wBGS->setVisible(false);
	wBGS->setDraggable(false);
	cbBGS = env->addComboBox(rect<s32>(25, 40, 170, 90), wBGS, COMBOBOX_BGS);
	cbBGS->setMaxSelectionRows(10);
	imgBG = env->addImage(rect<s32>(180, 25, 732, 385), wBGS);
	imgBG->setImage(imageManager.tBackGround);	
	imgBG->setScaleImage(true);
	imgBG->setUseAlphaChannel(true);
	btnBGOK1 = env->addButton(rect<s32>(25, 110, 170, 160), wBGS, BUTTON_BGS_OK1, dataManager.GetSysString(1449));
	btnBGOK2 = env->addButton(rect<s32>(25, 180, 170, 230), wBGS, BUTTON_BGS_OK2, dataManager.GetSysString(1450));
	btnBGOK3 = env->addButton(rect<s32>(25, 250, 170, 300), wBGS, BUTTON_BGS_OK3, dataManager.GetSysString(1451));
	btnBGSExit = env->addButton(rect<s32>(25, 320, 170, 365), wBGS, BUTTON_BGS_EXIT, dataManager.GetSysString(1210));
	RefreshBGS();
	//fortune
	wDV = env->addWindow(rect<s32>(35, 70, 635, 770), false, L"");
	wDV->getCloseButton()->setVisible(false);
	wDV->setDrawTitlebar(false);
	wDV->setDrawBackground(false);
	wDV->setVisible(false);
	imgDV = env->addImage(rect<s32>(0, 0, 430, 625), wDV);
	imgDV->setImage(imageManager.tDV);
	imgDV->setScaleImage(false);
	imgDV->setUseAlphaChannel(true);
	btnDVClose = env->addButton(rect<s32>(65, 630, 365, 670), wDV, BUTTON_DV_CLOSE, dataManager.GetSysString(1210));
	//lan mode
	wLanWindow = env->addWindow(rect<s32>(220, 100, 800, 520), false, dataManager.GetSysString(1200));
	wLanWindow->getCloseButton()->setVisible(false);
	wLanWindow->setVisible(false);
	wLanWindow->setDraggable(false);
	env->addStaticText(dataManager.GetSysString(1220), rect<s32>(10, 30, 220, 50), false, false, wLanWindow);
	ebNickName = env->addEditBox(gameConf.nickname, rect<s32>(110, 25, 450, 50), true, wLanWindow);
	ebNickName->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	lstHostList = env->addListBox(rect<s32>(10, 60, 570, 320), wLanWindow, LISTBOX_LAN_HOST, true);
	lstHostList->setItemHeight(18);
	btnLanRefresh = env->addButton(rect<s32>(100, 325, 200, 350), wLanWindow, BUTTON_LAN_REFRESH, dataManager.GetSysString(1217));
	env->addStaticText(dataManager.GetSysString(1221), rect<s32>(10, 360, 220, 380), false, false, wLanWindow);
	btnSC = env->addButton(rect<s32>(240, 325, 340, 350), wLanWindow, BUTTON_SERVER_CHOOSE, dataManager.GetSysString(1427));
	btnRM = env->addButton(rect<s32>(380, 325, 480, 350), wLanWindow, BUTTON_ROOM_CODE, dataManager.GetSysString(1513));
	ebJoinHost = env->addEditBox(gameConf.lasthost, rect<s32>(110, 355, 350, 380), true, wLanWindow);
	ebJoinHost->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	ebJoinPort = env->addEditBox(gameConf.lastport, rect<s32>(360, 355, 420, 380), true, wLanWindow);
	ebJoinPort->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1222), rect<s32>(10, 390, 220, 410), false, false, wLanWindow);
	ebJoinPass = env->addEditBox(gameConf.roompass, rect<s32>(110, 385, 420, 410), true, wLanWindow);
	ebJoinPass->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnJoinHost = env->addButton(rect<s32>(460, 355, 570, 380), wLanWindow, BUTTON_JOIN_HOST, dataManager.GetSysString(1223));
	btnJoinCancel = env->addButton(rect<s32>(460, 385, 570, 410), wLanWindow, BUTTON_JOIN_CANCEL, dataManager.GetSysString(1212));
	btnCreateHost = env->addButton(rect<s32>(460, 25, 570, 50), wLanWindow, BUTTON_CREATE_HOST, dataManager.GetSysString(1224));
	//server choose
	wSC = env->addWindow(rect<s32>(-60, 80, 255, 437), false, dataManager.GetSysString(1428));
	wSC->getCloseButton()->setVisible(false);
	wSC->setVisible(false);
	wSC->setDraggable(false);
	btn233 = env->addButton(rect<s32>(10, 30, 270, 60), wSC, BUTTON_233, dataManager.GetSysString(1429));
	btn23333 = env->addButton(rect<s32>(10, 65, 270, 95), wSC, BUTTON_23333, dataManager.GetSysString(1430));
	btn888 = env->addButton(rect<s32>(10, 310, 270, 340), wSC, BUTTON_888, dataManager.GetSysString(1626));
	btnJP = env->addButton(rect<s32>(10, 205, 270, 235), wSC, BUTTON_JP, dataManager.GetSysString(1434));
	btnSP = env->addButton(rect<s32>(10, 275, 270, 305), wSC, BUTTON_SP, dataManager.GetSysString(1625));
	btn7210 = env->addButton(rect<s32>(10, 100, 270, 130), wSC, BUTTON_7210, dataManager.GetSysString(1431));
	btn222 = env->addButton(rect<s32>(10, 135, 270, 165), wSC, BUTTON_222, dataManager.GetSysString(1432));
	btn765 = env->addButton(rect<s32>(10, 170, 270, 200), wSC, BUTTON_765, dataManager.GetSysString(1433));
	btnNK = env->addButton(rect<s32>(10, 240, 270, 270), wSC, BUTTON_NK, dataManager.GetSysString(1435));	
	btnSCExit = env->addButton(rect<s32>(10, 345, 270, 375), wSC, BUTTON_SC_EXIT, dataManager.GetSysString(1436));
	//room code
	wRM = env->addWindow(rect<s32>(270, 65, 700, 425), false, dataManager.GetSysString(1513));
	wRM->getCloseButton()->setVisible(false);
	wRM->setVisible(false);
	wRM->setDraggable(false);
	btnRM1 = env->addButton(rect<s32>(10, 30, 210, 60), wRM, BUTTON_RM1, dataManager.GetSysString(1514));
	btnRM2 = env->addButton(rect<s32>(220, 30, 420, 60), wRM, BUTTON_RM2, dataManager.GetSysString(1515));
	btnRM3 = env->addButton(rect<s32>(10, 65, 210, 95), wRM, BUTTON_RM3, dataManager.GetSysString(1516));
	btnRM4 = env->addButton(rect<s32>(220, 65, 420, 95), wRM, BUTTON_RM4, dataManager.GetSysString(1517));
	btnRM5 = env->addButton(rect<s32>(10, 100, 210, 130), wRM, BUTTON_RM5, dataManager.GetSysString(1518));
	btnRM6 = env->addButton(rect<s32>(220, 100, 420, 130), wRM, BUTTON_RM6, dataManager.GetSysString(1519));
	btnRM7 = env->addButton(rect<s32>(10, 135, 210, 165), wRM, BUTTON_RM7, dataManager.GetSysString(1520));
	btnRM8 = env->addButton(rect<s32>(220, 135, 420, 165), wRM, BUTTON_RM8, dataManager.GetSysString(1521));
	btnRM9 = env->addButton(rect<s32>(10, 170, 210, 200), wRM, BUTTON_RM9, dataManager.GetSysString(1522));
	btnRM10 = env->addButton(rect<s32>(220, 170, 420, 200), wRM, BUTTON_RM10, dataManager.GetSysString(1523));
	btnRM11 = env->addButton(rect<s32>(10, 205, 210, 235), wRM, BUTTON_RM11, dataManager.GetSysString(1524));
	btnRM12 = env->addButton(rect<s32>(220, 205, 420, 235), wRM, BUTTON_RM12, dataManager.GetSysString(1525));
	btnRM13 = env->addButton(rect<s32>(10, 240, 210, 270), wRM, BUTTON_RM13, dataManager.GetSysString(1526));
	btnRM14 = env->addButton(rect<s32>(220, 240, 420, 270), wRM, BUTTON_RM14, dataManager.GetSysString(1527));
	btnRMCM = env->addButton(rect<s32>(30, 285, 90, 345), wRM, BUTTON_RMCM, dataManager.GetSysString(1528));
	btnRMWN = env->addButton(rect<s32>(120, 285, 180, 345), wRM, BUTTON_RMWN, dataManager.GetSysString(1529));
	btnRMAI = env->addButton(rect<s32>(210, 280, 310, 310), wRM, BUTTON_RMAI, dataManager.GetSysString(1530));
	btnRMA = env->addButton(rect<s32>(320, 280, 420, 310), wRM, BUTTON_RMA, dataManager.GetSysString(1531));
	btnRMExit = env->addButton(rect<s32>(250, 320, 380, 350), wRM, BUTTON_RM_EXIT, dataManager.GetSysString(1436));
	//create host
	wCreateHost = env->addWindow(rect<s32>(320, 100, 700, 520), false, dataManager.GetSysString(1224));
	wCreateHost->getCloseButton()->setVisible(false);
	wCreateHost->setVisible(false);
	wCreateHost->setDraggable(false);
	env->addStaticText(dataManager.GetSysString(1226), rect<s32>(20, 30, 220, 50), false, false, wCreateHost);
	cbLFlist = env->addComboBox(rect<s32>(140, 25, 300, 50), wCreateHost);
	for(unsigned int i = 0; i < deckManager._lfList.size(); ++i)
		cbLFlist->addItem(deckManager._lfList[i].listName.c_str(), deckManager._lfList[i].hash);
	env->addStaticText(dataManager.GetSysString(1225), rect<s32>(20, 60, 220, 80), false, false, wCreateHost);
	cbRule = env->addComboBox(rect<s32>(140, 55, 300, 80), wCreateHost);
	cbRule->addItem(dataManager.GetSysString(1240));
	cbRule->addItem(dataManager.GetSysString(1241));
	cbRule->addItem(dataManager.GetSysString(1242));
	cbRule->addItem(dataManager.GetSysString(1243));
	cbRule->setSelected(gameConf.defaultOT - 1);
	env->addStaticText(dataManager.GetSysString(1227), rect<s32>(20, 90, 220, 110), false, false, wCreateHost);
	cbMatchMode = env->addComboBox(rect<s32>(140, 85, 300, 110), wCreateHost);
	cbMatchMode->addItem(dataManager.GetSysString(1244));
	cbMatchMode->addItem(dataManager.GetSysString(1245));
	cbMatchMode->addItem(dataManager.GetSysString(1246));
	env->addStaticText(dataManager.GetSysString(1237), rect<s32>(20, 120, 320, 140), false, false, wCreateHost);
	myswprintf(strbuf, L"%d", 180);
	ebTimeLimit = env->addEditBox(strbuf, rect<s32>(140, 115, 220, 140), true, wCreateHost);
	ebTimeLimit->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1228), rect<s32>(20, 150, 320, 170), false, false, wCreateHost);
	env->addStaticText(dataManager.GetSysString(1236), rect<s32>(20, 180, 220, 200), false, false, wCreateHost);
	cbDuelRule = env->addComboBox(rect<s32>(140, 175, 300, 200), wCreateHost);
	cbDuelRule->addItem(dataManager.GetSysString(1260));
	cbDuelRule->addItem(dataManager.GetSysString(1261));
	cbDuelRule->addItem(dataManager.GetSysString(1262));
	cbDuelRule->addItem(dataManager.GetSysString(1263));
	cbDuelRule->addItem(dataManager.GetSysString(1264));
	cbDuelRule->setSelected(gameConf.default_rule - 1);
	chkNoCheckDeck = env->addCheckBox(false, rect<s32>(20, 210, 170, 230), wCreateHost, -1, dataManager.GetSysString(1229));
	chkNoShuffleDeck = env->addCheckBox(false, rect<s32>(180, 210, 360, 230), wCreateHost, -1, dataManager.GetSysString(1230));
	env->addStaticText(dataManager.GetSysString(1231), rect<s32>(20, 240, 320, 260), false, false, wCreateHost);
	myswprintf(strbuf, L"%d", 8000);
	ebStartLP = env->addEditBox(strbuf, rect<s32>(140, 235, 220, 260), true, wCreateHost);
	ebStartLP->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1232), rect<s32>(20, 270, 320, 290), false, false, wCreateHost);
	myswprintf(strbuf, L"%d", 5);
	ebStartHand = env->addEditBox(strbuf, rect<s32>(140, 265, 220, 290), true, wCreateHost);
	ebStartHand->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1233), rect<s32>(20, 300, 320, 320), false, false, wCreateHost);
	myswprintf(strbuf, L"%d", 1);
	ebDrawCount = env->addEditBox(strbuf, rect<s32>(140, 295, 220, 320), true, wCreateHost);
	ebDrawCount->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1234), rect<s32>(10, 360, 220, 380), false, false, wCreateHost);
	ebServerName = env->addEditBox(gameConf.gamename, rect<s32>(110, 355, 250, 380), true, wCreateHost);
	ebServerName->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	env->addStaticText(dataManager.GetSysString(1235), rect<s32>(10, 390, 220, 410), false, false, wCreateHost);
	ebServerPass = env->addEditBox(L"", rect<s32>(110, 385, 250, 410), true, wCreateHost);
	ebServerPass->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnHostConfirm = env->addButton(rect<s32>(260, 355, 370, 380), wCreateHost, BUTTON_HOST_CONFIRM, dataManager.GetSysString(1211));
	btnHostCancel = env->addButton(rect<s32>(260, 385, 370, 410), wCreateHost, BUTTON_HOST_CANCEL, dataManager.GetSysString(1212));
	//host(single)
	wHostPrepare = env->addWindow(rect<s32>(270, 120, 750, 440), false, dataManager.GetSysString(1250));
	wHostPrepare->getCloseButton()->setVisible(false);
	wHostPrepare->setVisible(false);
	wHostPrepare->setDraggable(false);
	btnHostPrepDuelist = env->addButton(rect<s32>(10, 30, 110, 55), wHostPrepare, BUTTON_HP_DUELIST, dataManager.GetSysString(1251));
	for(int i = 0; i < 2; ++i) {
		stHostPrepDuelist[i] = env->addStaticText(L"", rect<s32>(40, 65 + i * 25, 240, 85 + i * 25), true, false, wHostPrepare);
		btnHostPrepKick[i] = env->addButton(rect<s32>(10, 65 + i * 25, 30, 85 + i * 25), wHostPrepare, BUTTON_HP_KICK, L"X");
		chkHostPrepReady[i] = env->addCheckBox(false, rect<s32>(250, 65 + i * 25, 270, 85 + i * 25), wHostPrepare, CHECKBOX_HP_READY, L"");
		chkHostPrepReady[i]->setEnabled(false);
	}
	for(int i = 2; i < 4; ++i) {
		stHostPrepDuelist[i] = env->addStaticText(L"", rect<s32>(40, 75 + i * 25, 240, 95 + i * 25), true, false, wHostPrepare);
		btnHostPrepKick[i] = env->addButton(rect<s32>(10, 75 + i * 25, 30, 95 + i * 25), wHostPrepare, BUTTON_HP_KICK, L"X");
		chkHostPrepReady[i] = env->addCheckBox(false, rect<s32>(250, 75 + i * 25, 270, 95 + i * 25), wHostPrepare, CHECKBOX_HP_READY, L"");
		chkHostPrepReady[i]->setEnabled(false);
	}
	btnHostPrepOB = env->addButton(rect<s32>(10, 180, 110, 205), wHostPrepare, BUTTON_HP_OBSERVER, dataManager.GetSysString(1252));
	myswprintf(strbuf, L"%ls%d", dataManager.GetSysString(1253), 0);
	stHostPrepOB = env->addStaticText(strbuf, rect<s32>(10, 210, 270, 230), false, false, wHostPrepare);
	stHostPrepRule = env->addStaticText(L"", rect<s32>(280, 30, 460, 230), false, true, wHostPrepare);
	env->addStaticText(dataManager.GetSysString(1254), rect<s32>(10, 235, 110, 255), false, false, wHostPrepare);
	cbDeckSelect = env->addComboBox(rect<s32>(120, 230, 270, 255), wHostPrepare);
	cbDeckSelect->setMaxSelectionRows(10);
	btnHostPrepReady = env->addButton(rect<s32>(170, 180, 270, 205), wHostPrepare, BUTTON_HP_READY, dataManager.GetSysString(1218));
	btnHostPrepNotReady = env->addButton(rect<s32>(170, 180, 270, 205), wHostPrepare, BUTTON_HP_NOTREADY, dataManager.GetSysString(1219));
	btnHostPrepNotReady->setVisible(false);
	btnHostPrepStart = env->addButton(rect<s32>(230, 280, 340, 305), wHostPrepare, BUTTON_HP_START, dataManager.GetSysString(1215));
	btnHostPrepCancel = env->addButton(rect<s32>(350, 280, 460, 305), wHostPrepare, BUTTON_HP_CANCEL, dataManager.GetSysString(1210));
	//img
	wCardImg = env->addStaticText(L"", rect<s32>(1, 1, 1 + CARD_IMG_WIDTH + 20, 1 + CARD_IMG_HEIGHT + 18), true, false, 0, -1, true);
	wCardImg->setBackgroundColor(0xc0c0c0c0);
	wCardImg->setVisible(false);
	imgCard = env->addImage(rect<s32>(10, 9, 10 + CARD_IMG_WIDTH, 9 + CARD_IMG_HEIGHT), wCardImg);
	imgCard->setImage(imageManager.tCover[0]);
	showingcode = 0;
	imgCard->setScaleImage(true);
	imgCard->setUseAlphaChannel(true);
	//phase
	wPhase = env->addStaticText(L"", rect<s32>(480, 310, 855, 330));
	wPhase->setVisible(false);
	btnPhaseStatus = env->addButton(rect<s32>(0, 0, 50, 20), wPhase, BUTTON_PHASE, L"");
	btnPhaseStatus->setIsPushButton(true);
	btnPhaseStatus->setPressed(true);
	btnPhaseStatus->setVisible(false);
	btnBP = env->addButton(rect<s32>(160, 0, 210, 20), wPhase, BUTTON_BP, L"\xff22\xff30");
	btnBP->setVisible(false);
	btnM2 = env->addButton(rect<s32>(160, 0, 210, 20), wPhase, BUTTON_M2, L"\xff2d\xff12");
	btnM2->setVisible(false);
	btnEP = env->addButton(rect<s32>(320, 0, 370, 20), wPhase, BUTTON_EP, L"\xff25\xff30");
	btnEP->setVisible(false);
	//tab
	wInfos = env->addTabControl(rect<s32>(1, 275, 301, 639), 0, true);
	wInfos->setTabExtraWidth(16);
	wInfos->setVisible(false);
	//info
	irr::gui::IGUITab* tabInfo = wInfos->addTab(dataManager.GetSysString(1270));
	stName = env->addStaticText(L"", rect<s32>(10, 10, 287, 32), true, false, tabInfo, -1, false);
	stName->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stInfo = env->addStaticText(L"", rect<s32>(15, 37, 296, 60), false, true, tabInfo, -1, false);
	stInfo->setOverrideColor(SColor(255, 0, 50, 0));
	stDataInfo = env->addStaticText(L"", rect<s32>(15, 60, 296, 83), false, true, tabInfo, -1, false);
	stDataInfo->setOverrideColor(SColor(255, 197, 16, 0));
	stSetName = env->addStaticText(L"", rect<s32>(15, 83, 296, 106), false, true, tabInfo, -1, false);
	stSetName->setOverrideColor(SColor(255, 11, 35, 78));
	stText = env->addStaticText(L"", rect<s32>(15, 106, 287, 324), false, true, tabInfo, -1, false);
	scrCardText = env->addScrollBar(false, rect<s32>(267, 106, 287, 324), tabInfo, SCROLL_CARDTEXT);
	scrCardText->setLargeStep(1);
	scrCardText->setSmallStep(1);
	scrCardText->setVisible(false);
	//log
	irr::gui::IGUITab* tabLog = wInfos->addTab(dataManager.GetSysString(1271));
	lstLog = env->addListBox(rect<s32>(10, 10, 290, 290), tabLog, LISTBOX_LOG, false);
	lstLog->setItemHeight(18);
	btnClearLog = env->addButton(rect<s32>(160, 300, 260, 325), tabLog, BUTTON_CLEAR_LOG, dataManager.GetSysString(1272));
	//helper
	irr::gui::IGUITab* _tabHelper = wInfos->addTab(dataManager.GetSysString(1298));
	_tabHelper->setRelativePosition(recti(16, 49, 299, 362));
	tabHelper = env->addWindow(recti(0, 0, 250, 300), false, L"", _tabHelper);
	tabHelper->setDrawTitlebar(false);
	tabHelper->getCloseButton()->setVisible(false);
	tabHelper->setDrawBackground(false);
	tabHelper->setDraggable(false);
	scrTabHelper = env->addScrollBar(false, rect<s32>(252, 0, 272, 300), _tabHelper, SCROLL_TAB_HELPER);
	scrTabHelper->setLargeStep(1);
	scrTabHelper->setSmallStep(1);
	scrTabHelper->setVisible(false);
	int posX = 0;
	int posY = 0;
	chkMAutoPos = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1274));
	chkMAutoPos->setChecked(gameConf.chkMAutoPos != 0);
	posY += 30;
	chkSTAutoPos = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1278));
	chkSTAutoPos->setChecked(gameConf.chkSTAutoPos != 0);
	posY += 30;
	chkRandomPos = env->addCheckBox(false, rect<s32>(posX + 20, posY, posX + 20 + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1275));
	chkRandomPos->setChecked(gameConf.chkRandomPos != 0);
	posY += 30;
	chkAutoChain = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1276));
	chkAutoChain->setChecked(gameConf.chkAutoChain != 0);
	posY += 30;
	chkWaitChain = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1277));
	chkWaitChain->setChecked(gameConf.chkWaitChain != 0);
	posY += 30;
	chkQuickAnimation = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, CHECKBOX_QUICK_ANIMATION, dataManager.GetSysString(1299));
	chkQuickAnimation->setChecked(gameConf.quick_animation != 0);
	posY += 30;
	chkAskMSet = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, CHECKBOX_ASK_MSET, dataManager.GetSysString(1268));
	chkAskMSet->setChecked(gameConf.ask_mset != 0);
	posY += 30;
	chkAutoSaveReplay = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabHelper, -1, dataManager.GetSysString(1366));
	chkAutoSaveReplay->setChecked(gameConf.auto_save_replay != 0);
	elmTabHelperLast = chkRegex;
	//system
	irr::gui::IGUITab* _tabSystem = wInfos->addTab(dataManager.GetSysString(1273));
	_tabSystem->setRelativePosition(recti(16, 49, 299, 362));
	tabSystem = env->addWindow(recti(0, 0, 250, 300), false, L"", _tabSystem);
	tabSystem->setDrawTitlebar(false);
	tabSystem->getCloseButton()->setVisible(false);
	tabSystem->setDrawBackground(false);
	tabSystem->setDraggable(false);
	scrTabSystem = env->addScrollBar(false, rect<s32>(252, 0, 272, 300), _tabSystem, SCROLL_TAB_SYSTEM);
	scrTabSystem->setLargeStep(1);
	scrTabSystem->setSmallStep(1);
	scrTabSystem->setVisible(false);
	posY = 0;
	chkIgnore1 = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, CHECKBOX_DISABLE_CHAT, dataManager.GetSysString(1290));
	chkIgnore1->setChecked(gameConf.chkIgnore1 != 0);
	posY += 30;
	chkIgnore2 = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1291));
	chkIgnore2->setChecked(gameConf.chkIgnore2 != 0);
	posY += 30;
	chkHideSetname = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1354));
	chkHideSetname->setChecked(gameConf.chkHideSetname != 0);
	posY += 30;
	chkHideHintButton = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1355));
	chkHideHintButton->setChecked(gameConf.chkHideHintButton != 0);
	posY += 30;
	chkIgnoreDeckChanges = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1357));
	chkIgnoreDeckChanges->setChecked(gameConf.chkIgnoreDeckChanges != 0);
	posY += 30;
	chkEnablePScale = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1282));
	chkEnablePScale->setChecked(gameConf.chkEnablePScale != 0);
	posY += 30;
	chkEnableSound = env->addCheckBox(gameConf.enable_sound, rect<s32>(posX, posY, posX + 120, posY + 25), tabSystem, CHECKBOX_ENABLE_SOUND, dataManager.GetSysString(1279));
	chkEnableSound->setChecked(gameConf.enable_sound);
	scrSoundVolume = env->addScrollBar(true, rect<s32>(posX + 116, posY + 4, posX + 250, posY + 21), tabSystem, SCROLL_VOLUME);
	scrSoundVolume->setMax(100);
	scrSoundVolume->setMin(0);
	scrSoundVolume->setPos(gameConf.sound_volume * 100);
	scrSoundVolume->setLargeStep(1);
	scrSoundVolume->setSmallStep(1);
	posY += 30;
	chkEnableMusic = env->addCheckBox(gameConf.enable_music, rect<s32>(posX, posY, posX + 120, posY + 25), tabSystem, CHECKBOX_ENABLE_MUSIC, dataManager.GetSysString(1280));
	chkEnableMusic->setChecked(gameConf.enable_music);
	scrMusicVolume = env->addScrollBar(true, rect<s32>(posX + 116, posY + 4, posX + 250, posY + 21), tabSystem, SCROLL_VOLUME);
	scrMusicVolume->setMax(100);
	scrMusicVolume->setMin(0);
	scrMusicVolume->setPos(gameConf.music_volume * 100);
	scrMusicVolume->setLargeStep(1);
	scrMusicVolume->setSmallStep(1);
	posY += 30;
	chkMusicMode = env->addCheckBox(false, rect<s32>(posX, posY, posX + 260, posY + 25), tabSystem, -1, dataManager.GetSysString(1281));
	chkMusicMode->setChecked(gameConf.music_mode != 0);
	btnCardSearch = env->addButton(rect<s32>(150, 20, 250, 70), tabSystem, BUTTON_CARD_SEARCH, dataManager.GetSysString(1283));
	wSearchWindow = env->addWindow(rect<s32>(510, 200, 820, 320), false, dataManager.GetSysString(1284));
	wSearchWindow->getCloseButton()->setVisible(false);
	wSearchWindow->setVisible(false);
	wSearchWindow->setDraggable(false);
	env->addStaticText(dataManager.GetSysString(1285), rect<s32>(20, 27, 130, 47), false, false, wSearchWindow);
	ebSearchName = env->addEditBox(gameConf.searchname, rect<s32>(20, 50, 290, 70), true, wSearchWindow);
	ebSearchName->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	btnSearchAgree = env->addButton(rect<s32>(70, 80, 140, 105), wSearchWindow, BUTTON_SEARCH_AGREE, dataManager.GetSysString(1286));
	btnSearchCancel = env->addButton(rect<s32>(170, 80, 240, 105), wSearchWindow, BUTTON_SEARCH_CANCEL, dataManager.GetSysString(1287));
	elmTabSystemLast = chkMusicMode;
	//
	wHand = env->addWindow(rect<s32>(500, 450, 825, 605), false, L"");
	wHand->getCloseButton()->setVisible(false);
	wHand->setDraggable(false);
	wHand->setDrawTitlebar(false);
	wHand->setVisible(false);
	for(int i = 0; i < 3; ++i) {
		btnHand[i] = env->addButton(rect<s32>(10 + 105 * i, 10, 105 + 105 * i, 144), wHand, BUTTON_HAND1 + i, L"");
		btnHand[i]->setImage(imageManager.tHand[i]);
	}
	//
	wFTSelect = env->addWindow(rect<s32>(550, 240, 780, 340), false, L"");
	wFTSelect->getCloseButton()->setVisible(false);
	wFTSelect->setVisible(false);
	btnFirst = env->addButton(rect<s32>(10, 30, 220, 55), wFTSelect, BUTTON_FIRST, dataManager.GetSysString(100));
	btnSecond = env->addButton(rect<s32>(10, 60, 220, 85), wFTSelect, BUTTON_SECOND, dataManager.GetSysString(101));
	//message (310)
	wMessage = env->addWindow(rect<s32>(490, 200, 840, 340), false, dataManager.GetSysString(1216));
	wMessage->getCloseButton()->setVisible(false);
	wMessage->setVisible(false);
	stMessage =  env->addStaticText(L"", rect<s32>(20, 20, 350, 100), false, true, wMessage, -1, false);
	stMessage->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	btnMsgOK = env->addButton(rect<s32>(130, 105, 220, 130), wMessage, BUTTON_MSG_OK, dataManager.GetSysString(1211));
	//auto fade message (310)
	wACMessage = env->addWindow(rect<s32>(490, 240, 840, 300), false, L"");
	wACMessage->getCloseButton()->setVisible(false);
	wACMessage->setVisible(false);
	wACMessage->setDrawBackground(false);
	stACMessage = env->addStaticText(L"", rect<s32>(0, 0, 350, 60), true, true, wACMessage, -1, true);
	stACMessage->setBackgroundColor(0xc0c0c0ff);
	stACMessage->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	//auto center message (310)
	wASMessage = env->addWindow(rect<s32>(337, 290, 687, 350), false, L"");
	wASMessage->getCloseButton()->setVisible(false);
	wASMessage->setVisible(false);
	wASMessage->setDrawBackground(false);
	stASMessage = env->addStaticText(L"", rect<s32>(0, 0, 350, 60), true, true, wASMessage, -1, true);
	stASMessage->setBackgroundColor(0xc0c0c0ff);
	stASMessage->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	//yes/no (310)
	wQuery = env->addWindow(rect<s32>(490, 200, 840, 340), false, dataManager.GetSysString(560));
	wQuery->getCloseButton()->setVisible(false);
	wQuery->setVisible(false);
	stQMessage =  env->addStaticText(L"", rect<s32>(20, 20, 350, 100), false, true, wQuery, -1, false);
	stQMessage->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	btnYes = env->addButton(rect<s32>(100, 105, 150, 130), wQuery, BUTTON_YES, dataManager.GetSysString(1213));
	btnNo = env->addButton(rect<s32>(200, 105, 250, 130), wQuery, BUTTON_NO, dataManager.GetSysString(1214));
	//surrender yes/no (310)
	wSurrender = env->addWindow(rect<s32>(490, 200, 840, 340), false, dataManager.GetSysString(560));
	wSurrender->getCloseButton()->setVisible(false);
	wSurrender->setVisible(false);
	stSurrenderMessage = env->addStaticText(dataManager.GetSysString(1359), rect<s32>(20, 20, 350, 100), false, true, wSurrender, -1, false);
	stSurrenderMessage->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	btnSurrenderYes = env->addButton(rect<s32>(100, 105, 150, 130), wSurrender, BUTTON_SURRENDER_YES, dataManager.GetSysString(1213));
	btnSurrenderNo = env->addButton(rect<s32>(200, 105, 250, 130), wSurrender, BUTTON_SURRENDER_NO, dataManager.GetSysString(1214));
	//options (310)
	wOptions = env->addWindow(rect<s32>(490, 200, 840, 340), false, L"");
	wOptions->getCloseButton()->setVisible(false);
	wOptions->setVisible(false);
	stOptions = env->addStaticText(L"", rect<s32>(20, 20, 350, 100), false, true, wOptions, -1, false);
	stOptions->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
	btnOptionOK = env->addButton(rect<s32>(130, 105, 220, 130), wOptions, BUTTON_OPTION_OK, dataManager.GetSysString(1211));
	btnOptionp = env->addButton(rect<s32>(20, 105, 60, 130), wOptions, BUTTON_OPTION_PREV, L"<<<");
	btnOptionn = env->addButton(rect<s32>(290, 105, 330, 130), wOptions, BUTTON_OPTION_NEXT, L">>>");
	//pos select
	wPosSelect = env->addWindow(rect<s32>(340, 200, 935, 410), false, dataManager.GetSysString(561));
	wPosSelect->getCloseButton()->setVisible(false);
	wPosSelect->setVisible(false);
	btnPSAU = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(10, 45, 150, 185), wPosSelect, BUTTON_POS_AU);
	btnPSAU->setImageScale(core::vector2df(0.5, 0.5));
	btnPSAD = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(155, 45, 295, 185), wPosSelect, BUTTON_POS_AD);
	btnPSAD->setImageScale(core::vector2df(0.5, 0.5));
	btnPSAD->setImage(imageManager.tCover[0], rect<s32>(0, 0, CARD_IMG_WIDTH, CARD_IMG_HEIGHT));
	btnPSDU = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(300, 45, 440, 185), wPosSelect, BUTTON_POS_DU);
	btnPSDU->setImageScale(core::vector2df(0.5, 0.5));
	btnPSDU->setImageRotation(270);
	btnPSDD = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(445, 45, 585, 185), wPosSelect, BUTTON_POS_DD);
	btnPSDD->setImageScale(core::vector2df(0.5, 0.5));
	btnPSDD->setImageRotation(270);
	btnPSDD->setImage(imageManager.tCover[0], rect<s32>(0, 0, CARD_IMG_WIDTH, CARD_IMG_HEIGHT));
	//card select
	wCardSelect = env->addWindow(rect<s32>(320, 100, 1000, 400), false, L"");
	wCardSelect->getCloseButton()->setVisible(false);
	wCardSelect->setVisible(false);
	for(int i = 0; i < 5; ++i) {
		stCardPos[i] = env->addStaticText(L"", rect<s32>(30 + 125 * i, 30, 150 + 125 * i, 50), true, false, wCardSelect, -1, true);
		stCardPos[i]->setBackgroundColor(0xffffffff);
		stCardPos[i]->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
		btnCardSelect[i] = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(30 + 125 * i, 55, 150 + 125 * i, 225), wCardSelect, BUTTON_CARD_0 + i);
		btnCardSelect[i]->setImageScale(core::vector2df(0.6f, 0.6f));
	}
	scrCardList = env->addScrollBar(true, rect<s32>(30, 235, 650, 255), wCardSelect, SCROLL_CARD_SELECT);
	btnSelectOK = env->addButton(rect<s32>(300, 265, 380, 290), wCardSelect, BUTTON_CARD_SEL_OK, dataManager.GetSysString(1211));
	//card display
	wCardDisplay = env->addWindow(rect<s32>(320, 100, 1000, 400), false, L"");
	wCardDisplay->getCloseButton()->setVisible(false);
	wCardDisplay->setVisible(false);
	for(int i = 0; i < 5; ++i) {
		stDisplayPos[i] = env->addStaticText(L"", rect<s32>(30 + 125 * i, 30, 150 + 125 * i, 50), true, false, wCardDisplay, -1, true);
		stDisplayPos[i]->setBackgroundColor(0xffffffff);
		stDisplayPos[i]->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
		btnCardDisplay[i] = irr::gui::CGUIImageButton::addImageButton(env, rect<s32>(30 + 125 * i, 55, 150 + 125 * i, 225), wCardDisplay, BUTTON_DISPLAY_0 + i);
		btnCardDisplay[i]->setImageScale(core::vector2df(0.6f, 0.6f));
	}
	scrDisplayList = env->addScrollBar(true, rect<s32>(30, 235, 650, 255), wCardDisplay, SCROLL_CARD_DISPLAY);
	btnDisplayOK = env->addButton(rect<s32>(300, 265, 380, 290), wCardDisplay, BUTTON_CARD_DISP_OK, dataManager.GetSysString(1211));
	//announce number
	wANNumber = env->addWindow(rect<s32>(550, 200, 780, 295), false, L"");
	wANNumber->getCloseButton()->setVisible(false);
	wANNumber->setVisible(false);
	wANNumber->setDraggable(false);
	cbANNumber =  env->addComboBox(rect<s32>(40, 30, 190, 50), wANNumber, -1);
	cbANNumber->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnANNumberOK = env->addButton(rect<s32>(80, 60, 150, 85), wANNumber, BUTTON_ANNUMBER_OK, dataManager.GetSysString(1211));
	//announce card
	wANCard = env->addWindow(rect<s32>(510, 120, 820, 420), false, L"");
	wANCard->getCloseButton()->setVisible(false);
	wANCard->setVisible(false);
	wANCard->setDraggable(false);
	ebANCard = env->addEditBox(L"", rect<s32>(20, 25, 290, 45), true, wANCard, EDITBOX_ANCARD);
	ebANCard->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	lstANCard = env->addListBox(rect<s32>(20, 50, 290, 265), wANCard, LISTBOX_ANCARD, true);
	btnANCardOK = env->addButton(rect<s32>(110, 270, 200, 295), wANCard, BUTTON_ANCARD_OK, dataManager.GetSysString(1211));
	//announce attribute
	wANAttribute = env->addWindow(rect<s32>(500, 200, 830, 285), false, dataManager.GetSysString(562));
	wANAttribute->getCloseButton()->setVisible(false);
	wANAttribute->setVisible(false);
	for(int filter = 0x1, i = 0; i < 7; filter <<= 1, ++i)
		chkAttribute[i] = env->addCheckBox(false, rect<s32>(10 + (i % 4) * 80, 25 + (i / 4) * 25, 90 + (i % 4) * 80, 50 + (i / 4) * 25),
		                                   wANAttribute, CHECK_ATTRIBUTE, dataManager.FormatAttribute(filter));
	//announce race
	wANRace = env->addWindow(rect<s32>(480, 200, 850, 410), false, dataManager.GetSysString(563));
	wANRace->getCloseButton()->setVisible(false);
	wANRace->setVisible(false);
	for(int filter = 0x1, i = 0; i < RACES_COUNT; filter <<= 1, ++i)
		chkRace[i] = env->addCheckBox(false, rect<s32>(10 + (i % 4) * 90, 25 + (i / 4) * 25, 100 + (i % 4) * 90, 50 + (i / 4) * 25),
		                              wANRace, CHECK_RACE, dataManager.FormatRace(filter));
	//selection hint
	stHintMsg = env->addStaticText(L"", rect<s32>(500, 82, 820, 112), true, false, 0, -1, false);
	stHintMsg->setBackgroundColor(0xc0ffffff);
	stHintMsg->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stHintMsg->setVisible(false);
	//cmd menu
	wCmdMenu = env->addWindow(rect<s32>(10, 10, 110, 179), false, L"");
	wCmdMenu->setDrawTitlebar(false);
	wCmdMenu->setVisible(false);
	wCmdMenu->getCloseButton()->setVisible(false);
	btnActivate = env->addButton(rect<s32>(1, 1, 99, 21), wCmdMenu, BUTTON_CMD_ACTIVATE, dataManager.GetSysString(1150));
	btnSummon = env->addButton(rect<s32>(1, 22, 99, 42), wCmdMenu, BUTTON_CMD_SUMMON, dataManager.GetSysString(1151));
	btnSPSummon = env->addButton(rect<s32>(1, 43, 99, 63), wCmdMenu, BUTTON_CMD_SPSUMMON, dataManager.GetSysString(1152));
	btnMSet = env->addButton(rect<s32>(1, 64, 99, 84), wCmdMenu, BUTTON_CMD_MSET, dataManager.GetSysString(1153));
	btnSSet = env->addButton(rect<s32>(1, 85, 99, 105), wCmdMenu, BUTTON_CMD_SSET, dataManager.GetSysString(1153));
	btnRepos = env->addButton(rect<s32>(1, 106, 99, 126), wCmdMenu, BUTTON_CMD_REPOS, dataManager.GetSysString(1154));
	btnAttack = env->addButton(rect<s32>(1, 127, 99, 147), wCmdMenu, BUTTON_CMD_ATTACK, dataManager.GetSysString(1157));
	btnShowList = env->addButton(rect<s32>(1, 148, 99, 168), wCmdMenu, BUTTON_CMD_SHOWLIST, dataManager.GetSysString(1158));
	btnOperation = env->addButton(rect<s32>(1, 169, 99, 189), wCmdMenu, BUTTON_CMD_ACTIVATE, dataManager.GetSysString(1161));
	btnReset = env->addButton(rect<s32>(1, 190, 99, 210), wCmdMenu, BUTTON_CMD_RESET, dataManager.GetSysString(1162));
	//deck edit
	wDeckEdit = env->addStaticText(L"", rect<s32>(309, 5, 605, 130), true, false, 0, -1, true);
	wDeckEdit->setVisible(false);
	stBanlist = env->addStaticText(dataManager.GetSysString(1300), rect<s32>(10, 9, 100, 29), false, false, wDeckEdit);
	cbDBLFList = env->addComboBox(rect<s32>(80, 5, 220, 30), wDeckEdit, COMBOBOX_DBLFLIST);
	cbDBLFList->setMaxSelectionRows(10);
	stDeck = env->addStaticText(dataManager.GetSysString(1301), rect<s32>(10, 39, 100, 59), false, false, wDeckEdit);
	cbDBDecks = env->addComboBox(rect<s32>(80, 35, 220, 60), wDeckEdit, COMBOBOX_DBDECKS);
	cbDBDecks->setMaxSelectionRows(15);
	for(unsigned int i = 0; i < deckManager._lfList.size(); ++i)
		cbDBLFList->addItem(deckManager._lfList[i].listName.c_str());
	btnSaveDeck = env->addButton(rect<s32>(225, 35, 290, 60), wDeckEdit, BUTTON_SAVE_DECK, dataManager.GetSysString(1302));
	ebDeckname = env->addEditBox(L"", rect<s32>(80, 65, 220, 90), true, wDeckEdit, -1);
	ebDeckname->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnSaveDeckAs = env->addButton(rect<s32>(225, 65, 290, 90), wDeckEdit, BUTTON_SAVE_DECK_AS, dataManager.GetSysString(1303));
	btnDeleteDeck = env->addButton(rect<s32>(225, 95, 290, 120), wDeckEdit, BUTTON_DELETE_DECK, dataManager.GetSysString(1308));
	btnShuffleDeck = env->addButton(rect<s32>(5, 99, 55, 120), wDeckEdit, BUTTON_SHUFFLE_DECK, dataManager.GetSysString(1307));
	btnSortDeck = env->addButton(rect<s32>(60, 99, 110, 120), wDeckEdit, BUTTON_SORT_DECK, dataManager.GetSysString(1305));
	btnClearDeck = env->addButton(rect<s32>(115, 99, 165, 120), wDeckEdit, BUTTON_CLEAR_DECK, dataManager.GetSysString(1304));
	btnSideOK = env->addButton(rect<s32>(400, 40, 710, 80), 0, BUTTON_SIDE_OK, dataManager.GetSysString(1334));
	btnSideOK->setVisible(false);
	btnSideShuffle = env->addButton(rect<s32>(310, 100, 370, 130), 0, BUTTON_SHUFFLE_DECK, dataManager.GetSysString(1307));
	btnSideShuffle->setVisible(false);
	btnSideSort = env->addButton(rect<s32>(375, 100, 435, 130), 0, BUTTON_SORT_DECK, dataManager.GetSysString(1305));
	btnSideSort->setVisible(false);
	btnSideReload = env->addButton(rect<s32>(440, 100, 500, 130), 0, BUTTON_SIDE_RELOAD, dataManager.GetSysString(1309));
	btnSideReload->setVisible(false);
	btnRenameDeck = env->addButton(rect<s32>(170, 99, 220, 120), wDeckEdit, BUTTON_RENAME_DECK, dataManager.GetSysString(1362));
	//
	scrFilter = env->addScrollBar(false, recti(999, 161, 1019, 629), 0, SCROLL_FILTER);
	scrFilter->setLargeStep(10);
	scrFilter->setSmallStep(1);
	scrFilter->setVisible(false);
	//rename deck
	wRenameDeck = env->addWindow(rect<s32>(510, 200, 820, 320), false, dataManager.GetSysString(1376));
	wRenameDeck->getCloseButton()->setVisible(false);
	wRenameDeck->setVisible(false);
	wRenameDeck->setDraggable(false);
	env->addStaticText(dataManager.GetSysString(1377), rect<s32>(20, 27, 290, 47), false, false, wRenameDeck);
	ebREName = env->addEditBox(L"", rect<s32>(20, 50, 290, 70), true, wRenameDeck, -1);
	ebREName->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnREYes = env->addButton(rect<s32>(70, 80, 140, 105), wRenameDeck, BUTTON_RENAME_DECK_SAVE, dataManager.GetSysString(1341));
	btnRENo = env->addButton(rect<s32>(170, 80, 240, 105), wRenameDeck, BUTTON_RENAME_DECK_CANCEL, dataManager.GetSysString(1212));
	//sort type
	wSort = env->addStaticText(L"", rect<s32>(930, 132, 1020, 156), true, false, 0, -1, true);
	cbSortType = env->addComboBox(rect<s32>(10, 2, 85, 22), wSort, COMBOBOX_SORTTYPE);
	cbSortType->setMaxSelectionRows(10);
	for(int i = 1370; i <= 1373; i++)
		cbSortType->addItem(dataManager.GetSysString(i));
	wSort->setVisible(false);
	//filters
	wFilter = env->addStaticText(L"", rect<s32>(610, 5, 1020, 130), true, false, 0, -1, true);
	wFilter->setVisible(false);
	stCategory = env->addStaticText(dataManager.GetSysString(1311), rect<s32>(10, 25 / 6 + 2, 70, 22 + 25 / 6), false, false, wFilter);
	cbCardType = env->addComboBox(rect<s32>(60, 25 / 6, 120, 20 + 25 / 6), wFilter, COMBOBOX_MAINTYPE);
	cbCardType->addItem(dataManager.GetSysString(1310));
	cbCardType->addItem(dataManager.GetSysString(1312));
	cbCardType->addItem(dataManager.GetSysString(1313));
	cbCardType->addItem(dataManager.GetSysString(1314));
	cbCardType2 = env->addComboBox(rect<s32>(125, 25 / 6, 200, 20 + 25 / 6), wFilter, COMBOBOX_SECONDTYPE);
	cbCardType2->setMaxSelectionRows(10);
	cbCardType2->addItem(dataManager.GetSysString(1310), 0);
	stLimit = env->addStaticText(dataManager.GetSysString(1315), rect<s32>(205, 2 + 25 / 6, 280, 22 + 25 / 6), false, false, wFilter);
	cbLimit = env->addComboBox(rect<s32>(260, 25 / 6, 390, 20 + 25 / 6), wFilter, COMBOBOX_LIMIT);
	cbLimit->setMaxSelectionRows(10);
	cbLimit->addItem(dataManager.GetSysString(1310));
	cbLimit->addItem(dataManager.GetSysString(1316));
	cbLimit->addItem(dataManager.GetSysString(1317));
	cbLimit->addItem(dataManager.GetSysString(1318));
	cbLimit->addItem(dataManager.GetSysString(1240));
	cbLimit->addItem(dataManager.GetSysString(1241));
	cbLimit->addItem(dataManager.GetSysString(1242));
	cbLimit->addItem(dataManager.GetSysString(1243));
	stAttribute = env->addStaticText(dataManager.GetSysString(1319), rect<s32>(10, 22 + 50 / 6, 70, 42 + 50 / 6), false, false, wFilter);
	cbAttribute = env->addComboBox(rect<s32>(60, 20 + 50 / 6, 190, 40 + 50 / 6), wFilter, COMBOBOX_ATTRIBUTE);
	cbAttribute->setMaxSelectionRows(10);
	cbAttribute->addItem(dataManager.GetSysString(1310), 0);
	for(int filter = 0x1; filter != 0x80; filter <<= 1)
		cbAttribute->addItem(dataManager.FormatAttribute(filter), filter);
	stRace = env->addStaticText(dataManager.GetSysString(1321), rect<s32>(10, 42 + 75 / 6, 70, 62 + 75 / 6), false, false, wFilter);
	cbRace = env->addComboBox(rect<s32>(60, 40 + 75 / 6, 190, 60 + 75 / 6), wFilter, COMBOBOX_RACE);
	cbRace->setMaxSelectionRows(10);
	cbRace->addItem(dataManager.GetSysString(1310), 0);
	for(int filter = 0x1; filter < (1 << RACES_COUNT); filter <<= 1)
		cbRace->addItem(dataManager.FormatRace(filter), filter);
	stAttack = env->addStaticText(dataManager.GetSysString(1322), rect<s32>(205, 22 + 50 / 6, 280, 42 + 50 / 6), false, false, wFilter);
	ebAttack = env->addEditBox(L"", rect<s32>(260, 20 + 50 / 6, 340, 40 + 50 / 6), true, wFilter, EDITBOX_INPUTS);
	ebAttack->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stDefense = env->addStaticText(dataManager.GetSysString(1323), rect<s32>(205, 42 + 75 / 6, 280, 62 + 75 / 6), false, false, wFilter);
	ebDefense = env->addEditBox(L"", rect<s32>(260, 40 + 75 / 6, 340, 60 + 75 / 6), true, wFilter, EDITBOX_INPUTS);
	ebDefense->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stStar = env->addStaticText(dataManager.GetSysString(1324), rect<s32>(10, 62 + 100 / 6, 80, 82 + 100 / 6), false, false, wFilter);
	ebStar = env->addEditBox(L"", rect<s32>(60, 60 + 100 / 6, 100, 80 + 100 / 6), true, wFilter, EDITBOX_INPUTS);
	ebStar->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stScale = env->addStaticText(dataManager.GetSysString(1336), rect<s32>(101, 62 + 100 / 6, 150, 82 + 100 / 6), false, false, wFilter);
	ebScale = env->addEditBox(L"", rect<s32>(150, 60 + 100 / 6, 190, 80 + 100 / 6), true, wFilter, EDITBOX_INPUTS);
	ebScale->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stSearch = env->addStaticText(dataManager.GetSysString(1325), rect<s32>(205, 62 + 100 / 6, 280, 82 + 100 / 6), false, false, wFilter);
	ebCardName = env->addEditBox(L"", rect<s32>(260, 60 + 100 / 6, 390, 80 + 100 / 6), true, wFilter, EDITBOX_KEYWORD);
	ebCardName->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnEffectFilter = env->addButton(rect<s32>(345, 20 + 50 / 6, 390, 60 + 75 / 6), wFilter, BUTTON_EFFECT_FILTER, dataManager.GetSysString(1326));
	btnStartFilter = env->addButton(rect<s32>(205, 80 + 125 / 6, 390, 100 + 125 / 6), wFilter, BUTTON_START_FILTER, dataManager.GetSysString(1327));
	if(gameConf.separate_clear_button) {
		btnStartFilter->setRelativePosition(rect<s32>(260, 80 + 125 / 6, 390, 100 + 125 / 6));
		btnClearFilter = env->addButton(rect<s32>(205, 80 + 125 / 6, 255, 100 + 125 / 6), wFilter, BUTTON_CLEAR_FILTER, dataManager.GetSysString(1304));
	}
	wCategories = env->addWindow(rect<s32>(600, 60, 1000, 305), false, L"");
	wCategories->getCloseButton()->setVisible(false);
	wCategories->setDrawTitlebar(false);
	wCategories->setDraggable(false);
	wCategories->setVisible(false);
	btnCategoryOK = env->addButton(rect<s32>(150, 210, 250, 235), wCategories, BUTTON_CATEGORY_OK, dataManager.GetSysString(1211));
	int catewidth = 0;
	for(int i = 0; i < 32; ++i) {
		irr::core::dimension2d<unsigned int> dtxt = mainGame->guiFont->getDimension(dataManager.GetSysString(1100 + i));
		if((int)dtxt.Width + 40 > catewidth)
			catewidth = dtxt.Width + 40;
	}
	for(int i = 0; i < 32; ++i)
		chkCategory[i] = env->addCheckBox(false, recti(10 + (i % 4) * catewidth, 5 + (i / 4) * 25, 10 + (i % 4 + 1) * catewidth, 5 + (i / 4 + 1) * 25), wCategories, -1, dataManager.GetSysString(1100 + i));
	int wcatewidth = catewidth * 4 + 16;
	wCategories->setRelativePosition(rect<s32>(1000 - wcatewidth, 60, 1000, 305));
	btnCategoryOK->setRelativePosition(recti(wcatewidth / 2 - 50, 210, wcatewidth / 2 + 50, 235));
	btnMarksFilter = env->addButton(rect<s32>(60, 80 + 125 / 6, 190, 100 + 125 / 6), wFilter, BUTTON_MARKS_FILTER, dataManager.GetSysString(1374));
	wLinkMarks = env->addWindow(rect<s32>(700, 30, 820, 150), false, L"");
	wLinkMarks->getCloseButton()->setVisible(false);
	wLinkMarks->setDrawTitlebar(false);
	wLinkMarks->setDraggable(false);
	wLinkMarks->setVisible(false);
	btnMarksOK = env->addButton(recti(45, 45, 75, 75), wLinkMarks, BUTTON_MARKERS_OK, dataManager.GetSysString(1211));
	btnMark[0] = env->addButton(recti(10, 10, 40, 40), wLinkMarks, -1, L"\u2196");
	btnMark[1] = env->addButton(recti(45, 10, 75, 40), wLinkMarks, -1, L"\u2191");
	btnMark[2] = env->addButton(recti(80, 10, 110, 40), wLinkMarks, -1, L"\u2197");
	btnMark[3] = env->addButton(recti(10, 45, 40, 75), wLinkMarks, -1, L"\u2190");
	btnMark[4] = env->addButton(recti(80, 45, 110, 75), wLinkMarks, -1, L"\u2192");
	btnMark[5] = env->addButton(recti(10, 80, 40, 110), wLinkMarks, -1, L"\u2199");
	btnMark[6] = env->addButton(recti(45, 80, 75, 110), wLinkMarks, -1, L"\u2193");
	btnMark[7] = env->addButton(recti(80, 80, 110, 110), wLinkMarks, -1, L"\u2198");
	for(int i=0;i<8;i++)
		btnMark[i]->setIsPushButton(true);
	//replay window
	wReplay = env->addWindow(rect<s32>(220, 100, 800, 520), false, dataManager.GetSysString(1202));
	wReplay->getCloseButton()->setVisible(false);
	wReplay->setVisible(false);
	lstReplayList = env->addListBox(rect<s32>(10, 30, 350, 400), wReplay, LISTBOX_REPLAY_LIST, true);
	lstReplayList->setItemHeight(18);
	btnLoadReplay = env->addButton(rect<s32>(470, 355, 570, 380), wReplay, BUTTON_LOAD_REPLAY, dataManager.GetSysString(1348));
	btnDeleteReplay = env->addButton(rect<s32>(360, 355, 460, 380), wReplay, BUTTON_DELETE_REPLAY, dataManager.GetSysString(1361));
	btnRenameReplay = env->addButton(rect<s32>(360, 385, 460, 410), wReplay, BUTTON_RENAME_REPLAY, dataManager.GetSysString(1362));
	btnReplayCancel = env->addButton(rect<s32>(470, 385, 570, 410), wReplay, BUTTON_CANCEL_REPLAY, dataManager.GetSysString(1347));
	btnExportDeck = env->addButton(rect<s32>(470, 325, 570, 350), wReplay, BUTTON_EXPORT_DECK, dataManager.GetSysString(1369));
	env->addStaticText(dataManager.GetSysString(1349), rect<s32>(360, 30, 570, 50), false, true, wReplay);
	stReplayInfo = env->addStaticText(L"", rect<s32>(360, 60, 570, 320), false, true, wReplay);
	env->addStaticText(dataManager.GetSysString(1353), rect<s32>(360, 275, 570, 295), false, true, wReplay);
	ebRepStartTurn = env->addEditBox(L"", rect<s32>(360, 300, 460, 320), true, wReplay, -1);
	ebRepStartTurn->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	//single play window
	wSinglePlay = env->addWindow(rect<s32>(220, 100, 800, 520), false, dataManager.GetSysString(1201));
	wSinglePlay->getCloseButton()->setVisible(false);
	wSinglePlay->setVisible(false);
	irr::gui::IGUITabControl* wSingle = env->addTabControl(rect<s32>(0, 20, 579, 419), wSinglePlay, true);
	if(gameConf.enable_bot_mode) {
		irr::gui::IGUITab* tabBot = wSingle->addTab(dataManager.GetSysString(1380));
		lstBotList = env->addListBox(rect<s32>(10, 10, 350, 350), tabBot, LISTBOX_BOT_LIST, true);
		lstBotList->setItemHeight(18);
		btnStartBot = env->addButton(rect<s32>(459, 301, 569, 326), tabBot, BUTTON_BOT_START, dataManager.GetSysString(1211));
		btnBotCancel = env->addButton(rect<s32>(459, 331, 569, 356), tabBot, BUTTON_CANCEL_SINGLEPLAY, dataManager.GetSysString(1210));
		env->addStaticText(dataManager.GetSysString(1382), rect<s32>(360, 10, 550, 30), false, true, tabBot);
		stBotInfo = env->addStaticText(L"", rect<s32>(360, 40, 560, 160), false, true, tabBot);
		cbBotRule = env->addComboBox(rect<s32>(360, 165, 560, 190), tabBot, COMBOBOX_BOT_RULE);
		cbBotRule->addItem(dataManager.GetSysString(1262));
		cbBotRule->addItem(dataManager.GetSysString(1263));
		cbBotRule->addItem(dataManager.GetSysString(1264));
		cbBotRule->setSelected(gameConf.default_rule - 3);
		chkBotHand = env->addCheckBox(false, rect<s32>(360, 200, 560, 220), tabBot, -1, dataManager.GetSysString(1384));
		chkBotNoCheckDeck = env->addCheckBox(false, rect<s32>(360, 230, 560, 250), tabBot, -1, dataManager.GetSysString(1229));
		chkBotNoShuffleDeck = env->addCheckBox(false, rect<s32>(360, 260, 560, 280), tabBot, -1, dataManager.GetSysString(1230));
	} else { // avoid null pointer
		btnStartBot = env->addButton(rect<s32>(0, 0, 0, 0), wSinglePlay);
		btnBotCancel = env->addButton(rect<s32>(0, 0, 0, 0), wSinglePlay);
		btnStartBot->setVisible(false);
		btnBotCancel->setVisible(false);
	}
	irr::gui::IGUITab* tabSingle = wSingle->addTab(dataManager.GetSysString(1381));
	lstSinglePlayList = env->addListBox(rect<s32>(10, 10, 350, 350), tabSingle, LISTBOX_SINGLEPLAY_LIST, true);
	lstSinglePlayList->setItemHeight(18);
	btnLoadSinglePlay = env->addButton(rect<s32>(459, 301, 569, 326), tabSingle, BUTTON_LOAD_SINGLEPLAY, dataManager.GetSysString(1211));
	btnSinglePlayCancel = env->addButton(rect<s32>(459, 331, 569, 356), tabSingle, BUTTON_CANCEL_SINGLEPLAY, dataManager.GetSysString(1210));
	env->addStaticText(dataManager.GetSysString(1352), rect<s32>(360, 10, 550, 30), false, true, tabSingle);
	stSinglePlayInfo = env->addStaticText(L"", rect<s32>(360, 40, 550, 280), false, true, tabSingle);
	//replay save
	wReplaySave = env->addWindow(rect<s32>(510, 200, 820, 320), false, dataManager.GetSysString(1340));
	wReplaySave->getCloseButton()->setVisible(false);
	wReplaySave->setVisible(false);
	env->addStaticText(dataManager.GetSysString(1342), rect<s32>(20, 25, 290, 45), false, false, wReplaySave);
	ebRSName =  env->addEditBox(L"", rect<s32>(20, 50, 290, 70), true, wReplaySave, -1);
	ebRSName->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	btnRSYes = env->addButton(rect<s32>(70, 80, 140, 105), wReplaySave, BUTTON_REPLAY_SAVE, dataManager.GetSysString(1341));
	btnRSNo = env->addButton(rect<s32>(170, 80, 240, 105), wReplaySave, BUTTON_REPLAY_CANCEL, dataManager.GetSysString(1212));
	//replay control
	wReplayControl = env->addStaticText(L"", rect<s32>(205, 118, 295, 273), true, false, 0, -1, true);
	wReplayControl->setVisible(false);
	btnReplayStart = env->addButton(rect<s32>(5, 5, 85, 25), wReplayControl, BUTTON_REPLAY_START, dataManager.GetSysString(1343));
	btnReplayPause = env->addButton(rect<s32>(5, 30, 85, 50), wReplayControl, BUTTON_REPLAY_PAUSE, dataManager.GetSysString(1344));
	btnReplayStep = env->addButton(rect<s32>(5, 55, 85, 75), wReplayControl, BUTTON_REPLAY_STEP, dataManager.GetSysString(1345));
	btnReplayUndo = env->addButton(rect<s32>(5, 80, 85, 100), wReplayControl, BUTTON_REPLAY_UNDO, dataManager.GetSysString(1360));
	btnReplaySwap = env->addButton(rect<s32>(5, 105, 85, 125), wReplayControl, BUTTON_REPLAY_SWAP, dataManager.GetSysString(1346));
	btnReplayExit = env->addButton(rect<s32>(5, 130, 85, 150), wReplayControl, BUTTON_REPLAY_EXIT, dataManager.GetSysString(1347));
	//chat
	wChat = env->addWindow(rect<s32>(305, 615, 1020, 640), false, L"");
	wChat->getCloseButton()->setVisible(false);
	wChat->setDraggable(false);
	wChat->setDrawTitlebar(false);
	wChat->setVisible(false);
	ebChatInput = env->addEditBox(L"", rect<s32>(3, 2, 710, 22), true, wChat, EDITBOX_CHAT);
	//swap
	btnSpectatorSwap = env->addButton(rect<s32>(205, 100, 295, 135), 0, BUTTON_REPLAY_SWAP, dataManager.GetSysString(1346));
	btnSpectatorSwap->setVisible(false);
	//chain buttons
	btnChainIgnore = env->addButton(rect<s32>(205, 100, 295, 135), 0, BUTTON_CHAIN_IGNORE, dataManager.GetSysString(1292));
	btnChainAlways = env->addButton(rect<s32>(205, 140, 295, 175), 0, BUTTON_CHAIN_ALWAYS, dataManager.GetSysString(1293));
	btnChainWhenAvail = env->addButton(rect<s32>(205, 180, 295, 215), 0, BUTTON_CHAIN_WHENAVAIL, dataManager.GetSysString(1294));
	btnChainIgnore->setIsPushButton(true);
	btnChainAlways->setIsPushButton(true);
	btnChainWhenAvail->setIsPushButton(true);
	btnChainIgnore->setVisible(false);
	btnChainAlways->setVisible(false);
	btnChainWhenAvail->setVisible(false);
	//shuffle
	btnShuffle = env->addButton(rect<s32>(205, 230, 295, 265), 0, BUTTON_CMD_SHUFFLE, dataManager.GetSysString(1297));
	btnShuffle->setVisible(false);
	//cancel or finish
	btnCancelOrFinish = env->addButton(rect<s32>(205, 230, 295, 265), 0, BUTTON_CANCEL_OR_FINISH, dataManager.GetSysString(1295));
	btnCancelOrFinish->setVisible(false);
	//leave/surrender/exit
	btnLeaveGame = env->addButton(rect<s32>(205, 5, 295, 80), 0, BUTTON_LEAVE_GAME, L"");
	btnLeaveGame->setVisible(false);
	//tip
	stTip = env->addStaticText(L"", rect<s32>(0, 0, 150, 150), false, true, 0, -1, true);
	stTip->setBackgroundColor(0xc0ffffff);
	stTip->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stTip->setVisible(false);
	//tip for cards in select / display list
	stCardListTip = env->addStaticText(L"", rect<s32>(0, 0, 150, 150), false, true, wCardSelect, TEXT_CARD_LIST_TIP, true);
	stCardListTip->setBackgroundColor(0xc0ffffff);
	stCardListTip->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
	stCardListTip->setVisible(false);
	device->setEventReceiver(&menuHandler);
	LoadConfig();
	if(!soundManager.Init()) {
		chkEnableSound->setChecked(false);
		chkEnableSound->setEnabled(false);
		chkEnableSound->setVisible(false);
		chkEnableMusic->setChecked(false);
		chkEnableMusic->setEnabled(false);
		chkEnableMusic->setVisible(false);
		scrSoundVolume->setVisible(false);
		scrMusicVolume->setVisible(false);
		chkMusicMode->setEnabled(false);
		chkMusicMode->setVisible(false);
	}
	env->getSkin()->setFont(guiFont);
	env->setFocus(wMainMenu);
	for (u32 i = 0; i < EGDC_COUNT; ++i) {
		SColor col = env->getSkin()->getColor((EGUI_DEFAULT_COLOR)i);
		col.setAlpha(224);
		env->getSkin()->setColor((EGUI_DEFAULT_COLOR)i, col);
	}
	hideChat = false;
	hideChatTimer = 0;
	return true;
}
void Game::MainLoop() {
	wchar_t cap[256];
	camera = smgr->addCameraSceneNode(0);
	irr::core::matrix4 mProjection;
	BuildProjectionMatrix(mProjection, -0.90f, 0.45f, -0.42f, 0.42f, 1.0f, 100.0f);
	camera->setProjectionMatrix(mProjection);

	mProjection.buildCameraLookAtMatrixLH(vector3df(4.2f, 8.0f, 7.8f), vector3df(4.2f, 0, 0), vector3df(0, 0, 1));
	camera->setViewMatrixAffector(mProjection);
	smgr->setAmbientLight(SColorf(1.0f, 1.0f, 1.0f));
	float atkframe = 0.1f;
	irr::ITimer* timer = device->getTimer();
	timer->setTime(0);
	int fps = 0;
	int cur_time = 0;
	while(device->run()) {
		dimension2du size = driver->getScreenSize();
		if(window_size != size) {
			window_size = size;
			xScale = window_size.Width / 1024.0;
			yScale = window_size.Height / 640.0;
			OnResize();
		}
		linePatternD3D = (linePatternD3D + 1) % 30;
		linePatternGL = (linePatternGL << 1) | (linePatternGL >> 15);
		atkframe += 0.1f;
		atkdy = (float)sin(atkframe);
		driver->beginScene(true, true, SColor(0, 0, 0, 0));
		gMutex.lock();
		if(dInfo.isStarted) {
			if(dInfo.isFinished && showcardcode == 1)
				soundManager.PlayBGM(BGM_WIN);
			else if(dInfo.isFinished && (showcardcode == 2 || showcardcode == 3))
				soundManager.PlayBGM(BGM_LOSE);
			else if(dInfo.lp[0] > 0 && dInfo.lp[0] <= dInfo.lp[1] / 2)
				soundManager.PlayBGM(BGM_DISADVANTAGE);
			else if(dInfo.lp[0] > 0 && dInfo.lp[0] >= dInfo.lp[1] * 2)
				soundManager.PlayBGM(BGM_ADVANTAGE);
			else
				soundManager.PlayBGM(BGM_DUEL);
			DrawBackImage(imageManager.tBackGround);
			DrawBackGround();
			DrawCards();
			DrawMisc();
			smgr->drawAll();
			driver->setMaterial(irr::video::IdentityMaterial);
			driver->clearZBuffer();
		} else if(is_building) {
			soundManager.PlayBGM(BGM_DECK);
			DrawBackImage(imageManager.tBackGround_deck);
			DrawDeckBd();
		} else {
			soundManager.PlayBGM(BGM_MENU);
			DrawBackImage(imageManager.tBackGround_menu);
		}
		DrawGUI();
		DrawSpec();
		gMutex.unlock();
		if(signalFrame > 0) {
			signalFrame--;
			if(!signalFrame)
				frameSignal.Set();
		}
		if(waitFrame >= 0) {
			waitFrame++;
			if(waitFrame % 90 == 0) {
				stHintMsg->setText(dataManager.GetSysString(1390));
			} else if(waitFrame % 90 == 30) {
				stHintMsg->setText(dataManager.GetSysString(1391));
			} else if(waitFrame % 90 == 60) {
				stHintMsg->setText(dataManager.GetSysString(1392));
			}
		}
		driver->endScene();
		if(closeSignal.Wait(1))
			CloseDuelWindow();
		fps++;
		cur_time = timer->getTime();
		if(cur_time < fps * 17 - 20)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		if(cur_time >= 1000) {
			myswprintf(cap, L"YGO-VI-Rook FPS: %d", fps);
			device->setWindowCaption(cap);
			fps = 0;
			cur_time -= 1000;
			timer->setTime(0);
			if(dInfo.time_player == 0 || dInfo.time_player == 1)
				if(dInfo.time_left[dInfo.time_player]) {
					dInfo.time_left[dInfo.time_player]--;
					RefreshTimeDisplay();
				}
		}
		if (DuelClient::try_needed) {
			DuelClient::try_needed = false;
			DuelClient::StartClient(DuelClient::temp_ip, DuelClient::temp_port, false);
		}
	}
	DuelClient::StopClient(true);
	if(dInfo.isSingleMode)
		SingleMode::StopPlay(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	SaveConfig();
//	device->drop();
}
void Game::RefreshTimeDisplay() {
	for(int i = 0; i < 2; ++i) {
		if(dInfo.time_left[i] && dInfo.time_limit) {
			if(dInfo.time_left[i] >= dInfo.time_limit / 2)
				dInfo.time_color[i] = 0xffffffff;
			else if(dInfo.time_left[i] >= dInfo.time_limit / 3)
				dInfo.time_color[i] = 0xffffff00;
			else if(dInfo.time_left[i] >= dInfo.time_limit / 6)
				dInfo.time_color[i] = 0xffff7f00;
			else
				dInfo.time_color[i] = 0xffff0000;
		} else
			dInfo.time_color[i] = 0xffffffff;
	}
	myswprintf(dInfo.str_time_left[0], L"%d", dInfo.time_left[0]);
	myswprintf(dInfo.str_time_left[1], L"%d", dInfo.time_left[1]);
}
void Game::BuildProjectionMatrix(irr::core::matrix4& mProjection, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar) {
	for(int i = 0; i < 16; ++i)
		mProjection[i] = 0;
	mProjection[0] = 2.0f * znear / (right - left);
	mProjection[5] = 2.0f * znear / (top - bottom);
	mProjection[8] = (left + right) / (left - right);
	mProjection[9] = (top + bottom) / (bottom - top);
	mProjection[10] = zfar / (zfar - znear);
	mProjection[11] = 1.0f;
	mProjection[14] = znear * zfar / (znear - zfar);
}
void Game::InitStaticText(irr::gui::IGUIStaticText* pControl, u32 cWidth, u32 cHeight, irr::gui::CGUITTFont* font, const wchar_t* text) {
	std::wstring format_text;
	format_text = SetStaticText(pControl, cWidth, font, text);
	if(font->getDimension(format_text.c_str()).Height <= cHeight) {
		scrCardText->setVisible(false);
		if(env->hasFocus(scrCardText))
			env->removeFocus(scrCardText);
		return;
	}
	format_text = SetStaticText(pControl, cWidth-25, font, text);
	u32 fontheight = font->getDimension(L"A").Height + font->getKerningHeight();
	u32 step = (font->getDimension(format_text.c_str()).Height - cHeight) / fontheight + 1;
	scrCardText->setVisible(true);
	scrCardText->setMin(0);
	scrCardText->setMax(step);
	scrCardText->setPos(0);
}
std::wstring Game::SetStaticText(irr::gui::IGUIStaticText* pControl, u32 cWidth, irr::gui::CGUITTFont* font, const wchar_t* text, u32 pos) {
	int pbuffer = 0;
	u32 _width = 0, _height = 0;
	wchar_t prev = 0;
	wchar_t strBuffer[4096];
	std::wstring ret;

	for(size_t i = 0; text[i] != 0 && i < wcslen(text); ++i) {
		wchar_t c = text[i];
		u32 w = font->getCharDimension(c).Width + font->getKerningWidth(c, prev);
		prev = c;
		if(text[i] == L'\r') {
			continue;
		} else if(text[i] == L'\n') {
			strBuffer[pbuffer++] = L'\n';
			_width = 0;
			_height++;
			prev = 0;
			if(_height == pos)
				pbuffer = 0;
			continue;
		} else if(_width > 0 && _width + w > cWidth) {
			strBuffer[pbuffer++] = L'\n';
			_width = 0;
			_height++;
			prev = 0;
			if(_height == pos)
				pbuffer = 0;
		}
		_width += w;
		strBuffer[pbuffer++] = c;
	}
	strBuffer[pbuffer] = 0;
	if(pControl) pControl->setText(strBuffer);
	ret.assign(strBuffer);
	return ret;
}
void Game::ReLoadExpansions() {
	for (size_t i = 0; i < dataManager._expansionDatas.size(); ++i) {
		int code = dataManager._expansionDatas[i];
		dataManager._strings.erase(code);
		dataManager._datas.erase(code);
	}
	dataManager._expansionDatas.clear();
	LoadExpansions();
}
void Game::LoadExpansions() {
	FileSystem::TraversalDir(L"./expansions", [](const wchar_t* name, bool isdir) {
		if(!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".cdb", 4)) {
			wchar_t fpath[1024];
			myswprintf(fpath, L"./expansions/%ls", name);
			dataManager.LoadDB(fpath, true);
		}
		if(!isdir && wcsrchr(name, '.') && (!mywcsncasecmp(wcsrchr(name, '.'), L".zip", 4) || !mywcsncasecmp(wcsrchr(name, '.'), L".ypk", 4))) {
			wchar_t fpath[1024];
			myswprintf(fpath, L"./expansions/%ls", name);
#ifdef _WIN32
			dataManager.FileSystem->addFileArchive(fpath, true, false, EFAT_ZIP);
#else
			char upath[1024];
			BufferIO::EncodeUTF8(fpath, upath);
			dataManager.FileSystem->addFileArchive(upath, true, false, EFAT_ZIP);
#endif
		}
	});
	for(u32 i = 0; i < DataManager::FileSystem->getFileArchiveCount(); ++i) {
		const IFileList* archive = DataManager::FileSystem->getFileArchive(i)->getFileList();
		for(u32 j = 0; j < archive->getFileCount(); ++j) {
#ifdef _WIN32
			const wchar_t* fname = archive->getFullFileName(j).c_str();
#else
			wchar_t fname[1024];
			const char* uname = archive->getFullFileName(j).c_str();
			BufferIO::DecodeUTF8(uname, fname);
#endif
			if(wcsrchr(fname, '.') && !mywcsncasecmp(wcsrchr(fname, '.'), L".cdb", 4))
				dataManager.LoadDB(fname, true);
			if(wcsrchr(fname, '.') && !mywcsncasecmp(wcsrchr(fname, '.'), L".conf", 5)) {
#ifdef _WIN32
				IReadFile* reader = DataManager::FileSystem->createAndOpenFile(fname);
#else
				IReadFile* reader = DataManager::FileSystem->createAndOpenFile(uname);
#endif
				dataManager.LoadStrings(reader);
			}
		}
	}
}
void Game::RefreshDeck(irr::gui::IGUIComboBox* cbDeck) {
	cbDeck->clear();
	FileSystem::TraversalDir(L"./deck", [cbDeck](const wchar_t* name, bool isdir) {
		if(!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".ydk", 4)) {
			size_t len = wcslen(name);
			wchar_t deckname[256];
			wcsncpy(deckname, name, len - 4);
			deckname[len - 4] = 0;
			cbDeck->addItem(deckname);
		}
	});
	for(size_t i = 0; i < cbDeck->getItemCount(); ++i) {
		if(!wcscmp(cbDeck->getItem(i), gameConf.lastdeck)) {
			cbDeck->setSelected(i);
			break;
		}
	}
}
void Game::RefreshReplay() {
	lstReplayList->clear();
	FileSystem::TraversalDir(L"./replay", [this](const wchar_t* name, bool isdir) {
		if (!isdir && IsExtension(name, L".yrp"))
			lstReplayList->addItem(name);
	});
}
void Game::RefreshSingleplay() {
	lstSinglePlayList->clear();
	stSinglePlayInfo->setText(L"");
	FileSystem::TraversalDir(L"./single", [this](const wchar_t* name, bool isdir) {
		if(!isdir && IsExtension(name, L".lua"))
			lstSinglePlayList->addItem(name);
	});
}
void Game::RefreshFont() {
	cbFont->clear();
	FileSystem::TraversalDir(L"./font", [this](const wchar_t* name, bool isdir) {
		if((!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".ttc", 4)) || (!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".ttf", 4)))
			cbFont->addItem(name);
	});
	for(size_t i = 0; i < cbFont->getItemCount(); ++i) {
			size_t flen = wcslen(gameConf.textfont);
			wchar_t fname[256];
			wcsncpy(fname, gameConf.textfont + 5, flen - 5);
			fname[flen - 5] = 0;
		if(!wcscmp(cbFont->getItem(i), fname)) {
			cbFont->setSelected(i);
			break;
		}
	}
}
void Game::RefreshHDS() {
	cbHDS->clear();
	FileSystem::TraversalDir(L"./textures/head", [this](const wchar_t* name, bool isdir) {
		if(!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".jpg", 4))
			cbHDS->addItem(name);
	});
}
void Game::RefreshCRS() {
	cbCRS->clear();
	FileSystem::TraversalDir(L"./textures/cover", [this](const wchar_t* name, bool isdir) {
		if(!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".jpg", 4))
			cbCRS->addItem(name);
	});
}
void Game::RefreshBGS() {
	cbBGS->clear();
	FileSystem::TraversalDir(L"./textures/bg", [this](const wchar_t* name, bool isdir) {
		if(!isdir && wcsrchr(name, '.') && !mywcsncasecmp(wcsrchr(name, '.'), L".jpg", 4))
			cbBGS->addItem(name);
	});
}
void Game::RefreshBot() {
	if(!gameConf.enable_bot_mode)
		return;
	botInfo.clear();
	FILE* fp = fopen("bot.conf", "r");
	char linebuf[256];
	char strbuf[256];
	if(fp) {
		while(fgets(linebuf, 256, fp)) {
			if(linebuf[0] == '#')
				continue;
			if(linebuf[0] == '!') {
				BotInfo newinfo;
				sscanf(linebuf, "!%240[^\n]", strbuf);
				BufferIO::DecodeUTF8(strbuf, newinfo.name);
				fgets(linebuf, 256, fp);
				sscanf(linebuf, "%240[^\n]", strbuf);
				BufferIO::DecodeUTF8(strbuf, newinfo.command);
				fgets(linebuf, 256, fp);
				sscanf(linebuf, "%240[^\n]", strbuf);
				BufferIO::DecodeUTF8(strbuf, newinfo.desc);
				fgets(linebuf, 256, fp);
				newinfo.support_master_rule_3 = !!strstr(linebuf, "SUPPORT_MASTER_RULE_3");
				newinfo.support_new_master_rule = !!strstr(linebuf, "SUPPORT_NEW_MASTER_RULE");
				newinfo.support_master_rule_2020 = !!strstr(linebuf, "SUPPORT_MASTER_RULE_2020");
				int rule = cbBotRule->getSelected() + 3;
				if((rule == 3 && newinfo.support_master_rule_3)
					|| (rule == 4 && newinfo.support_new_master_rule)
					|| (rule == 5 && newinfo.support_master_rule_2020))
					botInfo.push_back(newinfo);
				continue;
			}
		}
		fclose(fp);
	}
	lstBotList->clear();
	stBotInfo->setText(L"");
	for(unsigned int i = 0; i < botInfo.size(); ++i) {
		lstBotList->addItem(botInfo[i].name);
	}
	if(botInfo.size() == 0)
		SetStaticText(stBotInfo, 200, guiFont, dataManager.GetSysString(1385));
}
void Game::LoadConfig() {
	FILE* fp = fopen("system.conf", "r");
	if(!fp)
		return;
	char linebuf[256];
	char strbuf[32];
	char valbuf[256];
	wchar_t wstr[256];
	gameConf.use_d3d = 0;
	gameConf.use_image_scale = 1;
	gameConf.antialias = 0;
	gameConf.serverport = 7911;
	gameConf.textfontsize = 12;
	gameConf.game_version = 4960;
	gameConf.nickname[0] = 0;
	gameConf.gamename[0] = 0;
	gameConf.lastdeck[0] = 0;
	gameConf.numfont[0] = 0;
	gameConf.textfont[0] = 0;
	gameConf.lasthost[0] = 0;
	gameConf.lastport[0] = 0;
	gameConf.roompass[0] = 0;
	gameConf.searchname[0] = 0;
	//settings
	gameConf.chkMAutoPos = 0;
	gameConf.chkSTAutoPos = 1;
	gameConf.chkRandomPos = 0;
	gameConf.chkAutoChain = 0;
	gameConf.chkWaitChain = 0;
	gameConf.chkIgnore1 = 0;
	gameConf.chkIgnore2 = 0;
	gameConf.default_rule = DEFAULT_DUEL_RULE;
	gameConf.chkHideSetname = 0;
	gameConf.chkHideHintButton = 0;
	gameConf.control_mode = 0;
	gameConf.draw_field_spell = 1;
	gameConf.separate_clear_button = 1;
	gameConf.auto_search_limit = 0;
	gameConf.search_multiple_keywords = 1;
	gameConf.search_regex = 0;
	gameConf.chkIgnoreDeckChanges = 0;
	gameConf.defaultOT = 1;
	gameConf.enable_bot_mode = 1;
	gameConf.quick_animation = 0;
	gameConf.auto_save_replay = 0;
	gameConf.enable_sound = true;
	gameConf.ask_mset = 0;
	gameConf.sound_volume = 0.5;
	gameConf.enable_music = true;
	gameConf.music_volume = 0.5;
	gameConf.music_mode = 1;
	gameConf.window_maximized = false;
	gameConf.window_width = 1515;
	gameConf.window_height = 916;
	gameConf.resize_popup_menu = false;
	gameConf.chkEnablePScale = 1;
	gameConf.random = 1;
	while(fgets(linebuf, 256, fp)) {
		sscanf(linebuf, "%s = %s", strbuf, valbuf);
		if(!strcmp(strbuf, "antialias")) {
			gameConf.antialias = atoi(valbuf);
		} else if(!strcmp(strbuf, "use_d3d")) {
			gameConf.use_d3d = atoi(valbuf) > 0;
		} else if(!strcmp(strbuf, "use_image_scale")) {
			gameConf.use_image_scale = atoi(valbuf) > 0;
		} else if(!strcmp(strbuf, "errorlog")) {
			unsigned int val = strtol(valbuf, nullptr, 10);
			enable_log = val & 0xff;
		} else if(!strcmp(strbuf, "textfont")) {
			BufferIO::DecodeUTF8(valbuf, wstr);
			int textfontsize;
			sscanf(linebuf, "%s = %s %d", strbuf, valbuf, &textfontsize);
			gameConf.textfontsize = textfontsize;
			BufferIO::CopyWStr(wstr, gameConf.textfont, 256);
		} else if(!strcmp(strbuf, "numfont")) {
			BufferIO::DecodeUTF8(valbuf, wstr);
			BufferIO::CopyWStr(wstr, gameConf.numfont, 256);
		} else if(!strcmp(strbuf, "serverport")) {
			gameConf.serverport = atoi(valbuf);
		} else if(!strcmp(strbuf, "game_version")) {
			gameConf.game_version = atoi(valbuf);
		} else if(!strcmp(strbuf, "lasthost")) {
			BufferIO::DecodeUTF8(valbuf, wstr);
			BufferIO::CopyWStr(wstr, gameConf.lasthost, 100);
		} else if(!strcmp(strbuf, "lastport")) {
			BufferIO::DecodeUTF8(valbuf, wstr);
			BufferIO::CopyWStr(wstr, gameConf.lastport, 20);
		} else if(!strcmp(strbuf, "roompass")) {
			BufferIO::DecodeUTF8(valbuf, wstr);
			BufferIO::CopyWStr(wstr, gameConf.roompass, 20);
		} else if(!strcmp(strbuf, "automonsterpos")) {
			gameConf.chkMAutoPos = atoi(valbuf);
		} else if(!strcmp(strbuf, "autospellpos")) {
			gameConf.chkSTAutoPos = atoi(valbuf);
		} else if(!strcmp(strbuf, "randompos")) {
			gameConf.chkRandomPos = atoi(valbuf);
		} else if(!strcmp(strbuf, "autochain")) {
			gameConf.chkAutoChain = atoi(valbuf);
		} else if(!strcmp(strbuf, "waitchain")) {
			gameConf.chkWaitChain = atoi(valbuf);
		} else if(!strcmp(strbuf, "mute_opponent")) {
			gameConf.chkIgnore1 = atoi(valbuf);
		} else if(!strcmp(strbuf, "mute_spectators")) {
			gameConf.chkIgnore2 = atoi(valbuf);
		} else if(!strcmp(strbuf, "default_rule")) {
			gameConf.default_rule = atoi(valbuf);
			if(gameConf.default_rule <= 0)
				gameConf.default_rule = DEFAULT_DUEL_RULE;
		} else if(!strcmp(strbuf, "hide_setname")) {
			gameConf.chkHideSetname = atoi(valbuf);
		} else if(!strcmp(strbuf, "hide_hint_button")) {
			gameConf.chkHideHintButton = atoi(valbuf);
		} else if(!strcmp(strbuf, "control_mode")) {
			gameConf.control_mode = atoi(valbuf);
		} else if(!strcmp(strbuf, "draw_field_spell")) {
			gameConf.draw_field_spell = atoi(valbuf);
		} else if(!strcmp(strbuf, "separate_clear_button")) {
			gameConf.separate_clear_button = atoi(valbuf);
		} else if(!strcmp(strbuf, "auto_search_limit")) {
			gameConf.auto_search_limit = atoi(valbuf);
			} else if(!strcmp(strbuf, "search_multiple_keywords")) {
				gameConf.search_multiple_keywords = atoi(valbuf);
			} else if(!strcmp(strbuf, "search_regex")) {
				gameConf.search_regex = atoi(valbuf);
		} else if(!strcmp(strbuf, "ignore_deck_changes")) {
			gameConf.chkIgnoreDeckChanges = atoi(valbuf);
		} else if(!strcmp(strbuf, "default_ot")) {
			gameConf.defaultOT = atoi(valbuf);
		} else if(!strcmp(strbuf, "enable_bot_mode")) {
			gameConf.enable_bot_mode = atoi(valbuf);
			} else if(!strcmp(strbuf, "quick_animation")) {
				gameConf.quick_animation = atoi(valbuf);
			} else if(!strcmp(strbuf, "auto_save_replay")) {
				gameConf.auto_save_replay = atoi(valbuf);
		} else if(!strcmp(strbuf, "ask_mset")) {
			gameConf.ask_mset = atoi(valbuf);
#ifdef YGOPRO_USE_AUDIO
			} else if(!strcmp(strbuf, "enable_sound")) {
				gameConf.enable_sound = atoi(valbuf) > 0;
			} else if(!strcmp(strbuf, "sound_volume")) {
			int vol = strtol(valbuf, nullptr, 10);
			if (vol < 0)
				vol = 0;
			else if (vol > 100)
				vol = 100;
			gameConf.sound_volume = (double)vol / 100;
			} else if(!strcmp(strbuf, "enable_music")) {
				gameConf.enable_music = atoi(valbuf) > 0;
			} else if(!strcmp(strbuf, "music_volume")) {
			int vol = strtol(valbuf, nullptr, 10);
			if (vol < 0)
				vol = 0;
			else if (vol > 100)
				vol = 100;
			gameConf.music_volume = (double)vol / 100;
			} else if(!strcmp(strbuf, "music_mode")) {
				gameConf.music_mode = atoi(valbuf);
#endif
		} else if(!strcmp(strbuf, "enable_pendulum_scale")) {
			gameConf.chkEnablePScale = atoi(valbuf);
		} else if (!strcmp(strbuf, "random_drawing")) {
			gameConf.random = atoi(valbuf);
		} else if(!strcmp(strbuf, "window_maximized")) {
			gameConf.window_maximized = atoi(valbuf) > 0;
		} else if(!strcmp(strbuf, "window_width")) {
			gameConf.window_width = atoi(valbuf);
		} else if(!strcmp(strbuf, "window_height")) {
			gameConf.window_height = atoi(valbuf);
		} else if(!strcmp(strbuf, "resize_popup_menu")) {
			gameConf.resize_popup_menu = atoi(valbuf) > 0;
		} else {
			// options allowing multiple words
			sscanf(linebuf, "%s = %240[^\n]", strbuf, valbuf);
			if (!strcmp(strbuf, "nickname")) {
				BufferIO::DecodeUTF8(valbuf, wstr);
				BufferIO::CopyWStr(wstr, gameConf.nickname, 20);
			} else if (!strcmp(strbuf, "gamename")) {
				BufferIO::DecodeUTF8(valbuf, wstr);
				BufferIO::CopyWStr(wstr, gameConf.gamename, 20);
			} else if (!strcmp(strbuf, "searchname")) {
				BufferIO::DecodeUTF8(valbuf, wstr);
				BufferIO::CopyWStr(wstr, gameConf.searchname, 256);
			} else if (!strcmp(strbuf, "lastdeck")) {
				BufferIO::DecodeUTF8(valbuf, wstr);
				BufferIO::CopyWStr(wstr, gameConf.lastdeck, 64);
			}
		}
	}
	fclose(fp);
}
void Game::SaveConfig() {
	FILE* fp = fopen("system.conf", "w");
	fprintf(fp, "#config file\n#nickname & gamename should be less than 20 characters\n");
	char linebuf[256];
	fprintf(fp, "use_d3d = %d\n", gameConf.use_d3d ? 1 : 0);
	fprintf(fp, "use_image_scale = %d\n", gameConf.use_image_scale ? 1 : 0);
	fprintf(fp, "antialias = %d\n", gameConf.antialias);
	fprintf(fp, "errorlog = %d\n", enable_log);
	fprintf(fp, "game_version = %d\n", gameConf.game_version);
	BufferIO::CopyWStr(ebNickName->getText(), gameConf.nickname, 20);
	BufferIO::EncodeUTF8(gameConf.nickname, linebuf);
	fprintf(fp, "nickname = %s\n", linebuf);
	BufferIO::EncodeUTF8(gameConf.gamename, linebuf);
	fprintf(fp, "gamename = %s\n", linebuf);
	BufferIO::EncodeUTF8(gameConf.lastdeck, linebuf);
	fprintf(fp, "lastdeck = %s\n", linebuf);
	BufferIO::EncodeUTF8(gameConf.textfont, linebuf);
	fprintf(fp, "textfont = %s %d\n", linebuf, gameConf.textfontsize);
	BufferIO::EncodeUTF8(gameConf.numfont, linebuf);
	fprintf(fp, "numfont = %s\n", linebuf);
	fprintf(fp, "serverport = %d\n", gameConf.serverport);
	BufferIO::EncodeUTF8(gameConf.lasthost, linebuf);
	fprintf(fp, "lasthost = %s\n", linebuf);
	BufferIO::EncodeUTF8(gameConf.lastport, linebuf);
	fprintf(fp, "lastport = %s\n", linebuf);
	//settings
	fprintf(fp, "automonsterpos = %d\n", (chkMAutoPos->isChecked() ? 1 : 0));
	fprintf(fp, "autospellpos = %d\n", (chkSTAutoPos->isChecked() ? 1 : 0));
	fprintf(fp, "randompos = %d\n", (chkRandomPos->isChecked() ? 1 : 0));
	fprintf(fp, "autochain = %d\n", (chkAutoChain->isChecked() ? 1 : 0));
	fprintf(fp, "waitchain = %d\n", (chkWaitChain->isChecked() ? 1 : 0));
	fprintf(fp, "mute_opponent = %d\n", (chkIgnore1->isChecked() ? 1 : 0));
	fprintf(fp, "mute_spectators = %d\n", (chkIgnore2->isChecked() ? 1 : 0));
	fprintf(fp, "default_rule = %d\n", gameConf.default_rule == DEFAULT_DUEL_RULE ? 0 : gameConf.default_rule);
	fprintf(fp, "hide_setname = %d\n", (chkHideSetname->isChecked() ? 1 : 0));
	fprintf(fp, "hide_hint_button = %d\n", (chkHideHintButton->isChecked() ? 1 : 0));
	fprintf(fp, "#control_mode = 0: Key A/S/D/R Chain Buttons. control_mode = 1: MouseLeft/MouseRight/NULL/F9 Without Chain Buttons\n");
	fprintf(fp, "control_mode = %d\n", gameConf.control_mode);
	fprintf(fp, "draw_field_spell = %d\n", gameConf.draw_field_spell);
	fprintf(fp, "separate_clear_button = %d\n", gameConf.separate_clear_button);
	fprintf(fp, "#auto_search_limit >= 0: Start search automatically when the user enters N chars\n");
	fprintf(fp, "auto_search_limit = %d\n", gameConf.auto_search_limit);
	fprintf(fp, "#search_multiple_keywords = 0: Disable. 1: Search mutiple keywords with separator \" \". 2: with separator \"+\"\n");
	fprintf(fp, "search_multiple_keywords = %d\n", gameConf.search_multiple_keywords);
	fprintf(fp, "search_regex = %d\n", gameConf.search_regex);
	fprintf(fp, "ignore_deck_changes = %d\n", (chkIgnoreDeckChanges->isChecked() ? 1 : 0));
	fprintf(fp, "default_ot = %d\n", gameConf.defaultOT);
	fprintf(fp, "enable_bot_mode = %d\n", gameConf.enable_bot_mode);
	fprintf(fp, "quick_animation = %d\n", gameConf.quick_animation);
	fprintf(fp, "auto_save_replay = %d\n", (chkAutoSaveReplay->isChecked() ? 1 : 0));
	fprintf(fp, "random_drawing = %d\n", gameConf.random);
	fprintf(fp, "ask_mset = %d\n", gameConf.ask_mset);
	fprintf(fp, "enable_sound = %d\n", (chkEnableSound->isChecked() ? 1 : 0));
	fprintf(fp, "enable_music = %d\n", (chkEnableMusic->isChecked() ? 1 : 0));
	fprintf(fp, "enable_pendulum_scale = %d\n", ((mainGame->chkEnablePScale->isChecked()) ? 1 : 0));
	fprintf(fp, "#Volume of sound and music, between 0 and 100\n");
	int vol = gameConf.sound_volume * 100;
	if(vol < 0) vol = 0; else if(vol > 100) vol = 100;
	fprintf(fp, "sound_volume = %d\n", vol);
	vol = gameConf.music_volume * 100;
	if(vol < 0) vol = 0; else if(vol > 100) vol = 100;
	fprintf(fp, "music_volume = %d\n", vol);
	fprintf(fp, "music_mode = %d\n", (chkMusicMode->isChecked() ? 1 : 0));
	fprintf(fp, "window_maximized = %d\n", (gameConf.window_maximized ? 1 : 0));
	fprintf(fp, "window_width = %d\n", gameConf.window_width);
	fprintf(fp, "window_height = %d\n", gameConf.window_height);
	fprintf(fp, "resize_popup_menu = %d\n", gameConf.resize_popup_menu ? 1 : 0);
	fclose(fp);
}
void Game::ShowCardInfo(int code, bool resize) {
	if(showingcode == code && !resize)
		return;
	wchar_t formatBuffer[256];
	auto cit = dataManager.GetCodePointer(code);
	bool is_valid = (cit != dataManager.datas_end);
	imgCard->setImage(imageManager.GetTexture(code, true));
	if (is_valid) {
		auto& cd = cit->second;
		if (cd.is_alternative())
			myswprintf(formatBuffer, L"%ls[%08d]", dataManager.GetName(cd.alias), cd.alias);
		else
			myswprintf(formatBuffer, L"%ls[%08d]", dataManager.GetName(code), code);
	}
	else {
		myswprintf(formatBuffer, L"%ls[%08d]", dataManager.GetName(code), code);
	}
	stName->setText(formatBuffer);
	int offset = 0;
	if(is_valid && !chkHideSetname->isChecked()) {
		auto& cd = cit->second;
		auto target = cit;
		if (cd.alias && dataManager.GetCodePointer(cd.alias) != dataManager.datas_end) {
			target = dataManager.GetCodePointer(cd.alias);
		}
		if (target->second.setcode[0]) {
			offset = 23;// *yScale;
			myswprintf(formatBuffer, L"%ls%ls", dataManager.GetSysString(1329), dataManager.FormatSetName(target->second.setcode));
			stSetName->setText(formatBuffer);
		}
		else
			stSetName->setText(L"");
	}
	else {
		stSetName->setText(L"");
	}
	if(is_valid && cit->second.type & TYPE_MONSTER) {
		auto& cd = cit->second;
		myswprintf(formatBuffer, L"[%ls] %ls/%ls", dataManager.FormatType(cd.type), dataManager.FormatRace(cd.race), dataManager.FormatAttribute(cd.attribute));
		stInfo->setText(formatBuffer);
		int offset_info = 0;
		irr::core::dimension2d<unsigned int> dtxt = mainGame->guiFont->getDimension(formatBuffer);
		if(dtxt.Width > (300 * xScale - 13) - 15)
			offset_info = 15;
		if(!(cd.type & TYPE_LINK)) {
			const wchar_t* form = L"\u2605";
			if(cd.type & TYPE_XYZ) form = L"\u2606";
			myswprintf(formatBuffer, L"[%ls%d] ", form, cd.level);
			wchar_t adBuffer[16];
			if(cd.attack < 0 && cd.defense < 0)
				myswprintf(adBuffer, L"?/?");
			else if(cd.attack < 0)
				myswprintf(adBuffer, L"?/%d", cd.defense);
			else if(cd.defense < 0)
				myswprintf(adBuffer, L"%d/?", cd.attack);
			else
				myswprintf(adBuffer, L"%d/%d", cd.attack, cd.defense);
			wcscat(formatBuffer, adBuffer);
		} else {
			myswprintf(formatBuffer, L"[LINK-%d] ", cd.level);
			wchar_t adBuffer[16];
			if(cd.attack < 0)
				myswprintf(adBuffer, L"?/-   ");
			else
				myswprintf(adBuffer, L"%d/-   ", cd.attack);
			wcscat(formatBuffer, adBuffer);
			wcscat(formatBuffer, dataManager.FormatLinkMarker(cd.link_marker));
		}
		if(cd.type & TYPE_PENDULUM) {
			wchar_t scaleBuffer[16];
			myswprintf(scaleBuffer, L"   %d/%d", cd.lscale, cd.rscale);
			wcscat(formatBuffer, scaleBuffer);
		}
		stDataInfo->setText(formatBuffer);
		int offset_arrows = offset_info;
		dtxt = mainGame->guiFont->getDimension(formatBuffer);
		if(dtxt.Width > (300 * xScale - 13) - 15)
			offset_arrows += 15;
		stInfo->setRelativePosition(rect<s32>(15, 37, 300 * xScale - 13, (60 + offset_info)));
		stDataInfo->setRelativePosition(rect<s32>(15, (60 + offset_info), 300 * xScale - 13, (83 + offset_arrows)));
		stSetName->setRelativePosition(rect<s32>(15, (83 + offset_arrows), 296 * xScale, (83 + offset_arrows) + offset));
		stText->setRelativePosition(rect<s32>(15, (83 + offset_arrows) + offset, 287 * xScale, 324 * yScale));
		scrCardText->setRelativePosition(rect<s32>(287 * xScale - 20, (83 + offset_arrows) + offset, 287 * xScale, 324 * yScale));
	}
	else {
		if (is_valid)
			myswprintf(formatBuffer, L"[%ls]", dataManager.FormatType(cit->second.type));
		else
			myswprintf(formatBuffer, L"[%ls]", dataManager.FormatType(0));
		stInfo->setText(formatBuffer);
		stDataInfo->setRelativePosition(recti(15, 60, 300 * xScale - 13, 83));
		stDataInfo->setText(L"");
		stSetName->setRelativePosition(rect<s32>(15, 60, 296 * xScale, 60 + offset));
		stText->setRelativePosition(rect<s32>(15, 60 + offset, 287 * xScale, 324 * yScale));
		scrCardText->setRelativePosition(rect<s32>(287 * xScale - 20, 60 + offset, 287 * xScale, 324 * yScale));
	}
	showingcode = code;
	showingtext = dataManager.GetText(code);
	const auto& tsize = stText->getRelativePosition();
	InitStaticText(stText, tsize.getWidth(), tsize.getHeight(), guiFont, showingtext);
}
void Game::ClearCardInfo(int player) {
	imgCard->setImage(imageManager.tCover[player]);
	showingcode = 0;
	stName->setText(L"");
	stInfo->setText(L"");
	stDataInfo->setText(L"");
	stSetName->setText(L"");
	stText->setText(L"");
	scrCardText->setVisible(false);
}
void Game::AddLog(const wchar_t* msg, int param) {
	logParam.push_back(param);
	lstLog->addItem(msg);
	if(!env->hasFocus(lstLog)) {
		lstLog->setSelected(-1);
	}
}
void Game::AddChatMsg(const wchar_t* msg, int player, bool play_sound) {
	for(int i = 7; i > 0; --i) {
		chatMsg[i] = chatMsg[i - 1];
		chatTiming[i] = chatTiming[i - 1];
		chatType[i] = chatType[i - 1];
	}
	chatMsg[0].clear();
	chatTiming[0] = 1200;
	chatType[0] = player;
	if(play_sound)
		soundManager.PlaySoundEffect(SOUND_CHAT);
	switch(player) {
	case 0: //from host
		chatMsg[0].append(dInfo.hostname);
		chatMsg[0].append(L": ");
		break;
	case 1: //from client
		chatMsg[0].append(dInfo.clientname);
		chatMsg[0].append(L": ");
		break;
	case 2: //host tag
		chatMsg[0].append(dInfo.hostname_tag);
		chatMsg[0].append(L": ");
		break;
	case 3: //client tag
		chatMsg[0].append(dInfo.clientname_tag);
		chatMsg[0].append(L": ");
		break;
	case 7: //local name
		chatMsg[0].append(ebNickName->getText());
		chatMsg[0].append(L": ");
		break;
	case 8: //system custom message, no prefix.
		soundManager.PlaySoundEffect(SOUND_CHAT);
		chatMsg[0].append(L"[System]: ");
		break;
	case 9: //error message
		chatMsg[0].append(L"[Script Error]: ");
		break;
	default: //from watcher or unknown
		if(player < 11 || player > 19)
			chatMsg[0].append(L"[---]: ");
	}
	chatMsg[0].append(msg);
	wchar_t msg_front[256];
	myswprintf(msg_front, L"[Chat]%ls", chatMsg[0].c_str());
	AddLog(msg_front);
}
void Game::ClearChatMsg() {
	for(int i = 7; i >= 0; --i) {
		chatTiming[i] = 0;
	}
}
void Game::AddDebugMsg(const char* msg) {
	if (enable_log & 0x1) {
		wchar_t wbuf[1024];
		BufferIO::DecodeUTF8(msg, wbuf);
		AddChatMsg(wbuf, 9);
	}
	if (enable_log & 0x2) {
		char msgbuf[1040];
		snprintf(msgbuf, sizeof msgbuf, "[Script Error]: %s", msg);
		ErrorLog(msgbuf);
	}
}
void Game::ErrorLog(const char* msg) {
	FILE* fp = fopen("error.log", "at");
	if(!fp)
		return;
	time_t nowtime = time(nullptr);
	tm* localedtime = localtime(&nowtime);
	char timebuf[40];
	strftime(timebuf, 40, "%Y-%m-%d %H:%M:%S", localedtime);
	fprintf(fp, "[%s]%s\n", timebuf, msg);
	fclose(fp);
}
void Game::initUtils() {
	//user files
	FileSystem::MakeDir("replay");
	//files in ygopro-starter-pack
	FileSystem::MakeDir("deck");
	//original files
	FileSystem::MakeDir("textures");
	//subdirs in textures
	FileSystem::MakeDir("textures/act");
	FileSystem::MakeDir("textures/attack");
	FileSystem::MakeDir("textures/bg");
	FileSystem::MakeDir("textures/bg_deck");
	FileSystem::MakeDir("textures/bg_menu");
	FileSystem::MakeDir("textures/cover");
	FileSystem::MakeDir("textures/cover2");
	FileSystem::MakeDir("textures/cover3");
	FileSystem::MakeDir("textures/cover4");
	FileSystem::MakeDir("textures/pscale");
	//sound
	FileSystem::MakeDir("sound");
	FileSystem::MakeDir("sound/BGM");
	FileSystem::MakeDir("sound/BGM/advantage");
	FileSystem::MakeDir("sound/BGM/deck");
	FileSystem::MakeDir("sound/BGM/disadvantage");
	FileSystem::MakeDir("sound/BGM/duel");
	FileSystem::MakeDir("sound/BGM/lose");
	FileSystem::MakeDir("sound/BGM/menu");
	FileSystem::MakeDir("sound/BGM/win");
	//pics
	FileSystem::MakeDir("pics");
	FileSystem::MakeDir("pics/field");
}
void Game::ClearTextures() {
	matManager.mCard.setTexture(0, 0);
	imageManager.tHead[1] = imageManager.GetRandomImage(TEXTURE_HEAD_S);
	if(mainGame->gameConf.random)
		imageManager.tBackGround_deck = imageManager.GetRandomImage(TEXTURE_DECK);
	if(mainGame->gameConf.random)
		imageManager.tBackGround = imageManager.GetRandomImage(TEXTURE_DUEL);
	imgCard->setImage(imageManager.tCover[0]);
	scrCardText->setVisible(false);
	imgCard->setScaleImage(true);
	imgHead->setImage(imageManager.tHead[0]);
	imgHead->setScaleImage(true);
	imgCover->setImage(imageManager.tCover[0]);
	imgCover->setScaleImage(true);
	imgBG->setImage(imageManager.tBackGround);
	imgBG->setScaleImage(true);
	btnPSAU->setImage();
	btnPSDU->setImage();
	for(int i=0; i<=4; ++i) {
		btnCardSelect[i]->setImage();
		btnCardDisplay[i]->setImage();
	}
	imageManager.ClearTexture();
}
void Game::CloseDuelWindow() {
	for(auto wit = fadingList.begin(); wit != fadingList.end(); ++wit) {
		if(wit->isFadein)
			wit->autoFadeoutFrame = 1;
	}
	wACMessage->setVisible(false);
	wANAttribute->setVisible(false);
	wANCard->setVisible(false);
	wANNumber->setVisible(false);
	wANRace->setVisible(false);
	wCardImg->setVisible(false);
	wCardSelect->setVisible(false);
	wCardDisplay->setVisible(false);
	wCmdMenu->setVisible(false);
	wFTSelect->setVisible(false);
	wHand->setVisible(false);
	wInfos->setVisible(false);
	wMessage->setVisible(false);
	wOptions->setVisible(false);
	wPhase->setVisible(false);
	wPosSelect->setVisible(false);
	wSearchWindow->setVisible(false);
	wQuery->setVisible(false);
	wSurrender->setVisible(false);
	wReplayControl->setVisible(false);
	wReplaySave->setVisible(false);
	stHintMsg->setVisible(false);
	btnSideOK->setVisible(false);
	btnSideShuffle->setVisible(false);
	btnSideSort->setVisible(false);
	btnSideReload->setVisible(false);
	btnLeaveGame->setVisible(false);
	btnSpectatorSwap->setVisible(false);
	btnChainIgnore->setVisible(false);
	btnChainAlways->setVisible(false);
	btnChainWhenAvail->setVisible(false);
	btnCancelOrFinish->setVisible(false);
	btnShuffle->setVisible(false);
	wChat->setVisible(false);
	lstLog->clear();
	logParam.clear();
	lstHostList->clear();
	DuelClient::hosts.clear();
	ClearTextures();
	ResizeChatInputWindow();
	closeDoneSignal.Set();
}
int Game::LocalPlayer(int player) const {
	int pid = player ? 1 : 0;
	return dInfo.isFirst ? pid : 1 - pid;
}
int Game::OppositePlayer(int player) {
	auto player_side_bit = dInfo.isTag ? 0x2 : 0x1;
	return player ^ player_side_bit;
}
int Game::ChatLocalPlayer(int player) {
	if(player > 3)
		return player;
	bool is_self;
	if(dInfo.isStarted || is_siding) {
		if(dInfo.isInDuel)
			// when in duel
			player = mainGame->dInfo.isFirst ? player : OppositePlayer(player);
		else {
			// when changing side or waiting tp result
			auto selftype_boundary = dInfo.isTag ? 2 : 1;
			if(DuelClient::selftype >= selftype_boundary && DuelClient::selftype < 4)
				player = OppositePlayer(player);
		}
		if (DuelClient::selftype >= 4) {
			is_self = false;
		} else if (dInfo.isTag) {
			is_self =  (player & 0x2) == 0 && (player & 0x1) == (DuelClient::selftype & 0x1);
		} else {
			is_self = player == 0;
		}
	} else {
		// when in lobby
		is_self = player == DuelClient::selftype;
	}
	if(dInfo.isTag && (player == 1 || player == 2)) {
		player = 3 - player;
	}
	return player | (is_self ? 0x10 : 0);
}
const wchar_t* Game::LocalName(int local_player) {
	return local_player == 0 ? dInfo.hostname : dInfo.clientname;
}
void Game::OnResize() {
#ifdef _WIN32
	WINDOWPLACEMENT plc;
	plc.length = sizeof(WINDOWPLACEMENT);
	if(GetWindowPlacement(hWnd, &plc))
		gameConf.window_maximized = (plc.showCmd == SW_SHOWMAXIMIZED);
#endif // _WIN32
	if(!gameConf.window_maximized) {
		gameConf.window_width = window_size.Width;
		gameConf.window_height = window_size.Height;
	}

	irr::gui::CGUITTFont* old_numFont = numFont;
	irr::gui::CGUITTFont* old_adFont = adFont;
	irr::gui::CGUITTFont* old_lpcFont = lpcFont;
	irr::gui::CGUITTFont* old_textFont = textFont;
	numFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, (yScale > 0.5 ? 16 * yScale : 8));
	adFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, (yScale > 0.75 ? 12 * yScale : 9));
	lpcFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.numfont, 48 * yScale);
	textFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.textfont, (yScale > 0.642 ? gameConf.textfontsize * yScale : 9));
	old_numFont->drop();
	old_adFont->drop();
	old_lpcFont->drop();
	//guiFont = irr::gui::CGUITTFont::createTTFont(env, gameConf.textfont, gameConf.textfontsize * yScale);
	//env->getSkin()->setFont(guiFont);

	wMainMenu->setRelativePosition(ResizeWin(370, 200, 650, 485));
	wOther->setRelativePosition(ResizeWin(370, 185, 650, 430));
	wSystem->setRelativePosition(ResizeWin(215, 160, 815, 365));
	wHDS->setRelativePosition(ResizeWin(342, 235, 682, 395));
	wCRS->setRelativePosition(ResizeWin(269.5, 187.5, 774.5, 452.5));
	wBGS->setRelativePosition(ResizeWin(132, 100, 882, 500));
	wDeckEdit->setRelativePosition(Resize(309, 8, 605, 130));
	cbDBLFList->setRelativePosition(Resize(80, 5, 220, 30));
	cbDBDecks->setRelativePosition(Resize(80, 35, 220, 60));
	btnClearDeck->setRelativePosition(Resize(115, 99, 165, 120));
	btnSortDeck->setRelativePosition(Resize(60, 99, 110, 120));
	btnShuffleDeck->setRelativePosition(Resize(5, 99, 55, 120));
	btnSaveDeck->setRelativePosition(Resize(225, 35, 290, 60));
	btnSaveDeckAs->setRelativePosition(Resize(225, 65, 290, 90));
	ebDeckname->setRelativePosition(Resize(80, 65, 220, 90));

	wSort->setRelativePosition(Resize(930, 132, 1020, 156));
	cbSortType->setRelativePosition(Resize(10, 2, 85, 22));
	wFilter->setRelativePosition(Resize(610, 8, 1020, 130));
	scrFilter->setRelativePosition(Resize(999, 161, 1019, 629));
	cbCardType->setRelativePosition(Resize(60, 3, 120, 23));
	cbCardType2->setRelativePosition(Resize(130, 3, 190, 23));
	cbRace->setRelativePosition(Resize(60, 49, 190, 69));
	cbAttribute->setRelativePosition(Resize(60, 26, 190, 46));
	cbLimit->setRelativePosition(Resize(260, 3, 390, 23));
	ebStar->setRelativePosition(Resize(60, 72, 100, 92));
	ebScale->setRelativePosition(Resize(150, 72, 190, 92));
	ebAttack->setRelativePosition(Resize(260, 26, 340, 46));
	ebDefense->setRelativePosition(Resize(260, 49, 340, 69));
	ebCardName->setRelativePosition(Resize(260, 72, 390, 92));
	btnEffectFilter->setRelativePosition(Resize(345, 28, 390, 69));
	btnStartFilter->setRelativePosition(Resize(260, 80 + 125 / 6, 390, 100 + 125 / 6));
	if(btnClearFilter)
		btnClearFilter->setRelativePosition(Resize(205, 80 + 125 / 6, 255, 100 + 125 / 6));
	btnMarksFilter->setRelativePosition(Resize(60, 80 + 125 / 6, 190, 100 + 125 / 6));

	recti btncatepos = btnEffectFilter->getAbsolutePosition();
	wCategories->setRelativePosition(recti(
		btncatepos.LowerRightCorner.X - wCategories->getRelativePosition().getWidth(),
		btncatepos.LowerRightCorner.Y - btncatepos.getHeight() / 2,
		btncatepos.LowerRightCorner.X,
		btncatepos.LowerRightCorner.Y - btncatepos.getHeight() / 2 + 245));

	wLinkMarks->setRelativePosition(ResizeWin(700, 30, 820, 150));
	stBanlist->setRelativePosition(ResizeWin(10, 9, 100, 29));
	stDeck->setRelativePosition(ResizeWin(10, 39, 100, 59));
	stCategory->setRelativePosition(ResizeWin(10, 5, 70, 25));
	stLimit->setRelativePosition(ResizeWin(205, 5, 280, 25));
	stAttribute->setRelativePosition(ResizeWin(10, 28, 70, 48));
	stRace->setRelativePosition(ResizeWin(10, 51, 70, 71));
	stAttack->setRelativePosition(ResizeWin(205, 28, 280, 48));
	stDefense->setRelativePosition(ResizeWin(205, 51, 280, 71));
	stStar->setRelativePosition(ResizeWin(10, 74, 80, 94));
	stSearch->setRelativePosition(ResizeWin(205, 74, 280, 94));
	stScale->setRelativePosition(ResizeWin(110, 74, 150, 94));
	btnSideOK->setRelativePosition(Resize(510, 40, 820, 80));
	btnSideShuffle->setRelativePosition(Resize(310, 100, 370, 130));
	btnSideSort->setRelativePosition(Resize(375, 100, 435, 130));
	btnSideReload->setRelativePosition(Resize(440, 100, 500, 130));
	btnDeleteDeck->setRelativePosition(Resize(225, 95, 290, 120));
	btnRenameDeck->setRelativePosition(Resize(170, 99, 220, 120));

	wLanWindow->setRelativePosition(ResizeWin(220, 100, 800, 520));
	recti btnCRpos = wLanWindow->getAbsolutePosition();
	wSC->setRelativePosition(recti(
		btnCRpos.LowerRightCorner.X - 860,
		btnCRpos.LowerRightCorner.Y - 435,
		btnCRpos.LowerRightCorner.X - 580,
		btnCRpos.LowerRightCorner.Y - 43));
	wRM->setRelativePosition(recti(
		btnCRpos.LowerRightCorner.X - 530,
		btnCRpos.LowerRightCorner.Y - 455,
		btnCRpos.LowerRightCorner.X - 100,
		btnCRpos.LowerRightCorner.Y - 95));
	wCreateHost->setRelativePosition(ResizeWin(320, 100, 700, 520));
	wHostPrepare->setRelativePosition(ResizeWin(270, 120, 750, 440));
	wReplay->setRelativePosition(ResizeWin(220, 100, 800, 520));
	wSinglePlay->setRelativePosition(ResizeWin(220, 100, 800, 520));

	wHand->setRelativePosition(ResizeWin(500, 450, 825, 605));
	wFTSelect->setRelativePosition(ResizeWin(550, 240, 780, 340));
	wMessage->setRelativePosition(ResizeWin(490, 200, 840, 340));
	wACMessage->setRelativePosition(ResizeWin(490, 240, 840, 300));
	wASMessage->setRelativePosition(ResizeWin(337, 290, 687, 350));
	wQuery->setRelativePosition(ResizeWin(490, 200, 840, 340));
	wSurrender->setRelativePosition(ResizeWin(490, 200, 840, 340));
	wOptions->setRelativePosition(ResizeWin(490, 200, 840, 340));
	wPosSelect->setRelativePosition(ResizeWin(340, 200, 935, 410));
	wCardSelect->setRelativePosition(ResizeWin(320, 100, 1000, 400));
	wANNumber->setRelativePosition(ResizeWin(550, 200, 780, 295));
	wANCard->setRelativePosition(ResizeWin(510, 120, 820, 420));
	wANAttribute->setRelativePosition(ResizeWin(500, 200, 830, 285));
	wANRace->setRelativePosition(ResizeWin(480, 200, 850, 410));
	wReplaySave->setRelativePosition(ResizeWin(510, 200, 820, 320));
	recti btnREpos = btnRenameDeck->getAbsolutePosition();
	wRenameDeck->setRelativePosition(recti(
		btnREpos.LowerRightCorner.X - 310,
		btnREpos.LowerRightCorner.Y - 1,
		btnREpos.LowerRightCorner.X,
		btnREpos.LowerRightCorner.Y + 119));
	stHintMsg->setRelativePosition(ResizeWin(500, 82, 820, 112));

	//sound / music volume bar
	scrSoundVolume->setRelativePosition(recti(scrSoundVolume->getRelativePosition().UpperLeftCorner.X, scrSoundVolume->getRelativePosition().UpperLeftCorner.Y, 20 + (300 * xScale) - 70, scrSoundVolume->getRelativePosition().LowerRightCorner.Y));
	scrMusicVolume->setRelativePosition(recti(scrMusicVolume->getRelativePosition().UpperLeftCorner.X, scrMusicVolume->getRelativePosition().UpperLeftCorner.Y, 20 + (300 * xScale) - 70, scrMusicVolume->getRelativePosition().LowerRightCorner.Y));
	cbFont->setRelativePosition(recti(427, 85, 590, 110));

	recti tabHelperPos = recti(0, 0, 300 * xScale - 50, 365 * yScale - 65);
	tabHelper->setRelativePosition(tabHelperPos);
	scrTabHelper->setRelativePosition(recti(tabHelperPos.LowerRightCorner.X + 2, 0, tabHelperPos.LowerRightCorner.X + 22, tabHelperPos.LowerRightCorner.Y));
	s32 tabHelperLastY = elmTabHelperLast->getRelativePosition().LowerRightCorner.Y;
	if(tabHelperLastY > tabHelperPos.LowerRightCorner.Y) {
		scrTabHelper->setMax(tabHelperLastY - tabHelperPos.LowerRightCorner.Y + 5);
		scrTabHelper->setPos(0);
		scrTabHelper->setVisible(true);
	}
	else
		scrTabHelper->setVisible(false);

	recti tabSystemPos = recti(0, 0, 300 * xScale - 50, 365 * yScale - 65);
	tabSystem->setRelativePosition(tabSystemPos);
	scrTabSystem->setRelativePosition(recti(tabSystemPos.LowerRightCorner.X + 2, 0, tabSystemPos.LowerRightCorner.X + 22, tabSystemPos.LowerRightCorner.Y));
	s32 tabSystemLastY = elmTabSystemLast->getRelativePosition().LowerRightCorner.Y;
	if(tabSystemLastY > tabSystemPos.LowerRightCorner.Y) {
		scrTabSystem->setMax(tabSystemLastY - tabSystemPos.LowerRightCorner.Y + 5);
		scrTabSystem->setPos(0);
		scrTabSystem->setVisible(true);
	} else
		scrTabSystem->setVisible(false);

	if(gameConf.resize_popup_menu) {
		int width = 100 * mainGame->xScale;
		int height = (mainGame->yScale >= 0.666) ? 21 * mainGame->yScale : 14;
		wCmdMenu->setRelativePosition(recti(1, 1, width + 1, 1));
		btnActivate->setRelativePosition(recti(1, 1, width, height));
		btnSummon->setRelativePosition(recti(1, 1, width, height));
		btnSPSummon->setRelativePosition(recti(1, 1, width, height));
		btnMSet->setRelativePosition(recti(1, 1, width, height));
		btnSSet->setRelativePosition(recti(1, 1, width, height));
		btnRepos->setRelativePosition(recti(1, 1, width, height));
		btnAttack->setRelativePosition(recti(1, 1, width, height));
		btnActivate->setRelativePosition(recti(1, 1, width, height));
		btnShowList->setRelativePosition(recti(1, 1, width, height));
		btnOperation->setRelativePosition(recti(1, 1, width, height));
		btnReset->setRelativePosition(recti(1, 1, width, height));
	}

	wCardImg->setRelativePosition(ResizeCard(1, 1, 20, 18));
	imgCard->setRelativePosition(ResizeCard(10, 9, 0, 0));
	imgCard->setScaleImage(true);
	wInfos->setRelativePosition(Resize(1, 275, 301, 639));
	stName->setRelativePosition(recti(10, 10, 287 * xScale, 32));
	lstLog->setRelativePosition(Resize(10, 10, 290, 290));
	//const auto& tsize = stText->getRelativePosition();
	if(showingcode)
		ShowCardInfo(showingcode, true);
	btnClearLog->setRelativePosition(Resize(160, 300, 260, 325));
	btnCardSearch->setRelativePosition(Resize(150, 20, 250, 70));
	recti btnCSpos = btnCardSearch->getAbsolutePosition();
	wSearchWindow->setRelativePosition(recti(
		btnCSpos.LowerRightCorner.X - 310,
		btnCSpos.LowerRightCorner.Y - 1,
		btnCSpos.LowerRightCorner.X,
		btnCSpos.LowerRightCorner.Y + 119));

	wPhase->setRelativePosition(Resize(480, 310, 855, 330));
	btnPhaseStatus->setRelativePosition(Resize(0, 0, 50, 20));
	btnBP->setRelativePosition(Resize(160, 0, 210, 20));
	btnM2->setRelativePosition(Resize(160, 0, 210, 20));
	btnEP->setRelativePosition(Resize(320, 0, 370, 20));

	ResizeChatInputWindow();

	btnLeaveGame->setRelativePosition(Resize(205, 5, 295, 80));
	wReplayControl->setRelativePosition(Resize(205, 143, 295, 273));
	btnReplayStart->setRelativePosition(Resize(5, 5, 85, 25));
	btnReplayPause->setRelativePosition(Resize(5, 5, 85, 25));
	btnReplayStep->setRelativePosition(Resize(5, 55, 85, 75));
	btnReplayUndo->setRelativePosition(Resize(5, 80, 85, 100));
	btnReplaySwap->setRelativePosition(Resize(5, 30, 85, 50));
	btnReplayExit->setRelativePosition(Resize(5, 105, 85, 125));

	btnSpectatorSwap->setRelativePosition(Resize(205, 100, 295, 135));
	btnChainAlways->setRelativePosition(Resize(205, 140, 295, 175));
	btnChainIgnore->setRelativePosition(Resize(205, 100, 295, 135));
	btnChainWhenAvail->setRelativePosition(Resize(205, 180, 295, 215));
	btnShuffle->setRelativePosition(Resize(205, 230, 295, 265));
	btnCancelOrFinish->setRelativePosition(Resize(205, 230, 295, 265));

	imageManager.ClearTexture();
}
void Game::ResizeChatInputWindow() {
	s32 x = wInfos->getRelativePosition().LowerRightCorner.X + 6;
	if(is_building) x = 802 * xScale;
	wChat->setRelativePosition(recti(x, window_size.Height - 25, window_size.Width, window_size.Height));
	ebChatInput->setRelativePosition(recti(3, 2, window_size.Width - wChat->getRelativePosition().UpperLeftCorner.X - 6, 22));
}
recti Game::Resize(s32 x, s32 y, s32 x2, s32 y2) {
	x = x * xScale;
	y = y * yScale;
	x2 = x2 * xScale;
	y2 = y2 * yScale;
	return recti(x, y, x2, y2);
}
recti Game::Resize(s32 x, s32 y, s32 x2, s32 y2, s32 dx, s32 dy, s32 dx2, s32 dy2) {
	x = x * xScale + dx;
	y = y * yScale + dy;
	x2 = x2 * xScale + dx2;
	y2 = y2 * yScale + dy2;
	return recti(x, y, x2, y2);
}
position2di Game::Resize(s32 x, s32 y) {
	x = x * xScale;
	y = y * yScale;
	return position2di(x, y);
}
position2di Game::ResizeReverse(s32 x, s32 y) {
	x = x / xScale;
	y = y / yScale;
	return position2di(x, y);
}
recti Game::ResizeWin(s32 x, s32 y, s32 x2, s32 y2, bool chat) {
	s32 sx = x2 - x;
	s32 sy = y2 - y;
	if(chat) {
		y = window_size.Height - sy;
		x2 = window_size.Width;
		y2 = y + sy;
		return recti(x, y, x2, y2);
	}
	x = (x + sx / 2) * xScale - sx / 2;
	y = (y + sy / 2) * yScale - sy / 2;
	x2 = sx + x;
	y2 = sy + y;
	return recti(x, y, x2, y2);
}
recti Game::ResizePhaseHint(s32 x, s32 y, s32 x2, s32 y2, s32 width) {
	x = x * xScale - width / 2;
	y = y * yScale;
	x2 = x2 * xScale;
	y2 = y2 * yScale;
	return recti(x, y, x2, y2);
}
recti Game::ResizeCard(s32 x, s32 y, s32 x2, s32 y2) {
	float mul = xScale;
	if(xScale > yScale)
		mul = yScale;
	s32 sx = CARD_IMG_WIDTH * mul + x2 * xScale;
	s32 sy = CARD_IMG_HEIGHT * mul + y2 * yScale;
	x = x * xScale;
	y = y * yScale;
	x2 = sx + x;
	y2 = sy + y;
	return recti(x, y, x2, y2);
}
recti Game::ResizeCardHint(s32 x, s32 y, s32 x2, s32 y2) {
	return ResizeCardMid(x, y, x2, y2, (x + x2) * 0.5, (y + y2) * 0.5);
}
position2di Game::ResizeCardHint(s32 x, s32 y) {
	return ResizeCardMid(x, y, x + CARD_IMG_WIDTH * 0.5, y + CARD_IMG_HEIGHT * 0.5);
}
recti Game::ResizeCardMid(s32 x, s32 y, s32 x2, s32 y2, s32 midx, s32 midy) {
	float mul = xScale;
	if(xScale > yScale)
		mul = yScale;
	s32 cx = midx * xScale;
	s32 cy = midy * yScale;
	x = cx + (x - midx) * mul;
	y = cy + (y - midy) * mul;
	x2 = cx + (x2 - midx) * mul;
	y2 = cy + (y2 - midy) * mul;
	return recti(x, y, x2, y2);
}
position2di Game::ResizeCardMid(s32 x, s32 y, s32 midx, s32 midy) {
	float mul = xScale;
	if(xScale > yScale)
		mul = yScale;
	s32 cx = midx * xScale;
	s32 cy = midy * yScale;
	x = cx + (x - midx) * mul;
	y = cy + (y - midy) * mul;
	return position2di(x, y);
}
recti Game::ResizeFit(s32 x, s32 y, s32 x2, s32 y2) {
	float mul = xScale;
	if(xScale > yScale)
		mul = yScale;
	x = x * mul;
	y = y * mul;
	x2 = x2 * mul;
	y2 = y2 * mul;
	return recti(x, y, x2, y2);
}
void Game::SetWindowsIcon() {
#ifdef _WIN32
	HINSTANCE hInstance = (HINSTANCE)GetModuleHandleW(nullptr);
	HICON hSmallIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	HICON hBigIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (long)hSmallIcon);
	SendMessageW(hWnd, WM_SETICON, ICON_BIG, (long)hBigIcon);
#endif
}
void Game::FlashWindow() {
#ifdef _WIN32
	FLASHWINFO fi;
	fi.cbSize = sizeof(FLASHWINFO);
	fi.hwnd = hWnd;
	fi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
	fi.uCount = 0;
	fi.dwTimeout = 0;
	FlashWindowEx(&fi);
#endif
}
void Game::takeScreenshot() {
	if(!FileSystem::IsDirExists(L"./screenshots") && !FileSystem::MakeDir(L"./screenshots"))
		return;
	irr::video::IImage* const image = driver->createScreenShot();
	if(image) {
		irr::c8 filename[64];
		snprintf(filename, 64, "screenshots/ygopro_%u.png", device->getTimer()->getRealTime());
		if (!driver->writeImageToFile(image, filename))
			device->getLogger()->log(L"Failed to take screenshot.", irr::ELL_WARNING);
		image->drop();
	} else
		device->getLogger()->log(L"Failed to take screenshot.", irr::ELL_WARNING);
}
bool Game::CheckRegEx(const std::wstring& text, const std::wstring& exp, bool exact) {
	if(!gameConf.search_regex)
		return false;
	bool result;
	try {
		if(exact)
			result = std::regex_match(text, std::wregex(exp));
		else
			result = std::regex_search(text, std::wregex(exp));
	} catch(...) {
		result = false;
	}
	return result;
}
void Game::SetCursor(ECURSOR_ICON icon) {
	ICursorControl* cursor = mainGame->device->getCursorControl();
	if(cursor->getActiveIcon() != icon) {
		cursor->setActiveIcon(icon);
	}
}

}
