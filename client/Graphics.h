#pragma once


#include "UIFramework/Fonts.h"
#include "../lib/GameConstants.h"
#include "UIFramework/Geometries.h"

/*
 * Graphics.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

class CDefEssential;
struct SDL_Surface;
class CGHeroInstance;
class CGTownInstance;
class CDefHandler;
class CHeroClass;
struct SDL_Color;
struct InfoAboutHero;
struct InfoAboutTown;
class CGObjectInstance;
class CGDefInfo;

enum EFonts
{
	FONT_BIG, FONT_CALLI, FONT_CREDITS, FONT_HIGH_SCORE, FONT_MEDIUM, FONT_SMALL, FONT_TIMES, FONT_TINY, FONT_VERD
};

/// Handles fonts, hero images, town images, various graphics
class Graphics
{
public:
	//Fonts
	static const int FONTS_NUMBER = 9;
	IFont * fonts[FONTS_NUMBER];
	\
	//various graphics
	SDL_Color * playerColors; //array [8]
	SDL_Color * neutralColor;
	SDL_Color * playerColorPalette; //palette to make interface colors good - array of size [256]
	SDL_Color * neutralColorPalette; 

	std::vector<CDefEssential *> flags1, flags2, flags3, flags4; //flags blitted on heroes when ,
	CDefEssential * resources32; //resources 32x32
	CDefEssential * heroMoveArrows;
	std::map<std::string, CDefEssential *> heroAnims; // [hero class def name]  //added group 10: up - left, 11 - left and 12 - left down // 13 - up-left standing; 14 - left standing; 15 - left down standing
	std::vector<CDefEssential *> boatAnims; // [boat type: 0 - 3]  //added group 10: up - left, 11 - left and 12 - left down // 13 - up-left standing; 14 - left standing; 15 - left down standing
	CDefHandler * FoWfullHide; //for Fog of War
	CDefHandler * FoWpartialHide; //for For of War

	std::map<std::string, CDefEssential *> advmapobjGraphics;
	CDefEssential * getDef(const CGObjectInstance * obj);
	CDefEssential * getDef(const CGDefInfo * info);
	//towns
	std::map<int, std::string> ERMUtoPicture[GameConstants::F_NUMBER]; //maps building ID to it's picture's name for each town type
	//for battles
	std::vector< std::vector< std::string > > battleBacks; //battleBacks[terType] - vector of possible names for certain terrain type
	std::map< int, std::vector < std::string > > battleACToDef; //maps AC format to vector of appropriate def names
	CDefEssential * spellEffectsPics; //bitmaps representing spells affecting a stack in battle
	//spells
	CDefEssential *spellscr; //spell on the scroll 83x61
	//functions
	Graphics();	
	void initializeBattleGraphics();
	void loadPaletteAndColors();
	void loadHeroFlags();
	void loadHeroFlags(std::pair<std::vector<CDefEssential *> Graphics::*, std::vector<const char *> > &pr, bool mode);
	void loadHeroAnims();
	CDefEssential *  loadHeroAnim(const std::string &name, const std::vector<std::pair<int,int> > &rotations);
	void loadErmuToPicture();
	void blueToPlayersAdv(SDL_Surface * sur, PlayerColor player); //replaces blue interface colour with a color of player
	void loadFonts();
};

extern Graphics * graphics;
