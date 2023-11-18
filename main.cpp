// Alternative ray caster
// ======================
// Using occlusion list and quad rendering this time
//
// Joseph21, november 18, 2023
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)
//   *  ManipulatedSprite.h and .cpp (see: https://github.com/Joseph21-6147/Sprite-manipulation-warping-rotating-scaling)
//   *  my_utility.h and .cpp (which is made available in the same github site as this program)

/* Short description
   -----------------
   Instead of evaluating all the screen columns one by one, this ray caster uses a more doom like approach:
   1. build a list of all the (non-empty) cells that are in the players field of view
   2. build a list of possibly visible faces of these cells (that are directed towards the player)
   3. sort this list in proximity, close by first
   4. use an occlusion list to determine if the screen is fully rendered, and which quads must be rendered and clipped at which locations
   5. render the walls one quad at a time

   To do
   -----

    Have fun!
 */

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include "my_utility.h"
#include "ManipulatedSprite.h"


#define PI 3.1415926535f

// Screen and pixel constants - keep the screen sizes constant and vary the resolution by adapting the pixel size
// to prevent accidentally defining too large a window
#define SCREEN_X    1400
#define SCREEN_Y     800
#define PIXEL_X        1
#define PIXEL_Y        1

// colour constants
#define COL_CEIL_FRNT    olc::BLUE
#define COL_CEIL_BACK    olc::WHITE
#define COL_FLOOR_FRNT   olc::VERY_DARK_RED
#define COL_FLOOR_BACK   olc::Pixel( 16, 0, 0 )
#define COL_WALL         olc::GREY
#define COL_TEXT         olc::YELLOW
#define COL_BG           olc::Pixel( 10, 10, 10 )   // near black

// constants for speed movements
#define SPEED_ROTATE      60.0f   //                          60 degrees per second
#define SPEED_MOVE         5.0f   // forward and backward -    5 units (tiles) per second
#define SPEED_STRAFE       5.0f   // left and right strafing - 5 units (tiles) per second

class AlternativeRayCaster : public olc::PixelGameEngine {

public:
    AlternativeRayCaster() {    // display the screen and pixel dimensions in the window caption
        sAppName = "Quad rendered RayCaster ";
        sAppName.append( "- S:(" + std::to_string( SCREEN_X / PIXEL_X ) + ", " + std::to_string( SCREEN_Y / PIXEL_Y ) + ")" );
        sAppName.append( ", P:(" + std::to_string(            PIXEL_X ) + ", " + std::to_string(            PIXEL_Y ) + ")" );
    }

private:
    // definition of the map
    std::string sMap;     // contains char's that define the type of block per map location
    int nMapX = 16;
    int nMapY = 16;

    // player: position and looking angle
    float fPlayerX     = 2.0f;
    float fPlayerY     = 2.0f;
    float fPlayerA_deg = 0.0f;    // looking angle is in degrees
    // player: height of eye point and field of view
    float fPlayerH       = 0.5f;
    float fPlayerFoV_deg = 60.0f;   // in degrees !!

    // for interior working
    float fPlayerA_rad = 0.0f;    // is kept synchronized with changes in fPlayerA_deg
    float fPlayerSin   = 0.0f;
    float fPlayerCos   = 0.0f;

    // distance to projection plane - needed for depth projection
    float fDistToProjPlane;

    bool bTestMode      = false;    // generic test mode toggle
    bool bHorRasterMode = false;    // rasters on screen - horizontal resp. vertical
    bool bVerRasterMode = false;
    bool bMapMode       = true;     // toggle for rendering minimap
    bool bInfoMode      = true;     // toggle for rendering info on screen

    float fMapScale     = 1.0f;     // 1.0f corresponds to 16x16 pixels per tile
    int nFacesRendered  = 0;        // counts nr of faces rendered per frame

    enum TextureMode {
        MONO = 0,
        SPRITE,
        DECAL
    };

    int  nTextureMode   = 0;        // mode for monochrome, sprite textured or decal textured quad rendering
    bool bWireFrameMode = true;     // toggle for rendering wireframes in monochrome mode

    float fRenderMaxDist = 20.0f;   // for shading - at this distance things completely dark

    olc::Sprite *brickTexture  = nullptr;
    olc::Sprite *brickTextureB = nullptr;    // duplicate with borders visualized

    olc::Sprite *pSpriteBG = nullptr;
    olc::Decal  *pDecalBG  = nullptr;

    olc::Sprite *pSpriteWalls[4] = { nullptr };
    olc::Decal  *pDecalWalls[4]  = { nullptr };
    olc::Sprite *pSpriteWallsB[4] = { nullptr };    // duplicate with borders visualized
    olc::Decal  *pDecalWallsB[4]  = { nullptr };

    int nLayerHUD;
    int nLayerScene;
    int nLayerBG;

    // column descriptor - a column is a strictly vertical line that is either the left or the right side of a face
    typedef struct sColDescriptor {
        int nScreenX              = 0;       // projection of face column onto screen column
        float fAngleFromPlayer    = 0.0f;    // angle
        float fDistFromPlayer     = 0.0f;    // distance - corrected for fish eye effect
        float fDistFromPlayer_raw = 0.0f;    // distance - not corrected
    } ColInfo;

    // types of faces
    enum FaceType {
        UNKNWN = -1,
        EAST = 0,
        SOUTH,
        WEST,
        NORTH
    };

    // face descriptor - a face is one of the four sides of a non-empty cell/tile
    // each face has a left and right column, as viewed from the outside of the face
    typedef struct sFaceDescriptor {
        olc::vi2d TileID;          // coords of the tile this face belongs to
        int nSide = UNKNWN;        // one of EAST, SOUTH, WEST, NORTH
        bool bVisible = false;     // for culling

        ColInfo leftCol, rghtCol;  // info on the columns for this face
    } FaceInfo;

    // tile descriptor - a tile has coordinates in the map
    typedef struct sTileDescriptor {
        olc::vi2d TileID;          // coords of the tile
    } TileInfo;

    // class variable lists containing tiles within VoF resp. faces directed towards player from that tiles
    std::vector<TileInfo> vTilesToRender;
    std::vector<FaceInfo> vFacesToRender;

    // test output functions for ColInfo, FaceInfo, TileInfo and Tile and Face lists
    // =============================================================================

    void PrintColInfo( ColInfo &c ) {
        std::cout << "col: "   << right_align( c.nScreenX            , 4    ) << ", ";
        std::cout << "angle: " <<   dot_align( c.fAngleFromPlayer    , 2, 5 ) << ", ";
        std::cout << "dist: "  <<   dot_align( c.fDistFromPlayer     , 2, 5 ) << ", ";
        std::cout << "raw: "   <<   dot_align( c.fDistFromPlayer_raw , 2, 5 ) << ", ";
    }

    std::string Face2String( int nFace ) {
        switch (nFace) {
            case UNKNWN: return "UNKNW";
            case EAST  : return "EAST ";
            case SOUTH : return "SOUTH";
            case WEST  : return "WEST ";
            case NORTH : return "NORTH";
        }
        return "ERROR";
    }

    std::string Coord2String( olc::vi2d c ) {
        return "(" + right_align( c.x, 3 ) + ", " + right_align( c.y, 3 ) + ")";
    }

