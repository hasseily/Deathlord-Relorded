#include "pch.h"
#include "AutoMap.h"
#include "Game.h"
#include "MemoryTriggers.h"
#include "resource.h"
#include "DeathlordHacks.h"
#include "TilesetCreator.h"
#include "Daytime.h"

using namespace DirectX;

// below because "The declaration of a static data member in its class definition is not a definition"
AutoMap* AutoMap::s_instance;

// In main
extern std::unique_ptr<Game>* GetGamePtr();

// This is used for any animated sprite to accelerate or slow its framerate (so one doesn't have to build 30 frames)
// This is crude compared to the way the animations class does it, but it can be kept simple
constexpr UINT STROBESLOWAVATAR = MAX(1, MAX_RENDERED_FRAMES_PER_SECOND * 6 / 30);
constexpr UINT STROBESLOWELEMENT = MAX(1, MAX_RENDERED_FRAMES_PER_SECOND * 24 / 30);
constexpr UINT STROBESLOWHIDDEN = MAX(1, MAX_RENDERED_FRAMES_PER_SECOND * 4 / 30);

constexpr UINT8 TILEID_REDRAW = 0xFF;			// when we see this nonexistent tile id we automatically redraw the tile

constexpr XMVECTORF32 COLORTRANSLUCENTWHITE = { 1.f, 1.f, 1.f, .8f };

