// Alternative ray caster using sprite warped rendering
//
// Joseph21, november 5, 2023
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)
//

/* Short description
   -----------------
   bla

   To do
   -----
   * Improve TileInFoV() accuracy
   * There's a bug in TileInFoV() - when player angle switches from 0 to
   * Fix fish eye effect
   * Replace painters algo in GetVisibleFaces() with more enhanced (doom like) algo

    Have fun!
 */

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define PI 3.1415926535f

// Screen and pixel constants - keep the screen sizes constant and vary the resolution by adapting the pixel size
// to prevent accidentally defining too large a window
#define SCREEN_X    1400
#define SCREEN_Y     800
#define PIXEL_X        1
#define PIXEL_Y        1

// colour constants
#define COL_CEIL    olc::DARK_BLUE
#define COL_FLOOR   olc::DARK_YELLOW
#define COL_WALL    olc::GREY
#define COL_TEXT    olc::MAGENTA

// constants for speed movements - all movements are modulated with fElapsedTime
#define SPEED_ROTATE      60.0f   //                          60 degrees per second
#define SPEED_MOVE         5.0f   // forward and backward -    5 units per second
#define SPEED_STRAFE       5.0f   // left and right strafing - 5 units per second

class AlternativeRayCaster : public olc::PixelGameEngine {

public:
    AlternativeRayCaster() {    // display the screen and pixel dimensions in the window caption
        sAppName = "Quad rendered RayCaster - S:(" + std::to_string( SCREEN_X / PIXEL_X ) + ", " + std::to_string( SCREEN_Y / PIXEL_Y ) + ")" +
                                           ", P:(" + std::to_string(            PIXEL_X ) + ", " + std::to_string(            PIXEL_Y ) + ")" ;
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

    float fPlayerA_rad = 0.0f;    // for interior working
    float fPlayerSin   = 0.0f;
    float fPlayerCos   = 0.0f;

    // player: height of eye point and field of view
    float fPlayerH       = 0.5f;
    float fPlayerFoV_deg = 60.0f;   // in degrees !!
    // distance to projection plane - needed for depth projection
    float fDistToProjPlane;

    bool bTestMode      = false;
    bool bHorRasterMode = false;
    bool bVerRasterMode = false;

    int nFacesRendered = 0;

    // column descriptor - a column is a strictly vertical line that is either the left or the right side of a face
    typedef struct sColDescriptor {
        int nScreenX = 0;                // projection of face column onto screen column
        float fAngleFromPlayer = 0.0f;   // angle
        float fDistFromPlayer = 0.0f;    // distance
    } ColInfo;

// types of faces
#define UNKNWN -1
#define EAST    0
#define SOUTH   1
#define WEST    2
#define NORTH   3

    // face descriptor - a face is one of the four sides of a non-empty cell/tile
    // each face has a left and right column, as viewed from the outside of the face
    typedef struct sFaceDescriptor {
        olc::vi2d TileID;          // coords of the tile this face belongs to
        int nSide = UNKNWN;        // one of EAST, SOUTH, WEST, NORTH
        bool bVisible = false;     // for culling

        ColInfo leftCol, rghtCol;  // info on the columns for this face
    } FaceInfo;

    // tile descriptor - a tile has coordinates in the map, and consists of four faces
    typedef struct sTileDescriptor {
        olc::vi2d TileID;          // coords of the tile
        FaceInfo faces[4];
    } TileInfo;

    std::vector<TileInfo> vTilesToRender;
    std::vector<FaceInfo> vFacesToRender;

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

        // work out distance to projection plane. This is a constant depending on the width of the projection plane and the field of view.
        fDistToProjPlane = ((ScreenWidth() / 2.0f) / sin( Deg2Rad( fPlayerFoV_deg / 2.0f ))) * cos( Deg2Rad( fPlayerFoV_deg / 2.0f ));

        return true;
    }

// prototypes - lots of optimizing possible and not applied yet!!!

    // converts degree angle to radian equivalent
    float Deg2Rad( float fDegAngle ) { return fDegAngle / 180.0f * PI; }
    // converts radian angle to degree equivalent
    float Rad2Deg( float fRadAngle ) { return fRadAngle * 180.0f / PI; }

    // returns true if f_low <= f <= f_hgh
    bool InBetween( float f, float f_low, float f_hgh ) { return (f_low <= f && f <= f_hgh); }