    void PrintFace( FaceInfo &f ) {
        std::cout << "face side: "  << Face2String(  f.nSide  ) << ", ";
        std::cout << "tile coord: " << Coord2String( f.TileID ) << ", ";
        std::cout << (f.bVisible ? "IS  " : "NOT ") << "visible, ";

        std::cout << " LEFT column = " ; PrintColInfo( f.leftCol );
        std::cout << " RIGHT column = "; PrintColInfo( f.rghtCol );
    }

    void PrintTile( TileInfo &t ) {
        std::cout << "tile coord: " << Coord2String( t.TileID );
    }

    void PrintTilesList( std::vector<TileInfo> &vVisibleTiles ) {
        for (int i = 0; i < (int)vVisibleTiles.size(); i++) {
            TileInfo &curTile = vVisibleTiles[i];
            std::cout << "Index: " << right_align( i, 3 ) << " - ";
            PrintTile( curTile );
            std::cout << std::endl;
        }
    }

    void PrintFacesList( std::vector<FaceInfo> &vVisibleFaces ) {
        for (int i = 0; i < (int)vVisibleFaces.size(); i++) {
            FaceInfo &curFace = vVisibleFaces[i];
            std::cout << "Index: " << right_align( i, 3 ) << " - ";
            PrintFace( curFace );
            std::cout << std::endl;
        }
    }

public:
    bool OnUserCreate() override {

        // tile layout of the map - must be of size nMapX x nMapY

        //            0         1
        //            0123456789012345
        sMap.append( "################" );
        sMap.append( "#..............#" );
        sMap.append( "#........####..#" );
        sMap.append( "#..............#" );
        sMap.append( "#...#.....#....#" );
        sMap.append( "#...#..........#" );
        sMap.append( "#...####.......#" );
        sMap.append( "#..............#" );
        sMap.append( "#..............#" );
        sMap.append( "#..............#" );
        sMap.append( "#......##.##...#" );
        sMap.append( "#......#...#...#" );
        sMap.append( "#......#...#...#" );
        sMap.append( "#.......###....#" );
        sMap.append( "#..............#" );
        sMap.append( "################" );

        // sprite used for texturing walls
//        brickTexture = new olc::Sprite( "Bricks_06-128x128.png" );
        brickTexture = new olc::Sprite( "new wall.png" );

        int nSpriteSize = brickTexture->height;   // assumption !! sprite is square
        brickTextureB = brickTexture->Duplicate();

        for (int b = 0; b < nSpriteSize; b++) {
            brickTextureB->SetPixel(               b,               0, olc::WHITE );
            brickTextureB->SetPixel(               b, nSpriteSize - 1, olc::BLUE  );
            brickTextureB->SetPixel(               0,               b, olc::RED   );
            brickTextureB->SetPixel( nSpriteSize - 1,               b, olc::GREEN );
        }

        // work out distance to projection plane. This is a constant depending on the width of the projection plane and the field of view.
        fDistToProjPlane = ((ScreenWidth() / 2.0f) / sin( Deg2Rad( fPlayerFoV_deg / 2.0f ))) * cos( Deg2Rad( fPlayerFoV_deg / 2.0f ));

        fPlayerA_rad = Deg2Rad( fPlayerA_deg );
        fPlayerSin   = sin(     fPlayerA_rad );
        fPlayerCos   = cos(     fPlayerA_rad );

        // creating layering structure and filling background layer

        nLayerHUD = 0;    // default screen layer is for HUD and stuff
        nLayerScene = CreateLayer();
        nLayerBG = CreateLayer();
        EnableLayer( nLayerScene, true );
        EnableLayer( nLayerBG, true );

        auto fill_gradient_rect = [=]( int x1, int y1, int x2, int y2, bool bDownWards, olc::Pixel col1, olc::Pixel col2 = olc::BLACK ) {
            int y_strt = bDownWards ? y1 : y2;
            int y_stop = bDownWards ? y2 : y1;
            int y_step = bDownWards ? +1 : -1;
            for (int y = y_strt; y != y_stop; y += y_step) {
                float t = float( y - y_strt ) / float( y_stop - y_strt );
                olc::Pixel renderCol = PixelLerp( col1, col2, t );
                DrawLine( x1, y, x2, y, renderCol );
            }
        };

        // prepare scene layer to be transparent
        SetDrawTarget( nLayerScene );
        Clear( olc::BLANK );

        // prepare a decal as background for rendering in the background layer
        SetDrawTarget( nLayerBG );
        olc::Sprite *aux = GetDrawTarget();
        pSpriteBG = aux->Duplicate();
        SetDrawTarget( pSpriteBG );
        // draw synthetic sky and floor in the background layer
        int nHorizon = ScreenHeight() / 2;
        fill_gradient_rect( 0,            0, ScreenWidth() - 1,       nHorizon, true , COL_CEIL_FRNT , COL_CEIL_BACK  );
        fill_gradient_rect( 0, nHorizon + 1, ScreenWidth() - 1, ScreenHeight(), false, COL_FLOOR_FRNT, COL_FLOOR_BACK );
        // get a copy of that layer and build a decal from it
        pDecalBG = new olc::Decal( pSpriteBG );

        // create the grey sprites for the walls
        // NOTE: could be done with tinting as well
//        int nGreyValue[4] = { 200, 160, 80, 120 };
//        olc::Pixel edgeCol = olc::DARK_BLUE;
        for (int f = EAST; f <= NORTH; f++) {

//            olc::Sprite *pAux = new olc::Sprite( nSpriteSize, nSpriteSize );

            olc::Sprite *pAux  = brickTexture->Duplicate();
            olc::Sprite *pAuxB = brickTextureB->Duplicate();

//            SetDrawTarget( pAux );
//            Clear( olc::Pixel( nGreyValue[f], nGreyValue[f], nGreyValue[f] ));
//            for (int b = 0; b < nSpriteSize; b++) {
//                pAux->SetPixel(               b,               0, edgeCol );
//                pAux->SetPixel(               b, nSpriteSize - 1, edgeCol );
//                pAux->SetPixel(               0,               b, edgeCol );
//                pAux->SetPixel( nSpriteSize - 1,               b, edgeCol );
//            }
            pSpriteWalls[f] = pAux;
            pSpriteWallsB[f] = pAuxB;
            // create a decal from it
            pDecalWalls[f]  = new olc::Decal( pSpriteWalls[f]  );
            pDecalWallsB[f] = new olc::Decal( pSpriteWallsB[f] );
        }

        // reset draw target
        SetDrawTarget( nLayerHUD );

        return true;
    }

    // Generic convenience functions
    // =============================

    // angle conversion
    float Deg2Rad( float fDegAngle ) { return fDegAngle / 180.0f * PI; }
    float Rad2Deg( float fRadAngle ) { return fRadAngle * 180.0f / PI; }

    // returns true if f_low <= f <= f_hgh - also int overloaded variant
    bool InBetween( float f, float f_low, float f_hgh ) { return (f_low <= f && f <= f_hgh); }
    bool InBetween( int   n, int   n_low, int   n_hgh ) { return (n_low <= n && n <= n_hgh); }

    // converts degree angle to equivalent in range [0, 360)
    float Mod360_deg( float fDegAngle ) {
        if (fDegAngle <    0.0f) fDegAngle += 360.0f;
        if (fDegAngle >= 360.0f) fDegAngle -= 360.0f;

        if (!InBetween( fDegAngle, 0.0f, 360.0f )) {
            std::cout << "WARNING: Mod360_deg() --> angle not in range [0, 360) after operation: " << fDegAngle << std::endl;
        }
        return fDegAngle;
    }