// Tile IDs that block visibility
constexpr UINT8 TILES_DUNGEON_BLOCKVIS[0x50] = { 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
												 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
												 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
												 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
												 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

constexpr UINT8 TILES_OVERLAND_BLOCKVIS[0x50] = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
												  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
											      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
												  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
												  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

constexpr UINT8 LOS_MAX_DISTANCE = 16;	// in tiles

UINT m_avatarStrobeIdx = 0;
constexpr UINT AVATARSTROBECT = 10;
constexpr float AVATARSTROBE[AVATARSTROBECT] = { 1.13f, 1.1f, 1.05f, 1.0f, 1.0f, 1.0f, 1.0f, 1.05f, 1.10f, 1.13f };

// number of sprite frames for element tiles animation
UINT m_spriteAnimElementIdx = 0;
constexpr UINT ELEMENTSPRITECT = 7;

// number of sprite frames for hidden layer animations
UINT m_spriteAnimHiddenIdx = 0;
constexpr UINT HIDDENSPRITECT = 4;

std::string m_cursor(1, 0x7B);

// food aquisition
int m_numStepsInMap = 0;	// Number of steps taken on this map
int m_lastFoodSteps = 0;	// Steps when food was acquired last
constexpr UINT FOODFORAGINGSTEPS = 32;	// Number of steps to forage for 1 food / char

constexpr RECT RECT_SPRITE_CURSOR = { 0, 288, 32, 324 };

void AutoMap::Initialize()
{
	bShowTransition = false;
	m_avatarPosition = XMUINT2(0, 0);
	m_currentMapRect = { 0,0,0,0 };
	m_currentMapUniqueName = "";
	// CreateNewTileSpriteMap();
	// Set up the arrays for all backbuffers
	UINT bbCount = m_deviceResources->GetBackBufferCount();
	m_FogOfWarTiles = std::vector<UINT8>(MAP_LENGTH, 0x00);								// states (seen, etc...)
	m_LOSVisibilityTiles = std::vector<float>(MAP_LENGTH, 0.f);							// visibility through line of sight
	m_bbufCurrentMapTiles = std::vector(bbCount, std::vector<UINT8>(MAP_LENGTH, 0x00));	// tile id
	m_LOSRadius = 8;
}

void AutoMap::ClearMapArea()
{
	for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
		std::fill(m_bbufCurrentMapTiles[i].begin(), m_bbufCurrentMapTiles[i].end(), 0x00);
	std::fill(m_FogOfWarTiles.begin(), m_FogOfWarTiles.end(), 0x00);
	std::fill(m_LOSVisibilityTiles.begin(), m_LOSVisibilityTiles.end(), 0.f);
}

void AutoMap::ForceRedrawMapArea()
{
	// TODO: unnecessary when we redraw the whole screen every time anyway
	for (size_t i = 0; i < m_deviceResources->GetBackBufferCount(); i++)
	{
		for (size_t mapPos = 0; mapPos < m_bbufCurrentMapTiles[i].size(); mapPos++)
		{
			m_bbufCurrentMapTiles[i][mapPos] = TILEID_REDRAW;
		}
	}
}

void AutoMap::SaveCurrentMapInfo()
{
	if (m_currentMapUniqueName == "")
		return;
	g_nonVolatile.fogOfWarMarkers[m_currentMapUniqueName] = m_FogOfWarTiles;
	g_nonVolatile.SaveToDisk();
}


std::string AutoMap::GetCurrentMapUniqueName()
{
	auto memPtr = MemGetMainPtr(0);
	char _buf[60];
	if (PlayerIsOverland())
	{
		sprintf_s(_buf, sizeof(_buf), "Overland_%.2d_%.2d",
			memPtr[MAP_OVERLAND_X],
			memPtr[MAP_OVERLAND_Y]);
	}
	else {
		sprintf_s(_buf, sizeof(_buf), "Map_%.3d_%.2d_%.2d",
			memPtr[MAP_ID],
			memPtr[MAP_TYPE],
			memPtr[MAP_FLOOR]);
	}
	return std::string(_buf);
}

void AutoMap::InitializeCurrentMapInfo()
{
	if (m_currentMapUniqueName == "")
	{
		m_currentMapUniqueName = GetCurrentMapUniqueName();
		auto markers = g_nonVolatile.fogOfWarMarkers[m_currentMapUniqueName];
		if (markers.size() < MAP_LENGTH)
		{
			// Incorrect size, it was probably just initialized to a 0 size
			// reset it
			markers.resize(MAP_LENGTH, 0);
		}
		m_FogOfWarTiles = markers;
	}
}

void AutoMap::SetShowTransition(bool showTransition)
{
	if (showTransition == bShowTransition)
		return;
	bShowTransition = showTransition;
	if (bShowTransition == true)
	{
		//OutputDebugString(L"Started transition!\n");
		// Save the old map
		SaveCurrentMapInfo();
		// We'll set the new map name later when we draw it
		// We don't know yet if the memory is ready for that
		ClearMapArea();
		//ForceRedrawMapArea();
		m_currentMapUniqueName = "";
		m_numStepsInMap = 0;
		m_lastFoodSteps = 0;
	}
	else
	{
		//OutputDebugString(L"       Ended transition!\n");
	}
}

#pragma warning(push)
#pragma warning(disable : 26451)
bool AutoMap::UpdateAvatarPositionOnAutoMap(UINT x, UINT y)
{
	// Make sure we don't draw on the wrong map!
	if (m_currentMapUniqueName != GetCurrentMapUniqueName())
		InitializeCurrentMapInfo();

	UINT32 cleanX = (x < MAP_WIDTH  ? x : MAP_WIDTH - 1);
	UINT32 cleanY = (y < MAP_HEIGHT ? y : MAP_HEIGHT - 1);
	if ((m_avatarPosition.x == cleanX) && (m_avatarPosition.y == cleanY))
		return false;

	++m_numStepsInMap;

	// We redraw all the tiles in the viewport later, so here just set the footsteps and LOS
	m_avatarPosition = { cleanX, cleanY };
	m_FogOfWarTiles[m_avatarPosition.x + m_avatarPosition.y * MAP_WIDTH] |= (1 << (UINT8)FogOfWarMarkers::Footstep);

	// Peasants and rangers can forage for food
	if (PlayerIsOverland())
	{
		if ((m_numStepsInMap - m_lastFoodSteps) > FOODFORAGINGSTEPS)
		{
			m_lastFoodSteps = m_numStepsInMap;
			if (PartyHasClass(DeathlordClasses::Ranger, DeathlordClasses::Peasant))
			{
				// Can forage for food. After FOODFORAGINGSTEPS, give the party 1 food
				UINT8 _foodCt;
				for (size_t i = 0; i < 6; i++)
				{
					_foodCt = MemGetMainPtr(PARTY_FOOD_START)[i];
					if (_foodCt < 99)
						MemGetMainPtr(PARTY_FOOD_START)[i] = _foodCt + 1;
				}
			}
		}
	}


	/*
	char _buf[400];
	sprintf_s(_buf, 400, "X: %03d / %03d, Y: %03d / %03d, Map name: %s\n",
		cleanX, m_avatarPosition.x, cleanY, m_avatarPosition.y, m_currentMapUniqueName.c_str());
	OutputDebugStringA(_buf);
	*/
	/*
	char _buf[500];
	sprintf_s(_buf, 500, "Old Avatar Pos tileid has values %2d, %2d in vector\n",
		m_bbufCurrentMapTiles[xx_x + xx_y * MAP_WIDTH], m_bbufCurrentMapTiles[xx_x + xx_y * MAP_WIDTH]);
	OutputDebugStringA(_buf);
	*/

	/*
	Disable the below
	TODO: This is to be removed now that we have AutoMapQuadrant::FollowPlayer

	// Automatically switch the visible quadrant if the map isn't visible in full
	AutoMapQuadrant _newQuadrant = AutoMapQuadrant::All;
	if (g_nonVolatile.mapQuadrant != AutoMapQuadrant::All)
	{
		if (m_avatarPosition.x < 0x20)
		{
			if (m_avatarPosition.y < 0x20)
				_newQuadrant = AutoMapQuadrant::TopLeft;
			else
				_newQuadrant = AutoMapQuadrant::BottomLeft;
		}
		else
		{
			if (m_avatarPosition.y < 0x20)
				_newQuadrant = AutoMapQuadrant::TopRight;
			else
				_newQuadrant = AutoMapQuadrant::BottomRight;
		}
		if (g_nonVolatile.mapQuadrant != _newQuadrant) {
			switch (_newQuadrant)
			{
			case AutoMapQuadrant::TopLeft:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYTOPLEFTQUADRANT, 1);
				break;
			case AutoMapQuadrant::BottomLeft:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYBOTTOMLEFTQUADRANT, 1);
				break;
			case AutoMapQuadrant::TopRight:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYTOPRIGHTQUADRANT, 1);
				break;
			case AutoMapQuadrant::BottomRight:
				PostMessageW(g_hFrameWindow, WM_COMMAND, (WPARAM)ID_AUTOMAP_DISPLAYBOTTOMRIGHTQUADRANT, 1);
				break;
			default:
				break;
			}
		}
	}
	 */
	return true;
}
#pragma region LineOfSight

void AutoMap::CalculateLOS()
{
	// Here we're just calculating pure LOS given the tiles that block the view
	int range = m_LOSRadius;	// (in tiles)
	int x, y;
	std::fill(m_LOSVisibilityTiles.begin(), m_LOSVisibilityTiles.end(), 0.f);
	// Always set the avatar tile to visible
	m_LOSVisibilityTiles[m_avatarPosition.x + m_avatarPosition.y * MAP_WIDTH] = 1.f;
	if (range == 0)
		return;
	int _centerX = (m_avatarPosition.x * FBTW) + FBTW / 2;
	int _centerY = (m_avatarPosition.y * FBTH) + FBTH / 2;
	for (double f = 0; f < 3.141592 * 2; f += 0.002) {
		x = int(range * FBTW * cos(f)) + _centerX;
		y = int(range * FBTH * sin(f)) + _centerY;
		DrawLine(_centerX, _centerY, x, y);
	}
}

void AutoMap::DrawLine(int x0, int y0, int x1, int y1) {
	int dx = abs(x0 - x1) * 2 / FBTW;
	int dy = abs(y0 - y1) * 2 / FBTH;
	double s = 0.99 / (dx > dy ? double(dx) : double(dy));
	double t = 0;
	int mapPos;
	UINT8 blockVal;	
	LPBYTE mapMemPtr = GetCurrentGameMap();

	while (t < 1.0) {
		dx = int((1.0 - t) * x0 + t * x1);
		dy = int((1.0 - t) * y0 + t * y1);
		if ((dx >= MAP_WIDTH * FBTW) | (dy >= MAP_HEIGHT * FBTH) | (dx < 0) | (dy < 0))
			goto CONT;
		if ((m_avatarPosition.x == dx / FBTW) && (m_avatarPosition.y == dy / FBTH))
		{
			goto CONT;	// Don't handle the tile the avatar is on
		}
		mapPos = (dx / FBTW) + ((dy / FBTH) * MAP_WIDTH);
		if (PlayerIsOverland())
			blockVal = TILES_OVERLAND_BLOCKVIS[mapMemPtr[mapPos] % 0x50];
		else
			blockVal = TILES_DUNGEON_BLOCKVIS[mapMemPtr[mapPos] % 0x50];
		// Always show the tile, even if it's blocking
		m_LOSVisibilityTiles[mapPos] = 1.f;
		m_FogOfWarTiles[mapPos] |= (1 << (UINT8)FogOfWarMarkers::UnFogOfWar);
		if (blockVal != 0) // It's a blocking tile. Don't show anything behind it. Exit.
			return;
CONT:
		t += s;
	}
}


bool AutoMap::UpdateLOSRadius()
{
	UINT8 _oldRadius = m_LOSRadius;
	if (PlayerIsOverland())
	{
		// Use time-of day to determine LOS radius
		float _time = Daytime::GetInstance()->TimeOfDayInFloat();
		if (_time <= 4 || _time >= 21)
			m_LOSRadius = 0;
		else if (_time >= 7 && _time <= 18)
			m_LOSRadius = LOS_MAX_DISTANCE;	// big LOS
		else if (_time < 7)	// _time is between 4 and 7
		{
			m_LOSRadius = LOS_MAX_DISTANCE * (_time - 4.f) / 3.f;
		}
		else // _time is between 18 and 21
		{
			m_LOSRadius = LOS_MAX_DISTANCE * (1.f - (_time - 18.f) / 3.f);
		}

	}
	else if (MemGetMainPtr(MAP_TYPE)[0] == (int)MapType::Town)
	{
		m_LOSRadius = LOS_MAX_DISTANCE;	// big LOS
	}
	else
	{
		if (PartyLeaderIsOfRace(DeathlordRaces::Gnome))
		{
			m_LOSRadius = LOS_MAX_DISTANCE;	// Gnomes get incredible LOS
		}
		else // default underground
		{
			m_LOSRadius = MemGetMainPtr(MAP_VISIBILITY_RADIUS)[0];
		}
	}
	// It should never be less than the original game
	// This also takes care of using torches or light spells
	if (m_LOSRadius < MemGetMainPtr(MAP_VISIBILITY_RADIUS)[0])
		m_LOSRadius = MemGetMainPtr(MAP_VISIBILITY_RADIUS)[0];
	return (_oldRadius != m_LOSRadius);
}

void AutoMap::ShouldCalcTileVisibility()
{
	m_shouldCalcTileVisibility = true;
}

void AutoMap::CalcTileVisibility(bool force)
{
	UpdateAvatarPositionOnAutoMap(MemGetMainPtr(MAP_XPOS)[0], MemGetMainPtr(MAP_YPOS)[0]);
	UpdateLOSRadius();
	if (force || m_shouldCalcTileVisibility)
		CalculateLOS();
	m_shouldCalcTileVisibility = false;
}

#pragma endregion LineOfSight

UINT8 AutoMap::StaticTileIdAtMapPosition(UINT8 x, UINT8 y)
{
	if (MemGetMainPtr(MAP_TYPE)[0] == (int)MapType::Dungeon)
	{
		UINT8* _xArray = MemGetMainPtr(DUNGEON_ARRAY_MONSTER_POSITION_X);
		UINT8* _yArray = MemGetMainPtr(DUNGEON_ARRAY_MONSTER_POSITION_Y);

		for (size_t i = 0; i < DUNGEON_ARRAY_MONSTER_SIZE; i++)
		{
			if (x == _xArray[i])
			{
				if (y == _yArray[i])
					return MemGetMainPtr(DUNGEON_ARRAY_MONSTER_TILE_ID)[i];
			}
		}
	}
	else
	{
		UINT8* _xArray = MemGetMainPtr(OVERLAND_ARRAY_MONSTER_POSITION_X);
		UINT8* _yArray = MemGetMainPtr(OVERLAND_ARRAY_MONSTER_POSITION_Y);

		for (size_t i = 0; i < OVERLAND_ARRAY_MONSTER_SIZE; i++)
		{
			if (x == _xArray[i])
			{
				if (y == _yArray[i])
					return MemGetMainPtr(OVERLAND_ARRAY_MONSTER_TILE_ID)[i];
			}
		}
	}
	// No monster is on this tile, return default
	return MemGetMainPtr(GAMEMAP_START_MEM)[x + y * MAP_WIDTH];
}

void AutoMap::ConditionallyDisplayHiddenLayerAroundPlayer(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, DirectX::CommonStates* states)
{
	if (!g_isInGameMap)
		return;
	if (PlayerIsOverland())	// Don't do anything if we're in the overland
		return;

	std::shared_ptr<DeathlordHacks>hw = GetDeathlordHacks();
	BYTE* tilesVisibleAroundAvatar = MemGetMainPtr(GAMEMAP_START_CURRENT_TILELIST);
	XMFLOAT2 spriteOrigin(0, 0);
	UINT32 fbBorderLeft = GetFrameBufferBorderWidth();	// these are additional shifts to make the tiles align
	UINT32 fbBorderTop = GetFrameBufferBorderHeight() + 16;	// these are additional shifts to make the tiles align
	float mapScale = 1.f;

	auto commandList = m_deviceResources->GetCommandList();
	spriteBatch->Begin(commandList, states->LinearWrap(), DirectX::SpriteSortMode_Deferred);

	for (UINT8 j = 3; j < 6; j++)	// rows
	{
		for (UINT8 i = 3; i < 6; i++)	// columns
		{
			UINT32 tilePosX = m_avatarPosition.x - 4 + i;
			UINT32 tilePosY = m_avatarPosition.y - 4 + j;
			if ((tilePosX >= MAP_WIDTH) || (tilePosY >= MAP_HEIGHT)) // outside the map (no need to check < 0, it'll roll over as a UINT32)
				continue;

			if ((i == 4) && (j == 4))	//player tile. Always enable the hidden layer there, but don't draw it over the player icon
			{
				// Move the visibility bit to the hidden position, and OR it with the value in the vector.
				// If the user ever sees the hidden info, it stays "seen"
				m_FogOfWarTiles[tilePosX + tilePosY * MAP_WIDTH] |= (1 << (UINT8)FogOfWarMarkers::Hidden);
				continue;
			}

			// Now look at the tiles around the player
			XMFLOAT2 tilePosInMap(
				fbBorderLeft + i * FBTW,
				fbBorderTop + j * FBTH
			);
			int mapPos = i + j * 9;	// the actual viewport map is 9x9 although we only look at the 8 tiles around the avatar
			{
				XMUINT2 _tileSheetPos(0, 0);
				RECT _tileSheetRect;
				bool _hasOverlay = false;

				// all tiles are animated. Start the strobe for each tile independently, so that it looks a bit better on the map
				_tileSheetPos.x = ((m_spriteAnimHiddenIdx / STROBESLOWHIDDEN) + mapPos) % HIDDENSPRITECT;
				switch (tilesVisibleAroundAvatar[mapPos])
				{
				case 0x2a:	// weapon chest
					_tileSheetPos.y = 1;
					if (PartyHasClass(DeathlordClasses::Thief))
						_hasOverlay = true;
					break;
				case 0x2b:	// armor chest
					_tileSheetPos.y = 2;
					if (PartyHasClass(DeathlordClasses::Thief))
						_hasOverlay = true;
					break;
				case 0xc6:	// water poison
					_tileSheetPos.y = 3;
					if (PartyHasClass(DeathlordClasses::Ranger, DeathlordClasses::Druid))
						_hasOverlay = true;
					if (PartyHasClass(DeathlordClasses::Barbarian))
						_hasOverlay = true;
					break;
				case 0x76:	// water bonus! "Z"-drink it and hope for the best!
					_tileSheetPos.y = 0;
					if (PartyHasClass(DeathlordClasses::Ranger, DeathlordClasses::Druid))
						_hasOverlay = true;
					if (PartyHasClass(DeathlordClasses::Barbarian))
						_hasOverlay = true;
					break;
				case 0x7e:	// pit
					_tileSheetPos.y = 4;
					if (PartyLeaderIsOfClass(DeathlordClasses::Thief, DeathlordClasses::Ranger))
						_hasOverlay = true;
					if (PartyLeaderIsOfRace(DeathlordRaces::DarkElf))
						_hasOverlay = true;
					break;
				case 0xce:	// chute / teleporter
					_tileSheetPos.y = 5;
					if (PartyHasClass(DeathlordClasses::Illusionist))
						_hasOverlay = true;
					break;
				case 0x02:	// illusiory wall
					_tileSheetPos.y = 7;
					if (PartyHasClass(DeathlordClasses::Illusionist))
						_hasOverlay = true;
					break;
				case 0x05:	// illusiory Rock
					_tileSheetPos.y = 7;
					if (PartyHasClass(DeathlordClasses::Illusionist))
						_hasOverlay = true;
					break;
				case 0x57:	// hidden door
					_tileSheetPos.y = 8;
					if (PartyHasClass(DeathlordClasses::Thief, DeathlordClasses::Ranger))
						_hasOverlay = true;
					if (PartyHasRace(DeathlordRaces::DarkElf))
						_hasOverlay = true;
					break;
				case 0x7F:	// interesting tombstone
					[[fallthrough]];
				case 0x50:	// interesting dark tile
					[[fallthrough]];
				case 0x85:	// interesting trees/bushes. No idea what this is
					_tileSheetPos.y = 0;
					if (PartyHasClass(DeathlordClasses::Thief, DeathlordClasses::Ranger))
						_hasOverlay = true;
					if (PartyHasRace(DeathlordRaces::DarkElf))
						_hasOverlay = true;
					break;
				case 0x37:	// jar unopened
					[[fallthrough]];
				case 0x39:	// coffin unopened	(could have a vampire or gold or nothing in it)
					[[fallthrough]];
				case 0x3b:	// chest unopened
					[[fallthrough]];
				case 0x3d:	// coffer unopened
					_tileSheetPos.y = 0;
					_hasOverlay = true;
					break;
				default:
					_hasOverlay = false;
					break;
				}

				if (_hasOverlay)	// draw the overlay if it exists
				{
					_tileSheetRect =
					{
						((UINT16)_tileSheetPos.x) * FBTW,
						((UINT16)_tileSheetPos.y) * FBTH,
						((UINT16)(_tileSheetPos.x + 1)) * FBTW,
						((UINT16)(_tileSheetPos.y + 1)) * FBTH
					};
					// draw it on the game's original viewport area
					spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapHiddenSpriteSheet), GetTextureSize(m_autoMapSpriteSheet.Get()),
						tilePosInMap, &_tileSheetRect, Colors::White, 0.f, spriteOrigin, mapScale);

					// Move the visibility bit to the hidden position, and OR it with the value in the vector.
					// If the user ever sees the hidden info, it stays "seen"
					m_FogOfWarTiles[tilePosX + tilePosY * MAP_WIDTH] |= (1 << (UINT8)FogOfWarMarkers::Hidden);
				}
			}
		}
	}
	spriteBatch->End();
}