    // converts degree angle to equivalent in range [0, 360)
    float Mod360_deg( float fDegAngle ) {
        if (fDegAngle <    0.0f) fDegAngle += 360.0f;
        if (fDegAngle >= 360.0f) fDegAngle -= 360.0f;

        if (!InBetween( fDegAngle, 0.0f, 360.0f )) {
            std::cout << "WARNING: Mod360_deg() --> angle not in range [ 0, 360 ] after operation: " << fDegAngle << std::endl;
        }
        return fDegAngle;
    }

    // converts radian angle to equivalent in range [0, 2 PI)
    float Mod2Pi_rad( float fRadAngle ) {
        if (fRadAngle <  0.0f     ) fRadAngle += 2.0f * PI;
        if (fRadAngle >= 2.0f * PI) fRadAngle -= 2.0f * PI;

        if (!InBetween( fRadAngle, 0.0f, 2.0f * PI )) {
            std::cout << "WARNING: Mod2Pi_rad() --> angle not in range [ 0, 2 PI ] after operation: " << fRadAngle << std::endl;
        }
        return fRadAngle;
    }

    // returns the angle (radians) from the player to location
    // NOTE: whilst atan2f() returns in range [-PI, +PI], this function returns in range [ 0, 2 * PI )
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
    // * (nTileX, nTileY) - the coordinate pair of the tile in the map
    // * nFace            - denotes which face must be picked
    // * bLeft            - signals to return either the left column (if true) or the right column (if false)
    olc::vf2d GetColCoordinates( int nTileX, int nTileY, int nFace, bool bLeft ) {
        switch (nFace) {
            case EAST : return bLeft ? olc::vf2d( nTileX + 1.0f, nTileY + 1.0f ) : olc::vf2d( nTileX + 1.0f, nTileY        );
            case SOUTH: return bLeft ? olc::vf2d( nTileX       , nTileY + 1.0f ) : olc::vf2d( nTileX + 1.0f, nTileY + 1.0f );
            case WEST : return bLeft ? olc::vf2d( nTileX       , nTileY        ) : olc::vf2d( nTileX       , nTileY + 1.0f );
            case NORTH: return bLeft ? olc::vf2d( nTileX + 1.0f, nTileY        ) : olc::vf2d( nTileX       , nTileY        );
        }
        std::cout << "WARNING: GetColCoordinates() --> unknown nFace value: " << nFace << std::endl;
        return olc::vf2d( -1.0f, -1.0f );
    }

    bool TileInFoV( int nTileX, int nTileY ) {

        // convert angles to radians to build left and right cone boundaries
        float fPlayerFoV_rad = Deg2Rad( fPlayerFoV_deg );
        // determine FoV boundary angles to compare against
        float fLeftAngleBoundary = Mod2Pi_rad( fPlayerA_rad - fPlayerFoV_rad * 0.5 );
        float fRghtAngleBoundary = Mod2Pi_rad( fPlayerA_rad + fPlayerFoV_rad * 0.5 );

        // little lambda to check if fLeftA <= fA <= fRghtA (mod 2 PI)
        auto is_in_FoV_cone = [=]( float fA, float fLeftA, float fRghtA ) {
            bool bResult;
            // check if FoV cone spans 360/0 transition angle
            if (fLeftA > fRghtA) {
                bResult = InBetween( fA, fLeftA, 2.0f * PI ) ||
                          InBetween( fA,   0.0f, fRghtA );
            } else {
                bResult = InBetween( fA, fLeftA, fRghtA );
            }
            return bResult;
        };

        // Check for each of the four columns of this cell whether it is in the FoV cone
        // Quit upon first true
        bool bResult = false;
        for (int f = EAST; f <= NORTH && !bResult; f++) {
            // get next column point for this tile
            olc::vf2d colPoint = GetColCoordinates( nTileX, nTileY, f, true );
            // determine angle from player to tile center
            float fAngleToPoint_rad = GetAngle_PlayerToLocation( colPoint );
            // check whether the angle is with the FoV cone
            bResult = is_in_FoV_cone( fLeftAngleBoundary, fRghtAngleBoundary, fAngleToPoint_rad );
        }
        return bResult;
    }

