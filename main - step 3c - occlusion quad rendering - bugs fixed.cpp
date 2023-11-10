// Alternative ray caster using sprite warped rendering
//
// Joseph21, november 8, 2023
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)
//

/* Short description
   -----------------
   bla

   To do
   -----

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
#define COL_CEIL    olc::BLUE
#define COL_FLOOR   olc::DARK_RED
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
    // player: height of eye point and field of view
    float fPlayerH       = 0.5f;
    float fPlayerFoV_deg = 60.0f;   // in degrees !!

    // for interior working
    float fPlayerA_rad = 0.0f;    // is kept synchronized with changes in fPlayerA_deg
    float fPlayerSin   = 0.0f;
    float fPlayerCos   = 0.0f;

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

    // tile descriptor - a tile has coordinates in the map, and consists of four faces
    typedef struct sTileDescriptor {
        olc::vi2d TileID;          // coords of the tile
        FaceInfo faces[4];
    } TileInfo;

    std::vector<TileInfo> vTilesToRender;
    std::vector<FaceInfo> vFacesToRender;

    // test output functions for ColInfo, FaceInfo, TileInfo and associated Tile and Face lists
    // ========================================================================================

    void PrintColInfo( ColInfo &c ) {
        std::cout << "screen col: "  << c.nScreenX         << ", ";
        std::cout << "angle frm P: " << c.fAngleFromPlayer << ", ";
        std::cout << "dist frm P: "  << c.fDistFromPlayer;
    }

    std::string Face2String( int nFace ) {
        switch (nFace) {
            case UNKNWN: return "_HUH_";
            case EAST  : return "EAST ";
            case SOUTH : return "SOUTH";
            case WEST  : return "WEST ";
            case NORTH : return "NORTH";
            default    : return " --- ERROR --- ";
        }
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

    // Generic convenience functions
    // =============================

    // angle conversion
    float Deg2Rad( float fDegAngle ) { return fDegAngle / 180.0f * PI; }
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

    // returns true if the tile at (nTileX, nTileY) is within the field of view of the player
    // also if the tile is only partially within the FoV
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
            olc::vf2d colPoint = GetColCoordinates( nTileX, nTileY, f, true );
            // determine angle from player to tile center
            float fAngleToPoint_rad = GetAngle_PlayerToLocation( colPoint );
            // check whether the angle is with the FoV cone
            bResult = AngleInSector( fAngleToPoint_rad, fLeftAngleBoundary, fRghtAngleBoundary );
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
            // faces are not visible from outside map boundaries, or if there's another non empty cell in front
            // furthermore the look direction and position of the player must be correct
            case EAST : return nTileX < nMapX - 1 && sMap[  nTileY      * nMapX + (nTileX + 1) ] != '#' && bLt && (fPlayerX > float( nTileX + 1 ));
            case WEST : return nTileX >         0 && sMap[  nTileY      * nMapX + (nTileX - 1) ] != '#' && bRt && (fPlayerX < float( nTileX     ));
            case SOUTH: return nTileY < nMapY - 1 && sMap[ (nTileY + 1) * nMapX +  nTileX      ] != '#' && bUp && (fPlayerY > float( nTileY + 1 ));
            case NORTH: return nTileY >         0 && sMap[ (nTileY - 1) * nMapX +  nTileX      ] != '#' && bDn && (fPlayerY < float( nTileY     ));
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

    // precondition - input parameter is in [0, 2 PI)
    // calculate angle from player as a % of the players FOV, and multiply by screen width
    // to get the projected column
    int GetColumnProjection( float fAngleFromPlayer_rad ) {
        // This function took me quite some time to get right. See the separate test program specifically made
        // for testing and tuning this function

        // determine angle of left boundary of FOV cone
        float fFOVRay0Angle_rad = Mod2Pi_rad( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ));
        // determine view angle associated with fAngleFromPlayer
        float fViewAngle_rad;
        // check if FoV cone spans over the 0/360 angle, then it could be the case that left boundary angle is
        // larger than angle from player to screen column, so check on that too
        if (fFOVRay0Angle_rad > fAngleFromPlayer_rad) {
            fViewAngle_rad = fAngleFromPlayer_rad + 2.0f * PI - fFOVRay0Angle_rad;
        } else {
            fViewAngle_rad = fAngleFromPlayer_rad             - fFOVRay0Angle_rad;
        }
        // make sure the angle sign is flipped to keep the coordinates projected on the right side of the screen
        // if the angle is more than 180 degrees beyond player angle, interpret as a negative angle
        float fSector1 = PI + Deg2Rad( fPlayerFoV_deg / 2.0f );
        float fSector2 = 2.0f * PI;
        if (InBetween( fViewAngle_rad, fSector1, fSector2 )) {
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
                curFace.nSide  = face;
                if (FaceVisible( curTile.TileID.x, curTile.TileID.y, face )) {
                    // face is visible - set info in tiles list and fill faces list
                    curFace.bVisible = true;

                    // work out info for left column
                    ColInfo &left         = curFace.leftCol;
                    olc::vf2d leftCoords  = GetColCoordinates( curTile.TileID.x, curTile.TileID.y, face, true );
                    left.fAngleFromPlayer = GetAngle_PlayerToLocation( leftCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    left.fDistFromPlayer  = GetDistance_PlayerToLocation( leftCoords ) * abs( cosf( fPlayerA_rad - left.fAngleFromPlayer ));
                    // get projected screen column for left vertical edge of face
                    left.nScreenX         = GetColumnProjection( left.fAngleFromPlayer );

                    // work out info for right column
                    ColInfo &rght         = curFace.rghtCol;
                    olc::vf2d rghtCoords  = GetColCoordinates( curTile.TileID.x, curTile.TileID.y, face, false );
                    rght.fAngleFromPlayer = GetAngle_PlayerToLocation( rghtCoords );
                    // correct distance for fish eye by applying cos() on the angle view angle from the player
                    rght.fDistFromPlayer  = GetDistance_PlayerToLocation( rghtCoords ) * abs( cosf( fPlayerA_rad - rght.fAngleFromPlayer ));
                    // get projected screen column for right vertical edge of face
                    rght.nScreenX         = GetColumnProjection( rght.fAngleFromPlayer );

                    // extra check on the resulted projections
                    if (left.nScreenX > rght.nScreenX) {
                        std::cout << "WARNING: GetVisibleFaces() --> projection seems invalid (left = " << left.nScreenX << ", right = " << rght.nScreenX << ") for face: " << std::endl;
                        PrintFace( curFace );
                        std::cout << std::endl;
                    }

                    // fill faces list with same info
                    vVisibleFaces.push_back( curFace );
                } else {
                    // face is not visible
                    curFace.bVisible = false;
                }
            }
        }

        // sort - from smallest to largest distance
        std::sort(
            vVisibleFaces.begin(),
            vVisibleFaces.end(),
            []( FaceInfo &a, FaceInfo &b ) {

                // I wonder - would taking the smallest of the distance columns yield better results?

                // use mean distance of the two columns as distance for the face
                return ((a.leftCol.fDistFromPlayer + a.rghtCol.fDistFromPlayer) / 2.0f) <
                       ((b.leftCol.fDistFromPlayer + b.rghtCol.fDistFromPlayer) / 2.0f);
            }
        );
    }

    // Convenience rendering functions
    // ===============================

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
            }
            return olc::Pixel( nRGB, nRGB, nRGB );
        };

        // clip horizontal rendering both by screen boundaries and clip coordinates
        int nRenderStrt = std::max( {                 0, curFace.leftCol.nScreenX, nLeftClip } );
        int nRenderStop = std::min( { ScreenWidth() - 1, curFace.rghtCol.nScreenX, nRghtClip } );

        // variables for clipped wire frame points
        olc::vf2d wf_ul, wf_ur, wf_ll, wf_lr;  // upper left/right & lower left/right

        // draw interior of quad by lerping
        for (int x = nRenderStrt; x <= nRenderStop; x++) {
            float t = float( x - curFace.leftCol.nScreenX ) / float( curFace.rghtCol.nScreenX - curFace.leftCol.nScreenX );
            float y_upper = left_upper.y + (rght_upper.y - left_upper.y) * t;
            float y_lower = left_lower.y + (rght_lower.y - left_lower.y) * t;

            // save clipped border points for wire frame drawing
            // (note that y coordinate is not clipped)
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

            DrawLine( x, y_upper, x, y_lower, get_face_colour( curFace.nSide ));
        }
        // draw the quad as a wire frame - offset by 1 pixel to improve visibility
        // NOTE - this line drawing is clipped, so doesn't necessarily reflect the original shape!
        DrawLine( wf_ul + olc::vf2d( +1,  0 ), wf_ll + olc::vf2d( +1,  0 ), olc::RED   );    // left  column
        DrawLine( wf_ur + olc::vf2d( -1,  0 ), wf_lr + olc::vf2d( -1,  0 ), olc::GREEN );    // right column
        DrawLine( wf_ul + olc::vf2d(  0, +1 ), wf_ur + olc::vf2d(  0, +1 ), olc::WHITE );    // top   side
        DrawLine( wf_ll + olc::vf2d(  0, -1 ), wf_lr + olc::vf2d(  0, -1 ), olc::BLUE  );    // floor side
    }


    // Occlusion list stuff
    // ====================

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
            std::cout << "[ " << ((*iter).left == INT_MIN ? "INT_MIN" : std::to_string( (*iter).left));
            std::cout << ", " << ((*iter).rght == INT_MAX ? "INT_MAX" : std::to_string( (*iter).rght));
            std::cout << " ], ";
            iter++;
        }
        std::cout << std::endl;
    }

    // Initialises the occlusion list with the two extreme elements left and right outside screen boundaries
    void InitOccList( OccListType &lst ) {
        OcclusionRec auxLeft = {       INT_MIN,      -1 };   // non visible intervals on the left and right
        OcclusionRec auxRght = { ScreenWidth(), INT_MAX };   // of the screen

        lst.clear();
        lst.push_front( auxLeft );
        lst.push_back(  auxRght );
    }

    int SizeOccList( OccListType &lst ) { return (int)lst.size(); }

    // returns true if this occlusion rec should be (partially) rendered. If so, nClipLeft and nClipRght denote
    // clipping values
    // PRECONDITION: the list contains at least two elements, and the extreme values for these elements are
    // INT_MIN and INT_MAX
    bool InsertOccList( OccListType &lst, OcclusionRec &rec, int &nClipLeft, int &nClipRght ) {

        if (lst.size() < 2) {
            std::cout << "ERROR: InsertOccList() --> called with too little elements: " << (int)lst.size() << std::endl;
            return false;
        }

        // maintain two iterators. Insertion should be done between those
        OccIterType iterLeft = lst.begin();
        OccIterType iterRght = std::next( iterLeft );

        // 1. search through list until insertion point is found
        // -----------------------------------------------------

        // insertion point is found iff
        //   A. left  part of rec overlaps with right side of left  element, or
        //   B. right part of rec overlaps with left  side of right element, or
        //   C. rec doesn't overlap with neither left nor right, but its values are in between left and right
        //      i.e. left.rightval < rec.leftval and rec.rightval < right.leftval
        bool bFound = (
            InBetween( rec.left, (*iterLeft).left, (*iterLeft).rght ) ||  // overlap between left element and rec
            InBetween( rec.rght, (*iterRght).left, (*iterRght).rght ) ||  // overlap between rec and right element
            (
                rec.left > (*iterLeft).rght &&     // left and rec may be adjacent, but there's no overlap
                rec.rght < (*iterRght).left        // same for rec and rght element
            )
        );

        while (!bFound && iterRght != lst.end()) {
            iterLeft++;
            iterRght++;

            bFound = iterRght != lst.end() && (
                InBetween( rec.left, (*iterLeft).left, (*iterLeft).rght ) ||  // overlap between left element and rec
                InBetween( rec.rght, (*iterRght).left, (*iterRght).rght ) ||  // overlap between rec and right element
                (
                    rec.left > (*iterLeft).rght &&     // left and rec may be adjacent, but there's no overlap
                    rec.rght < (*iterRght).left        // same for rec and rght element
                )
            );
        }

        if (!bFound) {
            // 2a. if location is not found, return false
            // ------------------------------------------
            nClipLeft = nClipRght = -1;

        } else {
            // 2b. if location is found, process this element
            // ----------------------------------------------

            // we know that this is the right place to insert rec, but there could be overlap
            // (or adjacency) both on the left side and on the right side

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

            // Intermezzo - Either a new records is inserted and iterLeft points to that new record, or
            //              the existing record is adapted to absorb rec and iterLeft points to the
            //              existing record.
            //              Either way, iterLeft points to the record of which the right value must possibly
            //              be merged with the element pointed to by iterRght

            // 4. Check if a merge with one or more elements to the right is needed
            if ((*iterLeft).rght + 1 >= (*iterRght).left) {
                nClipRght = (*iterRght).left - 1;
            } else {
                nClipRght = rec.rght;
            }
            while ((*iterLeft).rght + 1 >= (*iterRght).left && iterRght != lst.end()) {

                // 4a. there is overlap (or adjacency): EXTEND the LEFT element, ...
                (*iterLeft).left = std::min( (*iterLeft).left, (*iterRght).left );
                (*iterLeft).rght = std::max( (*iterLeft).rght, (*iterRght).rght );
                // ..., ERASE the RIGHT element and set iterRght to the element after it
                iterRght = lst.erase( iterRght );
            }
        }

        return nClipLeft < nClipRght;
    }

    bool OnUserUpdate( float fElapsedTime ) override {

        bTestMode = false;

        // step 1 - user input
        // ===================

        // factor to speed up or slow down
        float fSpeedUp = 1.0f;
        if (GetKey( olc::Key::SHIFT ).bHeld) fSpeedUp *= 4.00f;
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

        float fCDMargin = 0.25f;   // collision detection margin (in cells)
        float fCheckX, fCheckY;

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

        // step 2 - game logic
        // ===================

        vTilesToRender.clear();
        GetVisibleTiles( vTilesToRender );

        vFacesToRender.clear();
        GetVisibleFaces( vTilesToRender, vFacesToRender );

        // test output
        if (GetKey( olc::Key::T ).bPressed) { bTestMode = true; }
        if (bTestMode) {
            PrintTilesList( vTilesToRender );
            PrintFacesList( vFacesToRender );
        }


        // step 3 - render
        // ===============

        // Instead of clearing the screen, draw synthetic sky and floor
        int nHorizon = ScreenHeight() / 2;
        FillRect( 0,            0, ScreenWidth() - 1,       nHorizon, COL_CEIL  );
        FillRect( 0, nHorizon + 1, ScreenWidth() - 1, ScreenHeight(), COL_FLOOR );

        // iterate over visible faces list - use the occlusion list approach to determine whether
        // faces are (partly) occluded, and draw them as quads
        InitOccList( occList );
        nFacesRendered = 0;
        for (int i = 0; i < (int)vFacesToRender.size() && (int)SizeOccList( occList ) > 1; i++) {
            FaceInfo &curFace = vFacesToRender[i];

            OcclusionRec occRec = { curFace.leftCol.nScreenX, curFace.rghtCol.nScreenX };
            int nClipLt, nClipRt;
            bool bInsertResult = InsertOccList( occList, occRec, nClipLt, nClipRt );
            if (bInsertResult) {

                RenderWallQuad( curFace, nClipLt, nClipRt );
                nFacesRendered += 1;
            }
        }

        // output raster
        RenderRaster( bHorRasterMode, bVerRasterMode );

        // output map for debugging
        olc::vi2d cellSize = { 20, 20 };
        olc::vi2d position = {
            10,
            ScreenHeight() - cellSize.y * nMapY - 10 };
        RenderMiniMap( position, cellSize );

        // output player values for debugging
        RenderPlayerInfo( { 10, 10 } );
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