#pragma warning(pop)

void AutoMap::CreateNewTileSpriteMap()
{
	// This is obsolete. It used to dynamically generate the original in-game tilemap

#if 0
	// The tile spritemap will automatically update from memory onto the GPU texture, just like the AppleWin video buffer
	// Sprite sheet only exists when the player is in-game! Make sure this is called when player goes in game
	// And make sure the map only renders when in game.
	auto tileset = TilesetCreator::GetInstance();
	TextureDescriptors _txd;
	_txd = (IsOverland() ? TextureDescriptors::AutoMapOverlandTiles : TextureDescriptors::AutoMapDungeonTiles);
	LoadTextureFromMemory((const unsigned char*)tileset->GetCurrentTilesetBuffer(),
		&m_autoMapTexture, DXGI_FORMAT_R8G8B8A8_UNORM, _txd, PNGBUFFERWIDTH, PNGBUFFERHEIGHT);
	//OutputDebugStringA("Loaded map into GPU\n");
#endif
}


void AutoMap::DrawAutoMap(std::shared_ptr<DirectX::SpriteBatch>& spriteBatch, DirectX::CommonStates* states, RECT* mapRect)
{
	BYTE* tilesVisibleAroundAvatar = MemGetMainPtr(GAMEMAP_START_CURRENT_TILELIST);

	// Scale the viewport, then translate it in reverse to how much
	// the center of the mapRect was translated when scaled
	// In this way the map is always centered when scaled, within the mapRect requested
	// Also the center of the mapRect is shifted to one of the quadrants if a zoomed-in map is asked for

	UINT currentBackBufferIdx = m_deviceResources->GetSwapChain()->GetCurrentBackBufferIndex();
	CopyRect(&m_currentMapRect, mapRect);
	auto tileset = TilesetCreator::GetInstance();
	auto gamePtr = GetGamePtr();
	auto commandList = m_deviceResources->GetCommandList();
	SimpleMath::Viewport mapViewport(m_deviceResources->GetScreenViewport());
	spriteBatch->SetViewport(mapViewport);

	if (bShowTransition)
	{
		// Whatever happens we must show the transition right now
		spriteBatch->Begin(commandList, states->LinearWrap(), DirectX::SpriteSortMode_Deferred);
		auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackgroundTransition), mmBGTexSize,
			*mapRect, nullptr, Colors::White, 0.f, XMFLOAT2());
		spriteBatch->End();
		return;
	}

	spriteBatch->Begin(commandList, states->LinearWrap(), DirectX::SpriteSortMode_Deferred);
	SimpleMath::Rectangle mapScissorRect = SimpleMath::Rectangle (*mapRect);
	float _scale = (g_nonVolatile.mapQuadrant == AutoMapQuadrant::All ? 1.f : 2.f);
	Vector2 _mapCenter = mapScissorRect.Center();

	// Determine if the leader character has a certain status
	// STV should fade the map to blander colors
	// TOX should tint it green
	// ILL should fuzzy it up
	UINT8 _mStatus = MemGetMainPtr(PARTY_STATUS_START)[MemGetMainPtr(PARTY_CURRENT_CHAR_POS)[0]];

	switch (g_nonVolatile.mapQuadrant)
	{
	case AutoMapQuadrant::FollowPlayer:
		// OPTION 1: Avatar is only centered until the map reaches the edge
		//			 Then the avatar moves towards the edge
		// Use it for overland only, so that the edges of maps aren't empty
		if (PlayerIsOverland())
		{
			if (m_avatarPosition.x < 16)
				_mapCenter.x = mapScissorRect.x + 16 * PNGTW - mapScissorRect.width / 2.f;
			else if (m_avatarPosition.x > 48)
				_mapCenter.x = mapScissorRect.x + 48 * PNGTW - mapScissorRect.width / 2.f;
			else
				_mapCenter.x = mapScissorRect.x + m_avatarPosition.x * PNGTW - mapScissorRect.width / 2.f;
			if (m_avatarPosition.y < 16)
				_mapCenter.y = mapScissorRect.y + 16 * PNGTH - mapScissorRect.height / 2.f;
			else if (m_avatarPosition.y > 48)
				_mapCenter.y = mapScissorRect.y + 48 * PNGTH - mapScissorRect.height / 2.f;
			else
				_mapCenter.y = mapScissorRect.y + m_avatarPosition.y * PNGTH - mapScissorRect.height / 2.f;
		}
		else
		{ 		
			// OPTION 2: Avatar is always centered
			// Use it for dungeons (there's always a wall at the edge)
			// and for towns (where we always draw a grass background beyond the edge)
			_mapCenter.x = mapScissorRect.x + m_avatarPosition.x * PNGTW - mapScissorRect.width / 2.f;
			_mapCenter.y = mapScissorRect.y + m_avatarPosition.y * PNGTH - mapScissorRect.height / 2.f;
		}
		break;
	case AutoMapQuadrant::TopLeft:
		_mapCenter.x -= mapScissorRect.width / 2.f;
		_mapCenter.y -= mapScissorRect.height / 2.f;
		break;
	case AutoMapQuadrant::TopRight:
		_mapCenter.x += mapScissorRect.width / 2.f;
		_mapCenter.y -= mapScissorRect.height / 2.f;
		break;
	case AutoMapQuadrant::BottomLeft:
		_mapCenter.x -= mapScissorRect.width / 2.f;
		_mapCenter.y += mapScissorRect.height / 2.f;
		break;
	case AutoMapQuadrant::BottomRight:
		_mapCenter.x += mapScissorRect.width / 2.f;
		_mapCenter.y += mapScissorRect.height / 2.f;
		break;
	}
	Vector2 _mapCenterScaled(_mapCenter * _scale);
	Vector2 _mapTranslation = _mapCenterScaled - _mapCenter;
	mapViewport.x -= _mapTranslation.x;
	mapViewport.y -= _mapTranslation.y;
	mapViewport.width *= _scale;
	mapViewport.height *= _scale;
	//(*gamePtr)->m_mapViewport = mapViewport;
	//(*gamePtr)->m_mapScissorRect = mapScissorRect;
	commandList->RSSetViewports(1, mapViewport.Get12());
	commandList->RSSetScissorRects(1, &(RECT)mapScissorRect);

	// Now draw the automap tiles
	if (g_isInGameMap)
	{
		// Set the new map info if necessary
		if (m_currentMapUniqueName != GetCurrentMapUniqueName())
			InitializeCurrentMapInfo();

		float mapScale = (float)MAP_WIDTH_IN_VIEWPORT / (float)(MAP_WIDTH * PNGTW);
		XMFLOAT2 avatarPosInMap = {-1,-1};	// don't show if negative pos

		// These will be set to the correct sprite sheet and rect for each tile
		TextureDescriptors curr_texDesc;
		Microsoft::WRL::ComPtr<ID3D12Resource> curr_tileset;
		RECT curr_spriteRect;

		XMUINT2 tileTexSize(PNGTW, PNGTH);
		// Loop through the in-memory map that has all the tile IDs for the current map
		LPBYTE mapMemPtr = GetCurrentGameMap();
		//OutputDebugStringA((std::to_string(currentBackBufferIdx)).append(std::string(" backbuffer being parsed\n")).c_str());
		for (UINT32 mapPos = 0; mapPos < MAP_LENGTH; mapPos++)
		{
			//OutputDebugStringA((std::to_string(mapPos)).append(std::string(" tile on screen\n")).c_str());

			// TODO: This is disabled as we redraw everything every frame
			// Only redraw the current backbuffer when a tile changes, as well as forbid ClearRenderTargetView
			// if (m_bbufCurrentMapTiles[currentBackBufferIdx][mapPos] == (UINT8)mapMemPtr[mapPos])    // it's been drawn
			//	continue;

			// Decide how much visibility this tile has
			float _tileVisibilityLevel = m_LOSVisibilityTiles[mapPos];
			if (_tileVisibilityLevel < 1)
			{
				if ((m_FogOfWarTiles[mapPos] & (1 << (UINT8)FogOfWarMarkers::UnFogOfWar)) > 0)
					_tileVisibilityLevel = .15f;
			}
			if (g_nonVolatile.removeFog == true)		// override if no fog
				_tileVisibilityLevel = 1.0f;


			// Now check if the tile has been seen before. Otherwise set it to the black tile which is always
			// the first tile in the tileset

			UINT8 curr_tileId = mapMemPtr[mapPos] % 0x50;	// Right now don't consider the special bits
			bool shouldDraw = true;
			if (_tileVisibilityLevel > 0)
			{
				// Here decide which tiles to show!
				if (curr_tileId < 0x40)	// environment tiles
				{
ENVIRONMENT:
					if (PlayerIsOverland())
					{
						switch (curr_tileId)
						{
						case 0x2B:		// water tile
							curr_spriteRect.top = TILESET_ANIMATIONS_WATER_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
						case 0x3C:		// fire tile
							curr_spriteRect.top = TILESET_ANIMATIONS_FIRE_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
						default:
							curr_texDesc = TextureDescriptors::AutoMapOverlandTiles;
							curr_tileset = m_tilesOverland;
							curr_spriteRect.left = PNGTW * (curr_tileId % DEFAULT_TILES_PER_ROW);
							curr_spriteRect.top = PNGTH * (curr_tileId / DEFAULT_TILES_PER_ROW);
							break;
						};
					}
					else // Dungeon tileset
					{
						switch (curr_tileId)
						{
						case 0x26:		// water tile
							curr_spriteRect.top = TILESET_ANIMATIONS_WATER_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
							break;
						case 0x27:		// forcefield tile
							curr_spriteRect.top = TILESET_ANIMATIONS_FORCE_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
							break;
						case 0x2C:		// acid tile
							curr_spriteRect.top = TILESET_ANIMATIONS_ACID_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
							break;
						case 0x2D:		// fire tile
							curr_spriteRect.top = TILESET_ANIMATIONS_FIRE_Y_IDX * PNGTH;
							goto ELEMENT_TILES_GENERAL;
						case 0x38:		// magic tile
							curr_spriteRect.top = TILESET_ANIMATIONS_MAGIC_Y_IDX * PNGTH;
ELEMENT_TILES_GENERAL:
							curr_texDesc = TextureDescriptors::AutoMapElementsTiles;
							curr_tileset = m_tilesAnimated;
							curr_spriteRect.left = ((m_spriteAnimElementIdx / STROBESLOWELEMENT) % ELEMENTSPRITECT) * PNGTW;
							break;
						default:
							curr_texDesc = TextureDescriptors::AutoMapDungeonTiles;
							curr_tileset = m_tilesDungeon;
							curr_spriteRect.left = PNGTW * (curr_tileId % DEFAULT_TILES_PER_ROW);
							curr_spriteRect.top = PNGTH * (curr_tileId / DEFAULT_TILES_PER_ROW);
							break;
						};
					}
				} // environment tiles
				else // monster tiles
				{
					// Do we want to draw the monster? Only if we have full visibility!
					// Otherwise swap back the original tile
					if (_tileVisibilityLevel < 1)
					{
						curr_tileId = StaticTileIdAtMapPosition(mapPos % MAP_WIDTH, mapPos / MAP_WIDTH);
						goto ENVIRONMENT;
					}
					// Below is to draw the monster on the tile
					curr_texDesc = TextureDescriptors::AutoMapMonsterSpriteSheet;
					curr_tileset = m_monsterSpriteSheet;
					// find the monster id for this tile id
					UINT8 _monsterHGRTilePos = curr_tileId - 0x40;	// Monster's tile position relative to other monsters
					UINT8 _monsterId = MemGetMainPtr(GAMEMAP_START_MONSTERS_IN_LEVEL_IDX)[_monsterHGRTilePos];
					curr_spriteRect.left = PNGTW * (_monsterId % DEFAULT_TILES_PER_ROW);
					curr_spriteRect.top = PNGTH * (_monsterId / DEFAULT_TILES_PER_ROW);
				}
				curr_spriteRect.right = curr_spriteRect.left + PNGTW;
				curr_spriteRect.bottom = curr_spriteRect.top + PNGTH;
			}
			else // Tile we should not see
			{
				// TODO: Since we're clearing the whole viewport every frame, no need to draw the black tile
				curr_texDesc = TextureDescriptors::AutoMapOverlandTiles;
				curr_tileset = m_tilesOverland;
				curr_spriteRect = tileset->tileSpritePositions.at(0);
				shouldDraw = false;
			}

			XMFLOAT2 spriteOrigin(0, 0);
			UINT32 posX = mapPos % MAP_WIDTH;
			UINT32 posY = mapPos / MAP_WIDTH;
			XMFLOAT2 tilePosInMap(
				m_currentMapRect.left + (posX * PNGTW * mapScale),
				m_currentMapRect.top + (posY * PNGTH * mapScale)
			);
			if (shouldDraw)
			{
				RECT tilePosRectInMap = { tilePosInMap.x, tilePosInMap.y, 
					tilePosInMap.x + PNGTW * mapScale, tilePosInMap.y + PNGTH * mapScale };

				XMVECTORF32 _tileColor = XMVECTORF32({ { { 1.f, 1.f, 1.f, _tileVisibilityLevel } } });
				// set the statuses special effects
				auto _tt = (*gamePtr)->GetTotalTicks();
				auto _tt_fast = _tt >> 4;
				auto _tt_slow = _tt >> 16;

				if (_mStatus & (UINT8)DeathlordCharStatus::STV)
				{
					float _ttd = ((float)(_tt_slow % 5) - 2.f) / 100.f;	// delta for animation
					_tileColor.f[0] *= .7f + _ttd;
					_tileColor.f[1] *= .7f + _ttd;
					_tileColor.f[2] *= .7f + _ttd;
					_tileColor.f[3] *= .7f + _ttd;
				}
				if (_mStatus & (UINT8)DeathlordCharStatus::TOX)
				{
					float _ttd = ((float)(_tt_slow % 5) - 2.f) / 100.f;	// delta for animation
					_tileColor.f[0] *= .2f + _ttd;
					_tileColor.f[1] *= .8f + _ttd;
					_tileColor.f[2] *= .2f + _ttd;
				}

				spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)curr_texDesc),
					GetTextureSize(curr_tileset.Get()), tilePosRectInMap, &curr_spriteRect, _tileColor);

				// Show a marker for traps, hidden and unopened items
				// Only when not on the overland map

				// Reset the curr_tileID to have the special bits
				curr_tileId = mapMemPtr[mapPos];

				// First check if the player has already identified the hidden marker
				bool hasSeenHidden = false;
				if ((m_FogOfWarTiles[mapPos] & (1 << (UINT8)FogOfWarMarkers::Hidden)) > 0)
				{
					hasSeenHidden = true;
				}
				
				// The below shows the hidden items in the non-overland map,
				// either when the player has encountered them, or when the player has enabled this cheat
				if ((g_nonVolatile.showHidden || hasSeenHidden)
					&& !PlayerIsOverland())
				{
					XMUINT2 _tileSheetPos(0, 0);
					RECT _tileSheetRect;
					bool _hasOverlay = true;	// some tiles will not have overlays (in the default: section)

					// all tiles are animated. Start the strobe for each tile independently, so that it looks a bit better on the map
					_tileSheetPos.x = ((m_spriteAnimHiddenIdx / STROBESLOWHIDDEN) + mapPos) % HIDDENSPRITECT;
					switch (curr_tileId)
					{
					case 0x2A:	// weapon chest
						_tileSheetPos.y = 1;
						break;
					case 0x2B:	// armor chest
						_tileSheetPos.y = 2;
						break;
					case 0xC6:	// water poison
						_tileSheetPos.y = 3;
						break;
					case 0x7E:	// pit
						_tileSheetPos.y = 4;
						break;
					case 0xCE:	// chute / teleporter
						_tileSheetPos.y = 5;
						break;
					case 0x02:	// illusiory wall
						_tileSheetPos.y = 7;
						break;
					case 0x05:	// illusiory Rock
						_tileSheetPos.y = 7;
						break;
					case 0x57:	// hidden door
						_tileSheetPos.y = 8;
						break;
					case 0x7F:	// interesting tombstone
						[[fallthrough]];
					case 0x50:	// interesting dark tile
						[[fallthrough]];
					case 0x85:	// interesting trees/bushes. No idea what this is
						[[fallthrough]];
					case 0x76:	// water bonus! "Z"-drink it and hope for the best!
						[[fallthrough]];
					case 0x37:	// jar unopened
						[[fallthrough]];
					case 0x39:	// coffin unopened	(could have a vampire or gold or nothing in it)
						[[fallthrough]];
					case 0x3B:	// chest unopened
						[[fallthrough]];
					case 0x3D:	// coffer unopened
						_tileSheetPos.y = 0;
						break;
					default:
						_hasOverlay = false;
						break;
					}
					if (_hasOverlay)	// draw the overlay if it exists
					{
						_tileSheetRect =
						{
							((UINT16)_tileSheetPos.x) * FBTW,
							((UINT16)_tileSheetPos.y) * FBTH,
							((UINT16)(_tileSheetPos.x + 1)) * FBTW,
							((UINT16)(_tileSheetPos.y + 1)) * FBTH
						};
						spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapHiddenSpriteSheet), GetTextureSize(m_autoMapSpriteSheet.Get()),
							tilePosInMap, &_tileSheetRect, Colors::White, 0.f, spriteOrigin, mapScale);
					}