    // converts radian angle to equivalent in range [0, 2 PI)
    float Mod2Pi_rad( float fRadAngle ) {
        if (fRadAngle <  0.0f     ) fRadAngle += 2.0f * PI;
        if (fRadAngle >= 2.0f * PI) fRadAngle -= 2.0f * PI;

        if (!InBetween( fRadAngle, 0.0f, 2.0f * PI )) {
            std::cout << "WARNING: Mod2Pi_rad() --> angle not in range [0, 2 PI) after operation: " << fRadAngle << std::endl;
        }
        return fRadAngle;
    }

    // function to check if fLeftA <= fA <= fRghtA (mod 2 PI)
    bool AngleInSector( float fA, float fLeftA, float fRghtA ) {
        // check if FoV cone spans 360/0 transition angle
        if (fLeftA > fRghtA) {
            return InBetween( fA, fLeftA, 2.0f * PI ) ||
                   InBetween( fA,   0.0f, fRghtA );
        }
        return InBetween( fA, fLeftA, fRghtA );
    }

    // Functions for occlusion rendering
    // =================================

    // returns the angle (radians) from the player to location
    // NOTE: whilst atan2f() returns in range [-PI, +PI], this function returns in range [0, 2 PI)
    float GetAngle_PlayerToLocation( olc::vf2d location ) {
        olc::vf2d vecToLoc = location - olc::vf2d( fPlayerX, fPlayerY );
        return Mod2Pi_rad( atan2f( vecToLoc.y, vecToLoc.x ));
    }

    // returns the distance between the player and location
    float GetDistance_PlayerToLocation( olc::vf2d location ) {
        olc::vf2d vecToLoc = location - olc::vf2d( fPlayerX, fPlayerY );
        return vecToLoc.mag();
    }

    // returns the world coordinates of one of the columns of one of the faces of the denoted tile
    // * (nTileX, nTileY) - the coordinated of the tile in the map
    // * nFace            - denotes which face must be picked
    // * bLeft            - signals to return either the left column (if true) or the right column (if false)
    olc::vf2d GetColumnCoordinates( int nTileX, int nTileY, int nFace, bool bLeft ) {
        switch (nFace) {
            case EAST : return bLeft ? olc::vf2d( nTileX + 1.0f, nTileY + 1.0f ) : olc::vf2d( nTileX + 1.0f, nTileY        );
            case SOUTH: return bLeft ? olc::vf2d( nTileX       , nTileY + 1.0f ) : olc::vf2d( nTileX + 1.0f, nTileY + 1.0f );
            case WEST : return bLeft ? olc::vf2d( nTileX       , nTileY        ) : olc::vf2d( nTileX       , nTileY + 1.0f );
            case NORTH: return bLeft ? olc::vf2d( nTileX + 1.0f, nTileY        ) : olc::vf2d( nTileX       , nTileY        );
        }
        std::cout << "WARNING: GetColumnCoordinates() --> called with unknown nFace value: " << nFace << std::endl;
        return olc::vf2d( -1.0f, -1.0f );
    }

    // returns true if the tile at (nTileX, nTileY) is within the field of view of the player
    // even if the tile is only partially within the FoV: it checks if any of the columns of any of the
    // four faces is within the FoV sector
    bool TileInFoV( int nTileX, int nTileY ) {

        // convert angles to radians to build left and right cone boundaries
        float fPlayerFoV_rad = Deg2Rad( fPlayerFoV_deg );
        // determine FoV boundary angles to compare against
        float fLeftAngleBoundary = Mod2Pi_rad( fPlayerA_rad - fPlayerFoV_rad * 0.5 );
        float fRghtAngleBoundary = Mod2Pi_rad( fPlayerA_rad + fPlayerFoV_rad * 0.5 );

        // Check for each of the four columns of this cell whether it is in the FoV cone
        // Quit upon first true
        bool bResult = false;
        for (int f = EAST; f <= NORTH && !bResult; f++) {
            // get next column point for this tile
            olc::vf2d colPoint = GetColumnCoordinates( nTileX, nTileY, f, true );
            // determine angle from player to tile center
            float fAngleToPoint_rad = GetAngle_PlayerToLocation( colPoint );
            // check whether the angle is with the FoV cone
            bResult = AngleInSector( fAngleToPoint_rad, fLeftAngleBoundary, fRghtAngleBoundary );
        }
        return bResult;
    }

    // selects only the tiles that are in the FoV of the player, doesn't init the faces of these tiles
    void GetVisibleTiles( std::vector<TileInfo> &vVisibleTiles ) {
        for (int y = 0; y < nMapY; y++) {
            for (int x = 0; x < nMapX; x++) {
                if (sMap[ y * nMapX + x ] != '.' && TileInFoV( x, y )) {
                    TileInfo newTile;
                    newTile.TileID = olc::vi2d( x, y );
                    vVisibleTiles.push_back( newTile );
                }
            }
        }
    }

    // checks on ...
    //   * face direction irt player angle,
    //   * face direction irt tile and player location
    //   * occlusion by other tiles
    // ... to determine and return visibility of face
    bool FaceVisible( int nTileX, int nTileY, int nFace ) {
        // get boundary angles for FoV - in radians for calls to AngleInSector()
        float fFOVleft_rad = Deg2Rad( Mod360_deg( fPlayerA_deg - fPlayerFoV_deg / 2 ));
        float fFOVrght_rad = Deg2Rad( Mod360_deg( fPlayerA_deg + fPlayerFoV_deg / 2 ));
        // evaluate direction of player
        bool bRt = AngleInSector( fFOVleft_rad, 1.5f * PI, 0.5f * PI ) || AngleInSector( fFOVrght_rad, 1.5f * PI, 0.5f * PI );
        bool bUp = AngleInSector( fFOVleft_rad, 1.0f * PI, 2.0f * PI ) || AngleInSector( fFOVrght_rad, 1.0f * PI, 2.0f * PI );
        bool bDn = AngleInSector( fFOVleft_rad, 0.0f * PI, 1.0f * PI ) || AngleInSector( fFOVrght_rad, 0.0f * PI, 1.0f * PI );
        bool bLt = AngleInSector( fFOVleft_rad, 0.5f * PI, 1.5f * PI ) || AngleInSector( fFOVrght_rad, 0.5f * PI, 1.5f * PI );
        // determine face visibility
        switch (nFace) {
            // faces are not visible
            //   1. from outside map boundaries,
            //   2. if there's another non empty cell in front
            //   3. the look direction and position of the player don't allow visibility
            case EAST : return nTileX < nMapX - 1 && sMap[  nTileY      * nMapX + (nTileX + 1) ] != '#' && bLt && (fPlayerX > float( nTileX + 1 ));
            case WEST : return nTileX >         0 && sMap[  nTileY      * nMapX + (nTileX - 1) ] != '#' && bRt && (fPlayerX < float( nTileX     ));
            case SOUTH: return nTileY < nMapY - 1 && sMap[ (nTileY + 1) * nMapX +  nTileX      ] != '#' && bUp && (fPlayerY > float( nTileY + 1 ));
            case NORTH: return nTileY >         0 && sMap[ (nTileY - 1) * nMapX +  nTileX      ] != '#' && bDn && (fPlayerY < float( nTileY     ));
        }
        std::cout << "WARNING: FaceVisible() --> unknown nFace value: " << nFace << std::endl;
        return false;
    }