    // checks on face direction icw player angle, in combination with tile and player location
    // to determine visibility of face
    bool FaceVisible( int nTileX, int nTileY, int nFace ) {
        // get boundary angles for FoV
        float fFOVleft = Mod360_deg( fPlayerA_deg - fPlayerFoV_deg / 2 );
        float fFOVrght = Mod360_deg( fPlayerA_deg + fPlayerFoV_deg / 2 );

        bool bUp, bRt;

        if (fFOVleft > fFOVrght) {
            bRt = true;
            bUp = true;

        } else {
            bRt = InBetween( fFOVleft,   0.0f,  90.0f ) || InBetween( fFOVleft, 270.0f, 360.0f ) ||
                  InBetween( fFOVrght,   0.0f,  90.0f ) || InBetween( fFOVrght, 270.0f, 360.0f );

            bUp = InBetween( fFOVleft, 180.0f, 360.0f ) || InBetween( fFOVrght, 180.0f, 360.0f );
        }

        bool bDn = InBetween( fFOVleft,   0.0f, 180.0f ) || InBetween( fFOVrght,   0.0f, 180.0f );
        bool bLt = InBetween( fFOVleft,  90.0f, 270.0f ) || InBetween( fFOVrght,  90.0f, 270.0f );

        switch (nFace) {
            case EAST : return bLt && (fPlayerX > float( nTileX + 1 ));
            case SOUTH: return bUp && (fPlayerY > float( nTileY + 1 ));
            case WEST : return bRt && (fPlayerX < float( nTileX     ));
            case NORTH: return bDn && (fPlayerY < float( nTileY     ));
        }
        std::cout << "WARNING: FaceVisible() --> unknown nFace value: " << nFace << std::endl;
        return false;
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

    // test output functions for ColInfo, FaceInfo, TileInfo and associated
    // Tile and Face lists

    void PrintColInfo( ColInfo &c ) {
        std::cout << "screen col: " << c.nScreenX         << ", ";
        std::cout << "plyr angle: " << c.fAngleFromPlayer << ", ";
        std::cout << "plyr dist: "  << c.fDistFromPlayer;
    }

    std::string Face2String( int nFace ) {
        std::string sResult;
        switch (nFace) {
            case UNKNWN: sResult = "UNKNWN"; break;
            case EAST  : sResult = "EAST  "; break;
            case SOUTH : sResult = "SOUTH "; break;
            case WEST  : sResult = "WEST  "; break;
            case NORTH : sResult = "NORTH "; break;
            default: sResult = " --- ERROR --- ";
        }
        return sResult;
    }

    void PrintFace( FaceInfo &f ) {
        std::cout << "face side: "  << Face2String( f.nSide ) << ", ";
        std::cout << "tile coord: " << f.TileID               << ", ";
        std::cout << (f.bVisible ? "IS  " : "NOT ") << "visible, ";

        std::cout << " LEFT column = " ; PrintColInfo( f.leftCol );
        std::cout << " RIGHT column = "; PrintColInfo( f.rghtCol );
    }

    void PrintTile( TileInfo &t ) {
        std::cout << "tile coord: " << t.TileID << std::endl;
        for (int f = EAST; f <= NORTH; f++) {
            PrintFace( t.faces[f] );
            std::cout << std::endl;
        }
    }

    void PrintTilesList( std::vector<TileInfo> &vVisibleTiles ) {
        for (int i = 0; i < (int)vVisibleTiles.size(); i++) {
            TileInfo &curTile = vVisibleTiles[i];
            std::cout << "Index: " << i << " - ";
            PrintTile( curTile );
            std::cout << std::endl;
        }
    }

    void PrintFacesList( std::vector<FaceInfo> &vVisibleFaces ) {
        for (int i = 0; i < (int)vVisibleFaces.size(); i++) {
            FaceInfo &curFace = vVisibleFaces[i];
            std::cout << "Index: " << i << " - ";
            PrintFace( curFace );
            std::cout << std::endl;
        }
    }

    // calculate angle from player as a % of the players FOV, and multiply by screen width
    // to get the projected column
    //
    // This function took me quite some time to get right. See the separate test program specifically made
    // for testing and tuning this function
    int GetColumnProjection( float fAngleFromPlayer_rad ) {

        // determine angle of left boundary of FOV cone
        float fFOVRay0Angle_rad = Mod2Pi_rad( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ));
        // determine view angle associated with fAngleFromPlayer
        float fViewAngle_rad;
        // check if FoV cone spans over the 0/360 angle, then it could be the case that left boundary angle is
        // larger than angle from player to screen column, so check on that too
        if (fFOVRay0Angle_rad > fAngleFromPlayer_rad) {
            fViewAngle_rad = fAngleFromPlayer_rad + (2.0f * PI - fFOVRay0Angle_rad);
        } else {
            fViewAngle_rad = fAngleFromPlayer_rad - fFOVRay0Angle_rad;
        }
        // make sure the coordinates stay on the right side
        if (InBetween( fViewAngle_rad, PI, 2.0f * PI )) {
            fViewAngle_rad = fViewAngle_rad - 2.0f * PI;
        }
        // use view angle to work out percentage across FoV
        float fFoVPerc = fViewAngle_rad / Deg2Rad( fPlayerFoV_deg );
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
                FaceInfo &curFace = curTile.faces[ face ];
                curFace.TileID = curTile.TileID;
                curFace.nSide = face;
                if (FaceVisible( curTile.TileID.x, curTile.TileID.y, face )) {
                    // face is visible - set info in tiles list and fill faces list
                    curFace.bVisible = true;

                    // work out info for left column
                    ColInfo &left         = curFace.leftCol;
                    olc::vf2d leftCoords  = GetColCoordinates( curTile.TileID.x, curTile.TileID.y, face, true );
                    left.fAngleFromPlayer = GetAngle_PlayerToLocation( leftCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    left.fDistFromPlayer  = GetDistance_PlayerToLocation( leftCoords ) * cosf( fPlayerA_rad - left.fAngleFromPlayer );
                    // get projected screen column for left vertical edge of face
                    left.nScreenX         = GetColumnProjection( left.fAngleFromPlayer );

                    // work out info for right column
                    ColInfo &rght         = curFace.rghtCol;
                    olc::vf2d rghtCoords  = GetColCoordinates( curTile.TileID.x, curTile.TileID.y, face, false );
                    rght.fAngleFromPlayer = GetAngle_PlayerToLocation( rghtCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    rght.fDistFromPlayer  = GetDistance_PlayerToLocation( rghtCoords ) * cosf( fPlayerA_rad - rght.fAngleFromPlayer );
                    // get projected screen column for right vertical edge of face
                    rght.nScreenX         = GetColumnProjection( rght.fAngleFromPlayer );

                    // fill faces list with same info
                    vVisibleFaces.push_back( curFace );
                } else {
                    // face is not visible
                    curFace.bVisible = false;
                }
            }
        }

/* NOTE - below sort must be enhanced. Atm it's strictly painters algo, but this should from near to far icw an occlusion list
 *        as is done in doom. I expect this to be superior in performance
 */
        // sort - for test: from largest to smallest distance

        // sort - for test: from smallest to largest distance
        std::sort(
            vVisibleFaces.begin(),
            vVisibleFaces.end(),
            []( FaceInfo &a, FaceInfo &b ) {
                // use mean distance of the two columns as distance for the face
//                return ((a.leftCol.fDistFromPlayer + a.rghtCol.fDistFromPlayer) / 2.0f) >
//                       ((b.leftCol.fDistFromPlayer + b.rghtCol.fDistFromPlayer) / 2.0f);
                return ((a.leftCol.fDistFromPlayer + a.rghtCol.fDistFromPlayer) / 2.0f) <
                       ((b.leftCol.fDistFromPlayer + b.rghtCol.fDistFromPlayer) / 2.0f);
            }
        );
    }