#ifdef _DEBUGXXX
					char _tileHexVal[5];
					sprintf_s(_tileHexVal, 4, "%02x", mapMemPtr[mapPos]);
					// Display the tile ID to see the difference between regular and hidden walls, chutes, etc...
					(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), _tileHexVal,
						tilePosInMap + XMFLOAT2(.5f, .5f), Colors::Black, 0.f, spriteOrigin, .5f);
					(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), _tileHexVal,
						tilePosInMap, Colors::Cyan, 0.f, spriteOrigin, .5f);
#endif
				}
			}
			m_bbufCurrentMapTiles[currentBackBufferIdx][mapPos] = (UINT8)mapMemPtr[mapPos];

			// now draw the avatar and footsteps
			// Use the correct monster tile for the avatar, based on the class of the current char
			XMFLOAT2 _origin = { PNGTW / 2.f, PNGTH / 2.f };
			RECT avatarRect = { 0, 0, PNGTW, PNGTH };
			avatarRect.left = MemGetMainPtr(PARTY_CURRENT_CHAR_CLASS)[0] * PNGTW;
			avatarRect.right = avatarRect.left + PNGTW;

			XMFLOAT2 overlayPosInMap(	// Display the avatar in the center of the tile
				tilePosInMap.x + mapScale * PNGTW / 2,
				tilePosInMap.y + mapScale * PNGTW / 2
			);
			XMFLOAT2 footstepsPosInMap(	// Footsteps as well, but we need a bit of realignment
				overlayPosInMap.x - 1,
				overlayPosInMap.y + 2
			);

			if (posX == m_avatarPosition.x && posY == m_avatarPosition.y)
			{
				switch ((PartyIconType)(MemGetMainPtr(PARTY_ICON_TYPE)[0]))
				{
				case PartyIconType::Regular:
				{	
					// Only draw if not hidden
					if (MemGetMainPtr(MAP_IS_HIDDEN)[0] == 0)
					{
						spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapMonsterSpriteSheet),
							GetTextureSize(m_monsterSpriteSheet.Get()), overlayPosInMap, &avatarRect,
							Colors::White, 0.f, _origin,
							mapScale);
						avatarPosInMap = overlayPosInMap;
					}
					break;
				}
				case PartyIconType::Boat:
				{
					// We're on a boat! Boat is tile 0x12 of overland sheet
					RECT boatRect = { 2 * PNGTW, 1 * PNGTH, 3 * PNGTW, 2 * PNGTH };
					spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapOverlandTiles),
						GetTextureSize(m_tilesOverland.Get()), tilePosInMap, &boatRect,
						Colors::White, 0.f, XMFLOAT2(), mapScale);
					break;
				}
				case PartyIconType::Pit:
					break;
				default:
					break;
				}
				++m_avatarStrobeIdx;
				if (m_avatarStrobeIdx >= (AVATARSTROBECT * STROBESLOWAVATAR))
					m_avatarStrobeIdx = 0;
			}
			else // don't draw footsteps on the current tile but on anything else that has footsteps
			{
				if (g_nonVolatile.showFootsteps)
				{
					if ((m_FogOfWarTiles[mapPos] & (1 << (UINT8)FogOfWarMarkers::Footstep)) > 0)
					{
						(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontDLRegular)->DrawString(spriteBatch.get(), m_cursor.c_str(),
							footstepsPosInMap, Colors::Yellow, 0.f, XMFLOAT2(4.f, 9.f), .4f);
					}
				}
			}
		}

		// Draw the cursor on the avatar
		if (avatarPosInMap.x >= 0)
		{
			spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapHiddenSpriteSheet),
				GetTextureSize(m_autoMapSpriteSheet.Get()), avatarPosInMap, &RECT_SPRITE_CURSOR,
				Colors::White, 0.f, { (RECT_SPRITE_CURSOR.right - RECT_SPRITE_CURSOR.left) / 2.f, (RECT_SPRITE_CURSOR.bottom - RECT_SPRITE_CURSOR.top) / 2.f },
				mapScale * AVATARSTROBE[(m_avatarStrobeIdx / STROBESLOWAVATAR) % AVATARSTROBECT]);
		}

		++m_spriteAnimElementIdx;
		if (m_spriteAnimElementIdx >= (ELEMENTSPRITECT * STROBESLOWELEMENT))
			m_spriteAnimElementIdx = 0;
		++m_spriteAnimHiddenIdx;
		if (m_spriteAnimHiddenIdx >= (HIDDENSPRITECT * STROBESLOWHIDDEN))
			m_spriteAnimHiddenIdx = 0;
	}
	else // draw the background if not in game
	{	// this shouldn't ever happen since when not in game it's using the original game's video
		auto mmBGTexSize = GetTextureSize(m_autoMapTextureBG.Get());
		// nullptr here is the source rectangle. We're drawing the full background
		spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle((int)TextureDescriptors::AutoMapBackgroundTransition), mmBGTexSize,
			m_currentMapRect, nullptr, Colors::White, 0.f, XMFLOAT2());
		// write text on top of automap area
		Vector2 awaitTextPos(
			m_currentMapRect.left + (mapRect->right - mapRect->left) / 2 - 240.f,
			m_currentMapRect.top + (mapRect->bottom - mapRect->top) / 2 - 20.f);
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "THE LANDS OF LORN AWAIT",
			awaitTextPos - Vector2(3.f, -2.f), Colors::White, 0.f, Vector2(0.f, 0.f), 3.f);
		(*gamePtr)->GetSpriteFontAtIndex(FontDescriptors::FontA2Regular)->DrawString(spriteBatch.get(), "THE LANDS OF LORN AWAIT",
			awaitTextPos, COLOR_APPLE2_VIOLET, 0.f, Vector2(0.f, 0.f), 3.f);
	}

	spriteBatch->End();
	D3D12_VIEWPORT viewports[2] = { m_deviceResources->GetScreenViewport() };
	D3D12_RECT scissorRects[1] = { m_deviceResources->GetScissorRect() };
	commandList->RSSetViewports(1, viewports);
	commandList->RSSetScissorRects(1, scissorRects);
	// End drawing automap

}