    // precondition - input parameter is in [0, 2 PI)
    // calculate projection onto screen column, by first working out the angle from player as a % of the players FOV,
    // and then multiply that % by screen width
    int GetColumnProjection( float fAngleFromPlayer_rad ) {
        // This function took me quite some time to get right. See the separate test program specifically made
        // for testing and tuning this function

        // determine angle of left boundary of FOV cone
        float fHalfFoV_rad = Deg2Rad( fPlayerFoV_deg / 2 );
        float fFOVRay0Angle_rad = Mod2Pi_rad( fPlayerA_rad - fHalfFoV_rad );
        // determine view angle associated with fAngleFromPlayer
        float fViewAngle_rad;
        // check if FoV cone spans over the 0/360 angle, then it could be the case that left boundary angle is
        // larger than angle from player to screen column, so check on that too
        if (fFOVRay0Angle_rad > fAngleFromPlayer_rad) {
            fViewAngle_rad = fAngleFromPlayer_rad + 2.0f * PI - fFOVRay0Angle_rad;
        } else {
            fViewAngle_rad = fAngleFromPlayer_rad             - fFOVRay0Angle_rad;
        }
        // make sure the angle sign is flipped at exactly the correct location behind the player.
        // if the angle is more than 180 degrees beyond player angle, interpret as a negative angle
        float fSectorAngle1 = PI + fHalfFoV_rad;
        float fSectorAngle2 = 2.0f * PI;
        if (AngleInSector( fViewAngle_rad, fSectorAngle1, fSectorAngle2 )) {
            fViewAngle_rad = fViewAngle_rad - 2.0f * PI;
        }
        // use view angle to work out percentage across FoV
        float fFoVPerc = fViewAngle_rad / (2.0f * fHalfFoV_rad);

        // multiply by screen width to get screen column
        return int( fFoVPerc * float( ScreenWidth() ));
    }