    void RenderPlayerMiniMap( olc::vi2d pos, olc::vi2d tSize ) {
        // draw player as a yellow circle in the map (use size / 4 as radius)
        olc::vf2d playerProj = {
            pos.x + fPlayerX * tSize.x,
            pos.y + fPlayerY * tSize.y
        };
        FillCircle( playerProj.x, playerProj.y, tSize.x / 4, olc::YELLOW );

        // little lambda for drawing direction "finger" from player towards fAngle_rad
        auto draw_finger = [=]( float fAngle_rad, int nMultiplier, olc::Pixel col ) {
            olc::vf2d fingerPoint = {
                pos.x + fPlayerX * tSize.x + cos( fAngle_rad ) * nMultiplier,
                pos.y + fPlayerY * tSize.y + sin( fAngle_rad ) * nMultiplier
            };
            DrawLine( playerProj, fingerPoint, col );
        };

        // draw player direction indicator in yellow
        draw_finger( fPlayerA_rad, 25, olc::YELLOW );
        // draw player FoV indicators in magenta
        draw_finger( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ), 50, olc::MAGENTA );
        draw_finger( Deg2Rad( fPlayerA_deg + fPlayerFoV_deg / 2 ), 50, olc::MAGENTA );
    }

    void RenderMiniMap( olc::vi2d pos, olc::vi2d tSize ) {
        // first lay background for minimap
        FillRect( pos.x - 15, pos.y - 15, tSize.x * nMapX + 17, tSize.y * nMapY + 17, olc::VERY_DARK_GREEN );
        // render tiles in a grid, and put labels around it
        for (int y = 0; y < nMapY; y++) {
            DrawString( pos.x - 15, pos.y + tSize.y / 2 + y * tSize.y, std::to_string( y % 10 ), olc::MAGENTA );
            for (int x = 0; x < nMapX; x++) {
                if (sMap[y * nMapX + x] != '.') {
                    bool bTileVisible = TileInFoV( x, y );
                    olc::Pixel tileCol = bTileVisible ? olc::DARK_CYAN : olc::WHITE;
                    FillRect( pos.x + x * tSize.x, pos.y + y * tSize.y, tSize.x, tSize.y, tileCol );

                    // if the tile is visible, check for visible faces and render these additionally
                    // as a little red line within the cell - therefore offset by 1 pixel
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
                DrawRect( pos.x + x * tSize.x, pos.y + y * tSize.y, tSize.x, tSize.y, olc::DARK_GREY );
            }
        }
        for (int x = 0; x < nMapX; x++) {
            DrawString( pos.x + tSize.x / 2 + x * tSize.x, pos.y - 15, std::to_string( x % 10 ), olc::MAGENTA );
        }

        // output player in map for debugging
        RenderPlayerMiniMap( pos, tSize );
    }

    void RenderPlayerInfo( olc::vi2d pos ) {
        DrawString( pos.x, pos.y +  0, "fPlayerX = " + std::to_string( fPlayerX     ), COL_TEXT );
        DrawString( pos.x, pos.y + 10, "fPlayerY = " + std::to_string( fPlayerY     ), COL_TEXT );
        DrawString( pos.x, pos.y + 20, "fPlayerA = " + std::to_string( fPlayerA_deg ), COL_TEXT );
    }

    void RenderDebugInfo( olc::vi2d pos ) {
        DrawString( pos.x, pos.y +  0, "#tiles vis   = " + std::to_string( vTilesToRender.size() ), COL_TEXT );
        DrawString( pos.x, pos.y + 10, "#faces vis   = " + std::to_string( vFacesToRender.size() ), COL_TEXT );
        DrawString( pos.x, pos.y + 20, "#faces rndrd = " + std::to_string( nFacesRendered        ), COL_TEXT );
        DrawString( pos.x, pos.y + 30, "occList size = " + std::to_string(        occList.size() ), COL_TEXT );
    }

    void RenderRaster( bool bHorizontal, bool bVertical ) {
        if (bVertical) {
            for (int x = 0; x < ScreenWidth(); x += 10) {
                uint32_t linePat = 0x11111111;
                if (x %  50 == 0) linePat = 0x33333333;
                if (x % 100 == 0) linePat = 0xF0F0F0F0;
                DrawLine( x, 0, x, ScreenHeight(), olc::BLACK, linePat );
            }
        }
        if (bHorizontal) {
            for (int y = 0; y < ScreenHeight(); y += 10) {
                uint32_t linePat = 0x11111111;
                if (y %  50 == 0) linePat = 0x33333333;
                if (y % 100 == 0) linePat = 0xF0F0F0F0;
                DrawLine( 0, y, ScreenWidth(), y, olc::BLACK, linePat );
            }
        }
    }

    void RenderWallQuad( FaceInfo &curFace, int nLeftClip, int nRghtClip ) {

        // work out left column points
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
            }
            return olc::Pixel( nRGB, nRGB, nRGB );
        };

        // draw interior of quad by lerping