# pragma region D3D stuff
void AutoMap::CreateDeviceDependentResources(ResourceUploadBatch* resourceUpload)
{
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Background_NoMap.png",
			m_autoMapTextureBG.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapTextureBG.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapBackgroundTransition));
	// Overlay sprites
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/SpriteSheet.png",
			m_autoMapSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_autoMapSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapHiddenSpriteSheet));
	// Add overland, dungeon and monster tilesets
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Tileset_Relorded_Dungeon.png",
			m_tilesDungeon.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_tilesDungeon.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapDungeonTiles));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Tileset_Relorded_Overland.png",
			m_tilesOverland.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_tilesOverland.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapOverlandTiles));
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Tileset_Relorded_Monsters.png",
			m_monsterSpriteSheet.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_monsterSpriteSheet.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapMonsterSpriteSheet));
	// Add animated elements tilesets
	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, *resourceUpload, L"Assets/Tileset_Elements_Animated.png",
			m_tilesAnimated.ReleaseAndGetAddressOf()));
	CreateShaderResourceView(device, m_tilesAnimated.Get(),
		m_resourceDescriptors->GetCpuHandle((int)TextureDescriptors::AutoMapElementsTiles));
}

void AutoMap::OnDeviceLost()
{
	m_autoMapTextureBG.Reset();
	m_tilesOverland.Reset();
	m_tilesDungeon.Reset();
	m_tilesAnimated.Reset();
	m_monsterSpriteSheet.Reset();
	m_autoMapSpriteSheet.Reset();
}
#pragma endregion