    // precondition - vVisibleTiles is filled with the tiles that are within the FoV of the player
    // processes each visible tile in vVisibleTiles to determine which of it's faces are visible.
    // the visible faces are processed both in vVisibleTiles, and put into vVisibleFaces
    // In the processing, the distance, angle from player to column, and projection on screen column is
    // determined for both columns of each visible face
    void GetVisibleFaces( std::vector<TileInfo> &vVisibleTiles, std::vector<FaceInfo> &vVisibleFaces ) {

        for (int i = 0; i < (int)vVisibleTiles.size(); i++) {
            TileInfo &curTile = vVisibleTiles[i];
            for (int face = EAST; face <= NORTH; face++) {

                if (FaceVisible( curTile.TileID.x, curTile.TileID.y, face )) {
                    // face is visible - add it to faces list
                    FaceInfo curFace;
                    curFace.TileID   = curTile.TileID;
                    curFace.nSide    = face;
                    curFace.bVisible = true;

                    // work out info for left column
                    ColInfo &left            = curFace.leftCol;
                    olc::vf2d leftCoords     = GetColumnCoordinates( curTile.TileID.x, curTile.TileID.y, face, true );
                    left.fAngleFromPlayer    = GetAngle_PlayerToLocation( leftCoords );
                    // get raw (uncorreced) distance for distance comparison
                    left.fDistFromPlayer_raw = GetDistance_PlayerToLocation( leftCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    left.fDistFromPlayer     = left.fDistFromPlayer_raw * abs( cosf( fPlayerA_rad - left.fAngleFromPlayer ));
                    // get projected screen column for left vertical edge of face
                    left.nScreenX            = GetColumnProjection( left.fAngleFromPlayer );

                    // work out info for right column
                    ColInfo &rght            = curFace.rghtCol;
                    olc::vf2d rghtCoords     = GetColumnCoordinates( curTile.TileID.x, curTile.TileID.y, face, false );
                    rght.fAngleFromPlayer    = GetAngle_PlayerToLocation( rghtCoords );
                    // get raw (uncorreced) distance for distance comparison
                    rght.fDistFromPlayer_raw = GetDistance_PlayerToLocation( rghtCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    rght.fDistFromPlayer     = rght.fDistFromPlayer_raw * abs( cosf( fPlayerA_rad - rght.fAngleFromPlayer ));
                    // get projected screen column for right vertical edge of face
                    rght.nScreenX            = GetColumnProjection( rght.fAngleFromPlayer );

                    // check on the resulted projections
                    if (left.nScreenX > rght.nScreenX) {
                        std::cout << "WARNING: GetVisibleFaces() --> projections are flipped (left = " << left.nScreenX << ", right = " << rght.nScreenX << ") for face: " << std::endl;
                        PrintFace( curFace );
                        std::cout << std::endl;
                    }

                    // fill faces list with same info
                    vVisibleFaces.push_back( curFace );
                } // if face is not visible, just ignore it
            }
        }

        // sort assembled faces list - from smallest to largest distance
        auto faces_sort_small2large = [=]( FaceInfo &a, FaceInfo &b ) {

            // use mean distance of the two columns as distance for the face
            return ((a.leftCol.fDistFromPlayer_raw + a.rghtCol.fDistFromPlayer_raw) / 2.0f) <
                   ((b.leftCol.fDistFromPlayer_raw + b.rghtCol.fDistFromPlayer_raw) / 2.0f);

            // I wonder - would taking the smallest of the columns' distances yield better results?
            // Well, I tried this and it doesn't seem to make any difference...
            // NOTE: I didn't retry this after introducing the raw distances
//            return (std::min( a.leftCol.fDistFromPlayer_raw, a.rghtCol.fDistFromPlayer_raw ) <
//                    std::min( b.leftCol.fDistFromPlayer_raw, b.rghtCol.fDistFromPlayer_raw ));
        };

        std::sort( vVisibleFaces.begin(), vVisibleFaces.end(), faces_sort_small2large );
    }

    // Convenience rendering functions
    // ===============================

    // Draw player as a little circle (with direction and FoV indicator) within the minimap
    // pos and tSize need to correspond with the parameters of RenderMiniMap()
    void RenderPlayerMiniMap( olc::vi2d pos, float fScale ) {
        olc::vi2d tSize = { int( fScale * 16 ), int( fScale * 16 ) };
        // draw player as a yellow circle in the map (use size / 4 as radius)
        olc::vf2d playerProj = {
            pos.x + fPlayerX * tSize.x,
            pos.y + fPlayerY * tSize.y
        };
        FillCircle( playerProj.x, playerProj.y, tSize.x / 4, olc::YELLOW );

        // little lambda for drawing direction "finger" from player towards fAngle_rad
        auto draw_finger = [=]( float fAngle_rad, int nMultiplier, olc::Pixel col ) {
            olc::vf2d fingerPoint = {
                pos.x + fPlayerX * tSize.x + cos( fAngle_rad ) * nMultiplier * fScale,
                pos.y + fPlayerY * tSize.y + sin( fAngle_rad ) * nMultiplier * fScale
            };
            DrawLine( playerProj, fingerPoint, col );
        };

        // draw player direction indicator in yellow
        draw_finger( fPlayerA_rad, 25, olc::YELLOW );
        // draw player FoV cone indicators in magenta
        draw_finger( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ), 50, olc::MAGENTA );
        draw_finger( Deg2Rad( fPlayerA_deg + fPlayerFoV_deg / 2 ), 50, olc::MAGENTA );
    }

    // Render the minimap at screen position pos, using tile render size tSize
    void RenderMiniMap( olc::vi2d pos, float fScale ) {
        olc::vi2d tSize = { int( fScale * 16 ), int( fScale * 16 ) };
        // first lay background for minimap
        FillRect( pos.x - 15, pos.y - 15, tSize.x * nMapX + 17, tSize.y * nMapY + 17, COL_BG );
        // render tiles in a grid, and put labels around it
        for (int y = 0; y < nMapY; y++) {
            DrawString( pos.x - 15, pos.y + tSize.y / 2 + y * tSize.y, std::to_string( y % 10 ), COL_TEXT );
            for (int x = 0; x < nMapX; x++) {
                if (sMap[y * nMapX + x] != '.') {
                    // if it's a tile within the FoV of the player, render in a separate colour
                    bool bTileVisible = TileInFoV( x, y );
                    olc::Pixel tileCol = bTileVisible ? olc::DARK_CYAN : olc::WHITE;
                    FillRect( pos.x + x * tSize.x, pos.y + y * tSize.y, tSize.x, tSize.y, tileCol );

                    // if the tile is visible, also check for visible faces and render these additionally
                    // as a red line within the cell (hence the offset by 1 pixel)
                    if (bTileVisible) {
                        olc::vi2d ul = olc::vi2d( pos.x + 1 +  x      * tSize.x, pos.y + 1 +  y      * tSize.y );
                        olc::vi2d lr = olc::vi2d( pos.x - 1 + (x + 1) * tSize.x, pos.y - 1 + (y + 1) * tSize.y );
                        for (int f = EAST; f <= NORTH; f++) {
                            if (FaceVisible( x, y, f )) {
                                olc::vi2d p1, p2;
                                switch (f) {
                                    case EAST : p1 = { lr.x, ul.y }; p2 = { lr.x, lr.y }; break;
                                    case WEST : p1 = { ul.x, ul.y }; p2 = { ul.x, lr.y }; break;
                                    case NORTH: p1 = { ul.x, ul.y }; p2 = { lr.x, ul.y }; break;
                                    case SOUTH: p1 = { ul.x, lr.y }; p2 = { lr.x, lr.y }; break;
                                }
                                DrawLine( p1, p2, olc::RED );
                            }
                        }
                    }
                }
                // in all cases draw a rectangle - this will emphasize non empty cells, and visualize empty cells
                DrawRect( pos.x + x * tSize.x, pos.y + y * tSize.y, tSize.x, tSize.y, olc::DARK_GREY );
            }
        }
        for (int x = 0; x < nMapX; x++) {
            DrawString( pos.x + tSize.x / 2 + x * tSize.x, pos.y - 15, std::to_string( x % 10 ), COL_TEXT );
        }

        // Render player in mini map
        RenderPlayerMiniMap( pos, fScale );
    }

    // Render some player info on screen at pos
    void RenderPlayerInfo( olc::vi2d pos ) {
        // first lay background for text drawing
        FillRect( pos.x - 4, pos.y - 4, 180, 20 + 15, COL_BG );
        // then render info on top
        DrawString( pos.x, pos.y +  0, "fPlayerX = " + std::to_string( fPlayerX     ), COL_TEXT );
        DrawString( pos.x, pos.y + 10, "fPlayerY = " + std::to_string( fPlayerY     ), COL_TEXT );
        DrawString( pos.x, pos.y + 20, "fPlayerA = " + std::to_string( fPlayerA_deg ), COL_TEXT );
    }

    std::string TextureMode2String( int nMode ) {
        switch (nMode) {
            case MONO  : return "MONO  ";
            case SPRITE: return "SPRITE";
            case DECAL : return "DECAL ";
        }
        return "ERROR ";
    }

    // Render some debug info on screen at pos
    void RenderDebugInfo( olc::vi2d pos ) {
        // first lay background for text drawing
        FillRect( pos.x - 4, pos.y - 4, 180, 40 + 15, COL_BG );
        // then render info on top
        DrawString( pos.x, pos.y +  0, "#tiles visbl = " + std::to_string( vTilesToRender.size() ), COL_TEXT );
        DrawString( pos.x, pos.y + 10, "#faces visbl = " + std::to_string( vFacesToRender.size() ), COL_TEXT );
        DrawString( pos.x, pos.y + 20, "#faces rndrd = " + std::to_string( nFacesRendered        ), COL_TEXT );
        DrawString( pos.x, pos.y + 30, "occList size = " + std::to_string(        occList.size() ), COL_TEXT );
        DrawString( pos.x, pos.y + 40, "texture mode = " + TextureMode2String( nTextureMode      ), COL_TEXT );
    }

    // if bHorizontal is true, render horizontal grid lines every 10 pixels.
    // Mark the 50 and 100 grid lines in alternative pattern, and label the 100 grid lines.
    // Similar for bVertical
    void RenderRaster( bool bHorizontal, bool bVertical ) {
        if (bVertical) {
            for (int x = 0; x < ScreenWidth(); x += 10) {
                uint32_t linePat = 0x11111111;
                if (x %  50 == 0) linePat = 0x33333333;
                if (x % 100 == 0) linePat = 0xF0F0F0F0;
                DrawLine( x, 0, x, ScreenHeight(), olc::BLACK, linePat );
                if (x % 100 == 0) DrawString( x - 4, ScreenHeight() - 12, std::to_string( x ), COL_TEXT );
            }
        }
        if (bHorizontal) {
            for (int y = 0; y < ScreenHeight(); y += 10) {
                uint32_t linePat = 0x11111111;
                if (y %  50 == 0) linePat = 0x33333333;
                if (y % 100 == 0) linePat = 0xF0F0F0F0;
                DrawLine( 0, y, ScreenWidth(), y, olc::BLACK, linePat );
                if (y % 100 == 0) DrawString( 2, y - 4, std::to_string( y ), COL_TEXT );
            }
        }
    }

    // monochrome (non textured) version
    // Fills a quad whose corner points are specified in curFace. Restricts rendering between screen columns as
    // denoted by nLeftClip and nRghtClip
    void RenderWallQuad_mono( FaceInfo &curFace, int nLeftClip, int nRghtClip ) {

        // work out this face's left column points
        float leftProjectionHeight = fDistToProjPlane / curFace.leftCol.fDistFromPlayer;
        olc::vf2d left_upper = { float( curFace.leftCol.nScreenX ), (ScreenHeight() - leftProjectionHeight) * 0.5f };
        olc::vf2d left_lower = { float( curFace.leftCol.nScreenX ), (ScreenHeight() + leftProjectionHeight) * 0.5f };
        // work out right column points
        float rghtProjectionHeight = fDistToProjPlane / curFace.rghtCol.fDistFromPlayer;
        olc::vf2d rght_upper = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() - rghtProjectionHeight) * 0.5f };
        olc::vf2d rght_lower = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() + rghtProjectionHeight) * 0.5f };

        // synthetic wall shading
        auto get_face_colour = [=]( int nFace ) {
            int nRGB = 255;
            switch (nFace) {
                case EAST : nRGB = 200; break;
                case SOUTH: nRGB = 120; break;
                case WEST : nRGB =  80; break;
                case NORTH: nRGB = 160; break;

                default: std::cout << "WARNING: RenderWallQuad_mono()/get_face_colour() --> unknown face value: " << nFace << std::endl;
            }
            return olc::Pixel( nRGB, nRGB, nRGB );
        };

        // clip horizontal rendering both by screen boundaries and clip coordinates
        int nRenderStrt = std::max( {                 0, curFace.leftCol.nScreenX, nLeftClip } );
        int nRenderStop = std::min( { ScreenWidth() - 1, curFace.rghtCol.nScreenX, nRghtClip } );

        // variables for clipped wire frame points
        olc::vf2d wf_ul, wf_ur, wf_ll, wf_lr;  // upper left/right & lower left/right

        float fMeanDistance = (curFace.leftCol.fDistFromPlayer_raw + curFace.rghtCol.fDistFromPlayer_raw) / 2.0f;
        olc::Pixel quadColour = get_face_colour( curFace.nSide ) * (1.0f - std::min( 1.0f, fMeanDistance / fRenderMaxDist ));

        // draw interior of quad by lerping
        for (int x = nRenderStrt; x <= nRenderStop; x++) {
            float t = float( x - curFace.leftCol.nScreenX ) / float( curFace.rghtCol.nScreenX - curFace.leftCol.nScreenX );
            float y_upper = left_upper.y + (rght_upper.y - left_upper.y) * t;
            float y_lower = left_lower.y + (rght_lower.y - left_lower.y) * t;

            // save clipped border points for wire frame drawing
            // (note that y coordinate is not clipped yet)
            if (x == nRenderStrt) {
                wf_ul = { float( x ), y_upper };
                wf_ll = { float( x ), y_lower };
            }
            if (x == nRenderStop) {
                wf_ur = { float( x ), y_upper };
                wf_lr = { float( x ), y_lower };
            }

            // clamp y values to be within screen boundaries
            y_upper = std::max( float(                  0 ), y_upper );
            y_lower = std::min( float( ScreenHeight() - 1 ), y_lower );
            // draw vertical lines to build up the quad interior
            DrawLine( x, y_upper, x, y_lower, quadColour );
        }

        bool bMonoColour = true;

        if (bWireFrameMode) {
            // draw the quad as a wire frame - offset by 1 pixel to improve visibility
            // NOTE - this line drawing is clipped in horizontal direction, so doesn't necessarily reflect the original shape!
            DrawLine( wf_ul + olc::vf2d( +1,  0 ), wf_ll + olc::vf2d( +1,  0 ), bMonoColour ? olc::BLACK : olc::RED   );    // left  column
            DrawLine( wf_ur + olc::vf2d( -1,  0 ), wf_lr + olc::vf2d( -1,  0 ), bMonoColour ? olc::BLACK : olc::GREEN );    // right column
            DrawLine( wf_ul + olc::vf2d(  0, +1 ), wf_ur + olc::vf2d(  0, +1 ), bMonoColour ? olc::BLACK : olc::WHITE );    // top   side
            DrawLine( wf_ll + olc::vf2d(  0, -1 ), wf_lr + olc::vf2d(  0, -1 ), bMonoColour ? olc::BLACK : olc::BLUE  );    // floor side
        }
    }

    // decal based version
    // Fills a quad whose corner points are specified in curFace. Restricts rendering between screen columns as
    // denoted by nLeftClip and nRghtClip
    void RenderWallQuad_decal( FaceInfo &curFace, int nLeftClip, int nRghtClip ) {

        // work out this face's left column points
        float leftProjHeight = fDistToProjPlane / curFace.leftCol.fDistFromPlayer;
        olc::vf2d left_upper = { float( curFace.leftCol.nScreenX ), (ScreenHeight() - leftProjHeight) * 0.5f };
        olc::vf2d left_lower = { float( curFace.leftCol.nScreenX ), (ScreenHeight() + leftProjHeight) * 0.5f };
        // work out right column points
        float rghtProjHeight = fDistToProjPlane / curFace.rghtCol.fDistFromPlayer;
        olc::vf2d rght_upper = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() - rghtProjHeight) * 0.5f };
        olc::vf2d rght_lower = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() + rghtProjHeight) * 0.5f };
        // clip horizontal rendering both by screen boundaries and clip coordinates
        int nRenderStrt = std::max( {                 0, curFace.leftCol.nScreenX, nLeftClip } );
        int nRenderStop = std::min( { ScreenWidth() - 1, curFace.rghtCol.nScreenX, nRghtClip } );

        // lerp coordinates into clip range
        float t1 = float( nRenderStrt - curFace.leftCol.nScreenX ) / float( curFace.rghtCol.nScreenX - curFace.leftCol.nScreenX );
        float y1_upper = left_upper.y + (rght_upper.y - left_upper.y) * t1;
        float y1_lower = left_lower.y + (rght_lower.y - left_lower.y) * t1;
        float t2 = float( nRenderStop - curFace.leftCol.nScreenX ) / float( curFace.rghtCol.nScreenX - curFace.leftCol.nScreenX );
        float y2_upper = left_upper.y + (rght_upper.y - left_upper.y) * t2;
        float y2_lower = left_lower.y + (rght_lower.y - left_lower.y) * t2;

        // by picking the right decal you also pick the right colour
        // this could be done by tinting a uniform decal as well
        olc::Decal *pCurrentDecal = bWireFrameMode ? pDecalWallsB[curFace.nSide] : pDecalWalls[curFace.nSide];

        // determine position and size to render partial decal
        float decalW = pCurrentDecal->sprite->width;
        float decalH = pCurrentDecal->sprite->height;
        olc::vf2d quadPos  = {       t1  * decalW,   0.0f };
        olc::vf2d quadSize = { (t2 - t1) * decalW, decalH };

        // convert to std::array for call with DrawPartialWarpedDecal()
        std::array<olc::vf2d, 4> quadPoints = {
            olc::vf2d( float( nRenderStrt ), y1_upper ),
            olc::vf2d( float( nRenderStrt ), y1_lower ),
            olc::vf2d( float( nRenderStop + 1 ), y2_lower ),
            olc::vf2d( float( nRenderStop + 1 ), y2_upper )
        };

        float fMeanDistance = (curFace.leftCol.fDistFromPlayer_raw + curFace.rghtCol.fDistFromPlayer_raw) / 2.0f;
        olc::Pixel quadColour = olc::WHITE * (1.0f - std::min( 1.0f, fMeanDistance / fRenderMaxDist ));

        // render the quad
        DrawPartialWarpedDecal( pCurrentDecal, quadPoints, quadPos, quadSize, quadColour );
    }

    void RenderWallQuad_sprite( FaceInfo &curFace, int nLeftClip, int nRghtClip ) {

        // work out this face's left column points
        float leftProjectionHeight = fDistToProjPlane / curFace.leftCol.fDistFromPlayer;
        olc::vf2d left_upper = { float( curFace.leftCol.nScreenX ), (ScreenHeight() - leftProjectionHeight) * 0.5f };
        olc::vf2d left_lower = { float( curFace.leftCol.nScreenX ), (ScreenHeight() + leftProjectionHeight) * 0.5f };
        // work out right column points
        float rghtProjectionHeight = fDistToProjPlane / curFace.rghtCol.fDistFromPlayer;
        olc::vf2d rght_upper = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() - rghtProjectionHeight) * 0.5f };
        olc::vf2d rght_lower = { float( curFace.rghtCol.nScreenX ), (ScreenHeight() + rghtProjectionHeight) * 0.5f };
        // clip horizontal rendering both by screen boundaries and clip coordinates
        int nRenderStrt = std::max( {                 0, curFace.leftCol.nScreenX, nLeftClip } );
        int nRenderStop = std::min( { ScreenWidth() - 1, curFace.rghtCol.nScreenX, nRghtClip } );
        // convert to std::array for interfacing with DrawWarpedSprite()
        std::array<olc::vf2d, 4> quadPoints = {
            left_upper,
            left_lower,
            rght_lower,
            rght_upper
        };

        float fMeanDistance = (curFace.leftCol.fDistFromPlayer_raw + curFace.rghtCol.fDistFromPlayer_raw) / 2.0f;
        float fShadeFactor = 1.0f - std::min( 1.0f, fMeanDistance / fRenderMaxDist );
        // render the quad
        DrawWarpedSpriteClipped( this, bWireFrameMode ? brickTextureB : brickTexture, quadPoints, nRenderStrt, nRenderStop, fShadeFactor );
    }

    // Occlusion list stuff [ I could put this in it's own class definition ]
    // ====================

    typedef struct sOcclusionRec {
        int left, rght;
    } OcclusionRec;
    typedef std::list<OcclusionRec> OccListType;
    typedef OccListType::iterator   OccIterType;

    OccListType occList;

    // output occlusion list to screen (for debugging)
    void PrintOccList( OccListType &lst, const std::string &sMsg = "" ) {
        if (sMsg.length() > 0) {
            std::cout << sMsg << std::endl;
        }
        OccIterType iter = lst.begin();
        for (int i = 0; i < (int)lst.size(); i++) {
            std::cout << "[ " << ((*iter).left == INT_MIN ? "INT_MIN" : std::to_string( (*iter).left));
            std::cout << ", " << ((*iter).rght == INT_MAX ? "INT_MAX" : std::to_string( (*iter).rght));
            std::cout << " ], ";
            iter++;
        }
        std::cout << std::endl;
    }

    // Initialises the occlusion list with the two extreme elements left and right outside screen boundaries
    // This ensures two things:
    //   * during processing there are always at least two elements in the list
    //   * no occlusion range will ever be processed that extends beyound the initial boundary values
    void InitOccList( OccListType &lst ) {
        OcclusionRec auxLeft = {       INT_MIN,      -1 };   // non visible intervals on the left and right
        OcclusionRec auxRght = { ScreenWidth(), INT_MAX };   // of the screen

        lst.clear();
        lst.push_front( auxLeft );
        lst.push_back(  auxRght );
    }

    // if the size of the occlusion list becomes 1, the screen is totally occluded (and
    // processing more faces can be stopped)
    int SizeOccList( OccListType &lst ) { return (int)lst.size(); }

    // returns true if this occlusion rec should be (partially) rendered. If so, nClipLeft and nClipRght denote
    // clipping values
    // PRECONDITION: the list contains at least two elements, and the extreme values for these elements are
    // INT_MIN and INT_MAX. In practical terms this means:
    //   * InitOccList() must have been called at some point before calling this function
    //   * A check must have been done (with SizeOccList()) to ensure that the list contains at least two elements
    bool InsertOccList( OccListType &lst, OcclusionRec &rec, int &nClipLeft, int &nClipRght ) {

        // the code is extensively commented since this is a complex function
        // ------------------------------------------------------------------
        if (lst.size() < 2) {
            std::cout << "ERROR: InsertOccList() --> called with too little elements: " << (int)lst.size() << std::endl;
            return false;
        }
        if (lst.front().left != INT_MIN || lst.back().rght != INT_MAX) {
            std::cout << "ERROR: InsertOccList() --> something's wrong with list boundary values..." << std::endl;
            return false;
        }

        // 1. search through list until insertion point is found
        // -----------------------------------------------------

        // maintain two iterators. Insertion can be done between those
        OccIterType iterLeft = lst.begin();
        OccIterType iterRght = std::next( iterLeft );
        bool bFound = false;

        // scan the list until either insertion point is found or all possibilities are evaluated
        do {

            // The insertion point is found iff
            //   A. left  part of rec overlaps with right side of left  element, or
            //   B. right part of rec overlaps with left  side of right element, or
            //   C. rec doesn't overlap with neither left nor right, but its values are in between left and right
            //      i.e. left.rightval < rec.leftval and rec.rightval < right.leftval
            bFound = iterRght != lst.end() && (
                InBetween( rec.left, (*iterLeft).left, (*iterLeft).rght ) ||  // overlap between left element and rec
                InBetween( rec.rght, (*iterRght).left, (*iterRght).rght ) ||  // overlap between rec and right element
                (
                    rec.left > (*iterLeft).rght &&     // left and rec may be adjacent, but there's no overlap
                    rec.rght < (*iterRght).left        // same for rec and rght element
                )
            );
            // only advance iterators if insertion point is not yet found
            if (!bFound) {
                iterLeft++;
                iterRght++;
            }
        } while (!bFound && iterRght != lst.end());

        // Intermezzo - At this point in the code, an insertion point is either found or not.
        //              If it's found, insertion or adaption of the list must be done between iterLeft and iterRght;
        //              If it's not found, the occlusion range in rec is already fully occluded, and
        //              can be skipped;

        if (!bFound) {
            // 2a. if location is not found, return false
            // ------------------------------------------
            nClipLeft = -1;
            nClipRght = -2;   // setting clip values to same element will return false

        } else {
            // 2b. if location is found, process this element
            // ----------------------------------------------

            // we know that this is the right place to insert rec, but there could be overlap
            // (or adjacency) both on the left side and on the right side

            // Intermezzo - overlap is when two ranges have at least one screen column in common.
            //              when there's no overlap, but there's also no gap in the screen columns between them,
            //              I call them adjacent.
            //              For the insertion overlap and adjacency must be treated the same way.

            // 3. Check if the left element can be extended
            if ((*iterLeft).rght + 1 >= rec.left) {

                // 3a. there is overlap (or adjacency): EXTEND the EXISTING element
                nClipLeft = (*iterLeft).rght + 1;
                (*iterLeft).rght = std::max( (*iterLeft).rght, rec.rght );
                // check on a situation that should not occur
                if ((*iterLeft).left > rec.left) {
                    std::cout << "WARNING: InsertOccList() --> inserted record extends left from left element..." << std::endl;
                }
            } else {

                // 3b. there is NO overlap, INSERT the NEW element between iterLeft and iterRght
                //     and set iterLeft to point at it
                nClipLeft = rec.left;
                iterLeft = lst.insert( iterRght, rec );
            }

            // Intermezzo - Either a new record is inserted and iterLeft points to that new record, or
            //              the existing record is adapted to absorb the range in rec and iterLeft points to the
            //              existing record.
            //              Either way, iterLeft points to the record of which the right value must possibly
            //              be merged with the element pointed to by iterRght

            // 4a. Set right clipping value
            if ((*iterLeft).rght + 1 >= (*iterRght).left) {
                nClipRght = (*iterRght).left - 1;
            } else {
                nClipRght = rec.rght;
            }
            // 4b. Check if a merge with one or more elements to the right is needed
            while ((*iterLeft).rght + 1 >= (*iterRght).left && iterRght != lst.end()) {

                // 4c. there is overlap (or adjacency): EXTEND the LEFT element, ...
                (*iterLeft).left = std::min( (*iterLeft).left, (*iterRght).left );
                (*iterLeft).rght = std::max( (*iterLeft).rght, (*iterRght).rght );
                // ..., ERASE the RIGHT element and set iterRght to the element after it
                iterRght = lst.erase( iterRght );
            }
        }

        return nClipLeft <= nClipRght;
    }

    bool OnUserUpdate( float fElapsedTime ) override {

        bTestMode = false;

        // step 1 - user input
        // ===================

        // factor to speed up or slow down
        float fSpeedUp = 1.0f;
        if (GetKey( olc::Key::SHIFT ).bHeld) fSpeedUp *= 4.00f;
        if (GetKey( olc::Key::CTRL  ).bHeld) fSpeedUp *= 0.10f;

        // little lambda to keep derived var's sync'd with fPlayerA_deg
        auto sync_angle_vars = [&]() {
            fPlayerA_deg = Mod360_deg( fPlayerA_deg );
            fPlayerA_rad = Deg2Rad(    fPlayerA_deg );
            fPlayerSin   = sin(        fPlayerA_rad );
            fPlayerCos   = cos(        fPlayerA_rad );
        };

        // rotate, and keep player angle in [0, 360) range. This should be the only place in the code
        // where fPlayerA_deg is altered - keep derived var's sync'd
        if (GetKey( olc::D ).bHeld) { fPlayerA_deg += fSpeedUp * SPEED_ROTATE * fElapsedTime; sync_angle_vars(); }
        if (GetKey( olc::A ).bHeld) { fPlayerA_deg -= fSpeedUp * SPEED_ROTATE * fElapsedTime; sync_angle_vars(); }

        // temporary placeholders for (possible) new player location
        float fNewX = fPlayerX;
        float fNewY = fPlayerY;

        float fCDMargin = 0.25f;   // collision detection margin (in cells)
        // variables to aid in collision detection check
        float fCheckX;
        float fCheckY;

        // walk forward - collision detection checked
        if (GetKey( olc::W ).bHeld) {
            fNewX += fPlayerCos * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fNewY += fPlayerSin * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fCheckX = fNewX + fPlayerCos * fCDMargin;
            fCheckY = fNewY + fPlayerSin * fCDMargin;
        }
        // walk backwards - collision detection checked
        if (GetKey( olc::S ).bHeld) {
            fNewX -= fPlayerCos * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fNewY -= fPlayerSin * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fCheckX = fNewX - fPlayerCos * fCDMargin;
            fCheckY = fNewY - fPlayerSin * fCDMargin;
        }
        // strafe left - collision detection checked
        if (GetKey( olc::Q ).bHeld) {
            fNewX += fPlayerSin * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fNewY -= fPlayerCos * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fCheckX = fNewX + fPlayerSin * fCDMargin;
            fCheckY = fNewY - fPlayerCos * fCDMargin;
        }
        // strafe right - collision detection checked
        if (GetKey( olc::E ).bHeld) {
            fNewX -= fPlayerSin * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fNewY += fPlayerCos * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fCheckX = fNewX - fPlayerSin * fCDMargin;
            fCheckY = fNewY + fPlayerCos * fCDMargin;
        }
        // collision detection - check if out of bounds or inside occupied tile
        if (fCheckX >= 0 && fCheckX < nMapX &&
            fCheckY >= 0 && fCheckY < nMapY &&
            sMap[ int( fCheckY ) * nMapX + int( fCheckX ) ] != '#') {
            fPlayerX = fNewX;
            fPlayerY = fNewY;
        }
        // toggle raster flags
        if (GetKey( olc::Key::V ).bPressed) bVerRasterMode = !bVerRasterMode;
        if (GetKey( olc::Key::H ).bPressed) bHorRasterMode = !bHorRasterMode;
        // toggle textured / monochrome rendering flag
        if (GetKey( olc::Key::R ).bPressed) nTextureMode = (nTextureMode + 1) % 3;
        // toggle wire frame (icw monochrome) rendering flag
        if (GetKey( olc::Key::B ).bPressed) bWireFrameMode = !bWireFrameMode;
        // toggle map flag
        if (GetKey( olc::Key::M ).bPressed) bMapMode = !bMapMode;
        // toggle info on screen flag
        if (GetKey( olc::Key::I ).bPressed) bInfoMode = !bInfoMode;
        // manipulate map scale
        if (GetKey( olc::NP_ADD ).bHeld) fMapScale += 1.0f * fElapsedTime;
        if (GetKey( olc::NP_SUB ).bHeld) fMapScale -= 1.0f * fElapsedTime;

        // step 2 - game logic
        // ===================

        // step 3a - render logic
        // ======================

        // collect all tiles that are visible (i.e. who have at least one
        // face column within the players FoV) in the global tiles to render list
        vTilesToRender.clear();
        GetVisibleTiles( vTilesToRender );

        // from the visible tiles list, analyse which of the faces are potentially
        // visible for the player. This faces to render list is sorted from close by to far away
        vFacesToRender.clear();
        GetVisibleFaces( vTilesToRender, vFacesToRender );

        // test output
        if (GetKey( olc::Key::T ).bPressed) { bTestMode = true; }
        if (bTestMode) {
            PrintTilesList( vTilesToRender );
            PrintFacesList( vFacesToRender );
        }

        // step 3b - render
        // ================

        SetDrawTarget( nLayerBG );
        DrawDecal( { 0.0f, 0.0f }, pDecalBG );

        SetDrawTarget( nLayerScene );
        Clear( olc::BLANK );  // Use blank to keep the background layer visible

        // iterate over visible faces list - use the occlusion list approach to determine whether
        // faces are (partly) occluded, and draw them as quads

        if (bTestMode) PrintOccList( occList, "Before InitOccList()" );

        InitOccList( occList );

        if (bTestMode) PrintOccList( occList, "After InitOccList()" );

        nFacesRendered = 0;
        for (int i = 0; i < (int)vFacesToRender.size() && (int)SizeOccList( occList ) > 1; i++) {
            FaceInfo &curFace = vFacesToRender[i];

            OcclusionRec occRec = { curFace.leftCol.nScreenX, curFace.rghtCol.nScreenX };
            int nClipLt, nClipRt;

            if (bTestMode) PrintOccList( occList, "Before InsertOccList()" );
            if (bTestMode) std::cout << "Occ.record contains - left: " << occRec.left << ", right: " << occRec.rght << std::endl;

            bool bInsertResult = InsertOccList( occList, occRec, nClipLt, nClipRt );

            if (bTestMode) PrintOccList( occList, "After InsertOccList()" );
            if (bTestMode) std::cout << "Call returned: " << (bInsertResult ? "TRUE ," : "FALSE,") << "clip values - left: " << nClipLt << ", right: " << nClipRt << std::endl;

            if (bInsertResult) {

                // (at least a part of this) face is visible (not occluded) so render that part
                switch (nTextureMode) {
                    case MONO  : RenderWallQuad_mono(   curFace, nClipLt, nClipRt ); break;
                    case SPRITE: RenderWallQuad_sprite( curFace, nClipLt, nClipRt ); break;
                    case DECAL : RenderWallQuad_decal(  curFace, nClipLt, nClipRt ); break;
                }
                nFacesRendered += 1;
            }
        }

        SetDrawTarget( nLayerHUD );
        Clear( olc::BLANK );

        // render optional horizontal and/or vertical raster lines (for testing)
        RenderRaster( bHorRasterMode, bVerRasterMode );

        if (bMapMode) {
            // render minimap (for debugging) and player in it
            olc::vi2d position = { 50, 50 };
            RenderMiniMap( position, fMapScale );
        }

        if (bInfoMode) {
            // render player values for debugging
            RenderPlayerInfo( { ScreenWidth() / 2 -  75, 10 } );
            // render characteristics on rendering algorithm
            RenderDebugInfo(  { ScreenWidth()     - 200, 10 } );
        }

        return true;
    }
};

int main()
{
	AlternativeRayCaster demo;
	if (demo.Construct( SCREEN_X / PIXEL_X, SCREEN_Y / PIXEL_Y, PIXEL_X, PIXEL_Y ))
		demo.Start();

	return 0;
}