//        int nRenderStrt = std::max( 0, nLeftClip );
//        int nRenderStop = std::min( ScreenWidth() - 1, nRghtClip );

        int nRenderStrt = std::max( {                 0, curFace.leftCol.nScreenX, nLeftClip } );
        int nRenderStop = std::min( { ScreenWidth() - 1, curFace.rghtCol.nScreenX, nRghtClip } );

//        for (int x = curFace.leftCol.nScreenX; x < curFace.rghtCol.nScreenX; x++) {
        for (int x = nRenderStrt; x <= nRenderStop; x++) {
            float t = float( x - curFace.leftCol.nScreenX ) / float( curFace.rghtCol.nScreenX - curFace.leftCol.nScreenX );
            float y_upper = left_upper.y + (rght_upper.y - left_upper.y) * t;
            float y_lower = left_lower.y + (rght_lower.y - left_lower.y) * t;
//            // don't draw if column is outside of screen
//            if (InBetween( x, nRenderStrt, nRenderStop)) {
                // clamp y values to be within screen boundaries
                y_upper = std::max( float(                  0 ), y_upper );
                y_lower = std::min( float( ScreenHeight() - 1 ), y_lower );

                DrawLine( x, y_upper, x, y_lower, get_face_colour( curFace.nSide ));
//            }
        }
        // draw the quad as a wire frame - offset by 1 pixel to improve visibility
        // NOTE - this line drawing is not clipped!