#pragma region DX Helpers

// Simple helper function to load an image as a R8B8G8 memory buffer into a DX12 texture with common settings
// Returns true on success, with the SRV CPU handle having an SRV for the newly-created texture placed in it
// (srv_cpu_handle must be a handle in a valid descriptor heap)
bool AutoMap::LoadTextureFromMemory(const unsigned char* image_data,
	Microsoft::WRL::ComPtr <ID3D12Resource>* out_tex_resource,
	DXGI_FORMAT tex_format, TextureDescriptors tex_desc, int width, int height)
{
	int image_width = width;
	int image_height = height;
	if (image_data == NULL)
		return false;

	auto device = m_deviceResources->GetD3DDevice();

	// Create texture resource
	D3D12_HEAP_PROPERTIES props;
	memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
	props.Type = D3D12_HEAP_TYPE_DEFAULT;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = image_width;
	desc.Height = image_height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = tex_format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* pTexture = NULL;
	DX::ThrowIfFailed(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture)));

	if (pTexture == NULL)
		return false;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexture, 0, 1);

	props.Type = D3D12_HEAP_TYPE_UPLOAD;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// Create the GPU upload buffer.
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	DX::ThrowIfFailed(
		device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_textureUploadHeapMap.GetAddressOf())));

	m_textureDataMap.pData = image_data;
	m_textureDataMap.SlicePitch = static_cast<UINT64>(width) * height * sizeof(uint32_t);
	m_textureDataMap.RowPitch = static_cast<LONG_PTR>(desc.Width * sizeof(uint32_t));

	auto commandList = m_deviceResources->GetCommandList();
	commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);


	UpdateSubresources(commandList, pTexture, m_textureUploadHeapMap.Get(), 0, 0, 1, &m_textureDataMap);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	// Describe and create a SRV for the texture.
	// Using CreateShaderResourceView removes a ton of boilerplate
	CreateShaderResourceView(device, pTexture,
		m_resourceDescriptors->GetCpuHandle((int)tex_desc));

	// finish up
	DX::ThrowIfFailed(commandList->Close());
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));
	// Wait until assets have been uploaded to the GPU.
	m_deviceResources->WaitForGpu();

	// Return results
	out_tex_resource->Attach(pTexture);

	return true;
}

#pragma endregion