//        DrawLine( left_upper + olc::vf2d( +1,  0 ), left_lower + olc::vf2d( +1,  0 ), olc::RED   );    // left  column
//        DrawLine( rght_upper + olc::vf2d( -1,  0 ), rght_lower + olc::vf2d( -1,  0 ), olc::GREEN );    // right column
//        DrawLine( left_upper + olc::vf2d(  0, +1 ), rght_upper + olc::vf2d(  0, +1 ), olc::WHITE );    // top   side
//        DrawLine( left_lower + olc::vf2d(  0, -1 ), rght_lower + olc::vf2d(  0, -1 ), olc::BLUE  );    // floor side
    }


    typedef struct sOcclusionRec {
        int left, rght;
    } OcclusionRec;
    typedef std::list<OcclusionRec> OccListType;
    typedef OccListType::iterator   OccIterType;

    OccListType occList;


    void PrintOccList( const std::string &sMsg, OccListType &lst ) {
        std::cout << sMsg << std::endl;
        OccIterType iter = lst.begin();
        for (int i = 0; i < (int)lst.size(); i++) {
            std::cout << "[ ";
            std::cout << ((*iter).left == INT_MIN ? "INT_MIN" : std::to_string( (*iter).left));
            std::cout << ", ";
            std::cout << ((*iter).rght == INT_MAX ? "INT_MAX" : std::to_string( (*iter).rght));
            std::cout << " ], ";
            iter++;
        }
        std::cout << std::endl;
    }

    void PrintOccList2( const std::string &sMsg, OccListType &lst ) {
        std::cout << sMsg << std::endl;
        OccIterType iter = lst.begin();
        for (int i = 0; i < (int)lst.size(); i++) {
            std::cout << "Index: " << i           << " - ";
            if ((*iter).left == INT_MIN) {
                std::cout << "left: "  << "INT_MIN" << ", ";
            } else {
                std::cout << "left: "  << (*iter).left << ", ";
            }
            if ((*iter).rght == INT_MAX) {
                std::cout << "right: "  << "INT_MAX" << ", ";
            } else {
                std::cout << "right: "  << (*iter).rght << ", ";
            }
            std::cout << std::endl;
            iter++;
        }
    }

    void InitOccList( OccListType &lst ) {
        OcclusionRec auxLeft = {       INT_MIN,      -1 };   // non visible intervals on the left and right
        OcclusionRec auxRght = { ScreenWidth(), INT_MAX };   // of the screen

        lst.clear();
        lst.push_front( auxLeft );
        lst.push_back(  auxRght );
    }

    int SizeOccList( OccListType &lst ) { return (int)lst.size(); }

    // returns result if this part should be rendered. If so, nClipLeft and nClipRght denote
    // clipping values
    // PRECONDITION: the list contains at least two elements, and the extreme values for these elements are
    // INT_MIN and INT_MAX
    bool InsertOccList( OccListType &lst, OcclusionRec &rec, int &nClipLeft, int &nClipRght ) {

        if (lst.size() < 2) {
            std::cout << "ERROR: InsertOccList() --> called with too little elements: " << (int)lst.size() << std::endl;
        }

        // maintain two iterators. Insertion should be done between those
        OccIterType iter1 = lst.begin();
        OccIterType iter2 = std::next( iter1 );

        int index1 = 0;
        int index2 = 1;

// WIP HERE


        // 1. search through list until right value of new element is larger than right value of current list element
//        bool bFound = (rec.rght > (*iter1).rght) && ((rec.left <= (*iter1).rght + 1) || (rec.rght <= (*iter2).left));

        // rec must be inserted if 1. it overlaps with left element (on right side), or 2. it overlaps with right element (on left side),
        // or 3. it doesn't overlap, but it's after the left and before the right element
        bool bFound = (
            InBetween( rec.left, (*iter1).left, (*iter1).rght ) ||  // overlap between left element and rec
            InBetween( rec.rght, (*iter2).left, (*iter2).rght ) ||  // overlap between rec and right element
            (
                rec.left > (*iter1).rght + 1 &&     // left element and rec are disjoint (there's a gap between them)
                rec.rght < (*iter2).left - 1        // rec and right element are disjoint
            )
        );

        while (!bFound && iter2 != lst.end()) {
            iter1++; index1++;
            iter2++; index2++;
//            bFound = (rec.rght > (*iter1).rght) && ((rec.left <= (*iter1).rght + 1) || (rec.rght <= (*iter2).left));

            bFound = iter2 != lst.end() && (
                InBetween( rec.left, (*iter1).left, (*iter1).rght ) ||  // overlap rec and left element
                InBetween( rec.rght, (*iter2).left, (*iter2).rght ) ||  // overlap rec and right element
                (
                    rec.left > (*iter1).rght + 1 &&     // rec is disjoint and after left element
                    rec.rght < (*iter2).left - 1        // rec is disjoint and before right element
                )
            );
        }

        // 2a. if location is found, process this element
        if (bFound) {

            if (bTestMode) std::cout << "insertion point located between index " << index1 << " and " << index2;

            // 3. Check if the left element can be extended
            if ((*iter1).rght >= rec.left - 1) {

                if (bTestMode) std::cout << " 3a ";

                // 3a. there is overlap, EXTEND the EXISTING element
                nClipLeft = (*iter1).rght + 1;
                (*iter1).rght = std::max( (*iter1).rght, rec.rght );
            } else {

//                if (bTestMode) std::cout << " 3b ";

                // 3b. there is NO overlap, INSERT the NEW element
                nClipLeft = rec.left;
                iter1 = lst.insert( iter2, rec );

                if (bTestMode) PrintOccList( "Within Insert(), option 3b ", occList );   // debug output occlusion list

//                index1++;
//                index2++;
            }

            // 4. Check if the right element can be merged as well
            if ((*iter1).rght >= (*iter2).left - 1) {
                nClipRght = (*iter2).left - 1;
            } else {
                nClipRght = rec.rght;
            }
            while ((*iter1).rght >= (*iter2).left - 1 && iter2 != lst.end()) {
//                if (bTestMode) std::cout << " 4a ";

                if (bTestMode) PrintOccList( "Within Insert(), option 4a ", occList );   // debug output occlusion list

                // 4a. there is overlap, EXTEND the LEFT element and ERASE the RIGHT element
                (*iter1).left = std::min( (*iter1).left, (*iter2).left );
                (*iter1).rght = std::max( (*iter1).rght, (*iter2).rght );
                iter2 = lst.erase( iter2 );
            }

        } else { // 2b. if location is not found, return false
            nClipLeft = -1;
            nClipRght = -1;
        }

        return nClipLeft < nClipRght;
    }

    bool OnUserUpdate( float fElapsedTime ) override {

        bTestMode = false;

        // step 1 - user input
        // ===================

        // factor to speed up or slow down
        float fSpeedUp = 1.0f;
        if (GetKey( olc::Key::SHIFT ).bHeld) fSpeedUp *= 4.0f;
        if (GetKey( olc::Key::CTRL  ).bHeld) fSpeedUp *= 0.25f;

        auto sync_angle_vars = [&]() {
            // this should be the only place in the code where fPlayerA_deg is altered - keep derived var's sync'd
            fPlayerA_deg = Mod360_deg( fPlayerA_deg );
            fPlayerA_rad = Deg2Rad(    fPlayerA_deg );
            fPlayerSin   = sin(        fPlayerA_rad );
            fPlayerCos   = cos(        fPlayerA_rad );
        };

        // rotate, and keep player angle in [0, 360) range
        if (GetKey( olc::D ).bHeld) { fPlayerA_deg += fSpeedUp * SPEED_ROTATE * fElapsedTime; sync_angle_vars(); }
        if (GetKey( olc::A ).bHeld) { fPlayerA_deg -= fSpeedUp * SPEED_ROTATE * fElapsedTime; sync_angle_vars(); }

        float fNewX = fPlayerX;
        float fNewY = fPlayerY;

        // walk forward - collision detection checked
        if (GetKey( olc::W ).bHeld) {
            fNewX += fPlayerCos * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fNewY += fPlayerSin * fSpeedUp * SPEED_MOVE * fElapsedTime;
        }
        // walk backwards - collision detection checked
        if (GetKey( olc::S ).bHeld) {
            fNewX -= fPlayerCos * fSpeedUp * SPEED_MOVE * fElapsedTime;
            fNewY -= fPlayerSin * fSpeedUp * SPEED_MOVE * fElapsedTime;
        }
        // strafe left - collision detection checked
        if (GetKey( olc::Q ).bHeld) {
            fNewX += fPlayerSin * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fNewY -= fPlayerCos * fSpeedUp * SPEED_STRAFE * fElapsedTime;
        }
        // strafe right - collision detection checked
        if (GetKey( olc::E ).bHeld) {
            fNewX -= fPlayerSin * fSpeedUp * SPEED_STRAFE * fElapsedTime;
            fNewY += fPlayerCos * fSpeedUp * SPEED_STRAFE * fElapsedTime;
        }
        // collision detection - check if out of bounds or inside occupied tile
        if (fNewX >= 0 && fNewX < nMapX &&
            fNewY >= 0 && fNewY < nMapY &&
            sMap[ int( fNewY ) * nMapX + int( fNewX ) ] != '#') {
            fPlayerX = fNewX;
            fPlayerY = fNewY;
        }
        // toggle raster flags
        if (GetKey( olc::Key::V ).bPressed) bVerRasterMode = !bVerRasterMode;
        if (GetKey( olc::Key::H ).bPressed) bHorRasterMode = !bHorRasterMode;

        // step 2 - game logic
        // ===================

        vTilesToRender.clear();
        GetVisibleTiles( vTilesToRender );

        vFacesToRender.clear();
        GetVisibleFaces( vTilesToRender, vFacesToRender );

        // test output
        if (GetKey( olc::Key::T ).bPressed) {
//            PrintTilesList( vTilesToRender );
//            PrintFacesList( vFacesToRender );
            bTestMode = true;
        }

        // step 3 - render
        // ===============

        // Instead of clearing the screen, draw synthetic sky and floor
        int nHorizon = ScreenHeight() / 2;
        FillRect( 0,            0, ScreenWidth() - 1,       nHorizon, COL_CEIL  );
        FillRect( 0, nHorizon + 1, ScreenWidth() - 1, ScreenHeight(), COL_FLOOR );

        // iterate over visible faces list
        InitOccList( occList );

        if (bTestMode) PrintOccList( "After Init(): ", occList );   // debug output occlusion list

        bool bScreenFilled = false;
        nFacesRendered = 0;
        for (int i = 0; i < (int)vFacesToRender.size() && !bScreenFilled; i++) {
            FaceInfo &curFace = vFacesToRender[i];

            OcclusionRec occRec = { curFace.leftCol.nScreenX, curFace.rghtCol.nScreenX };

            if (bTestMode) std::cout << "Attempt Insert() on: (" << curFace.leftCol.nScreenX << ", " << curFace.rghtCol.nScreenX << ")" << std::endl;

            int nClipLt, nClipRt;
            bool bInsertResult = InsertOccList( occList, occRec, nClipLt, nClipRt );
            if (bInsertResult) {

                RenderWallQuad( curFace, nClipLt, nClipRt );
                nFacesRendered += 1;
            }

            if (bTestMode) {
                PrintOccList( "After Insert(): ", occList );   // debug output occlusion list
                std::cout << "clip LEFT: " << nClipLt << ", clip RIGHT: " << nClipRt << " - result: " << (bInsertResult ? "TRUE" : "FALSE") << std::endl;
                std::cout << "==========" << std::endl;
            }

            bScreenFilled = (SizeOccList( occList ) == 1);
        }

        // output raster
        RenderRaster( bHorRasterMode, bVerRasterMode );

        // output map for debugging
        olc::vi2d cellSize = { 12, 12 };
        olc::vi2d position = { 20, 20 };
        RenderMiniMap( position, cellSize );

        // output player values for debugging
        RenderPlayerInfo( { 10, ScreenHeight() - 40 } );
        // output render characteristics
        RenderDebugInfo( { ScreenWidth() - 200, 10 } );

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
