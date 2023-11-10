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
#define PIXEL_X        2
#define PIXEL_Y        2

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


    // player: position and looking angle
    float fPlayerX     = 2.0f;
    float fPlayerY     = 2.0f;
    float fPlayerA_deg = 0.0f;    // looking angle is in degrees
    // player: height of eye point and field of view
    float fPlayerH       = 0.5f;
    float fPlayerFoV_deg = 60.0f;   // in degrees !!

    olc::vf2d Object = { 200.0f, 200.0f };

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
    // each face has a left and right column, as viewd from the outside of the face
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


public:
    bool OnUserCreate() override {


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

    // function to check if fLeftA <= fA <= fRghtA (mod 2 PI)
    bool AngleInSector( float fA, float fLeftA, float fRghtA ) {
        // check if FoV cone spans 360/0 transition angle
        if (fLeftA > fRghtA) {
            return InBetween( fA, fLeftA, 2.0f * PI ) ||
                   InBetween( fA,   0.0f, fRghtA );
        }
        return InBetween( fA, fLeftA, fRghtA );
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


    // calculate angle from player as a % of the players FOV, and multiply by screen width
    // to get the projected column
    int GetColumnProjection( float fAngleFromPlayer_rad ) {

        DrawString( 10 + 250, 10, "fAngleFromPlayer_rad = " + std::to_string( fAngleFromPlayer_rad ) + " (deg = " + std::to_string( Rad2Deg( fAngleFromPlayer_rad )) + ")", olc::CYAN );
        DrawString( 10 + 250, 20, "fPlayerA_deg         = " + std::to_string( fPlayerA_deg         ), olc::CYAN );
        DrawString( 10 + 250, 30, "fPlayerFoV_deg       = " + std::to_string( fPlayerFoV_deg       ), olc::CYAN );


        // determine left most boundary of FOV cone
        float fFOVRay0Angle_rad = Mod2Pi_rad( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ));

        DrawString( 10 + 250, 40, "fFOVRay0Angle_rad    = " + std::to_string( fFOVRay0Angle_rad ) + " (deg = " + std::to_string( Rad2Deg( fFOVRay0Angle_rad )) + ")", olc::CYAN );

        // determine view angle associated with fAngleFromPlayer
        float fViewAngle_rad;
        // check if FoV cone spans over the 0/360 angle, then it could be the case that left boundary angle is
        // larger than angle from player to screen column, so check on that too
        if (fFOVRay0Angle_rad > fAngleFromPlayer_rad) {
            fViewAngle_rad = fAngleFromPlayer_rad + 2.0f * PI - fFOVRay0Angle_rad;
        } else {
            fViewAngle_rad = fAngleFromPlayer_rad             - fFOVRay0Angle_rad;
        }
        // make sure the coordinates stay on the right side
        float fSector1 = PI + Deg2Rad( fPlayerFoV_deg / 2.0f );
        float fSector2 = 2.0f * PI;

        if (InBetween( fViewAngle_rad, fSector1, fSector2 )) {
            fViewAngle_rad = fViewAngle_rad - 2.0f * PI;
        }

        DrawString( 10 + 250, 50, "fViewAngle_rad     = " + std::to_string( fViewAngle_rad ) + " (deg = " + std::to_string( Rad2Deg( fViewAngle_rad )) + ")", olc::CYAN );

        // use view angle to work out percentage across FoV
        float fFoVPerc = fViewAngle_rad / Deg2Rad( fPlayerFoV_deg );

        DrawString( 10 + 250, 60, "fFoVPerc           = " + std::to_string( fFoVPerc       ), olc::CYAN );

        // multiply by screen width to get screen column
        return int( fFoVPerc * float( ScreenWidth() ));
    }


    void RenderPlayer( olc::vi2d pos, olc::vi2d tSize ) {
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
            return true;
        };
        // draw player direction indicator in yellow
        draw_finger( Deg2Rad( fPlayerA_deg ), 150, olc::YELLOW );
        // draw player FoV indicators in magenta
        draw_finger( Deg2Rad( fPlayerA_deg - fPlayerFoV_deg / 2 ), 100, olc::MAGENTA );
        draw_finger( Deg2Rad( fPlayerA_deg + fPlayerFoV_deg / 2 ), 100, olc::MAGENTA );

        // Also draw a "finger" in the direction of 0 degrees
        draw_finger( 0.0f, 200, olc::WHITE );

        // also draw a connection line between player and object
        DrawLine( playerProj, Object + pos, olc::CYAN );
    }

    void RenderDebugInfo( olc::vi2d pos, olc::vi2d tSize ) {



        DrawString( 10, 10 +  0, "fPlayerX       = " + std::to_string( fPlayerX     ), COL_TEXT );
        DrawString( 10, 10 + 10, "fPlayerY       = " + std::to_string( fPlayerY     ), COL_TEXT );
        DrawString( 10, 10 + 20, "fPlayerA (deg) = " + std::to_string( fPlayerA_deg ), COL_TEXT );


        float fAngle_rad = GetAngle_PlayerToLocation( Object );
        float fAngle_deg = Rad2Deg( fAngle_rad );
        int nScreenX = GetColumnProjection( fAngle_rad );

        DrawString( 10, 10 + 40, "Object X          = " + std::to_string( Object.x   ), COL_TEXT );
        DrawString( 10, 10 + 50, "Object Y          = " + std::to_string( Object.y   ), COL_TEXT );
        DrawString( 10, 10 + 60, "A to object (deg) = " + std::to_string( fAngle_deg ), COL_TEXT );
        DrawString( 10, 10 + 70, "on screen column  = " + std::to_string( nScreenX   ), COL_TEXT );

    }

    bool OnUserUpdate( float fElapsedTime ) override {

        // step 1 - user input
        // ===================

        // factor to speed up or slow down
        float fSpeedUp = 1.0f;
        if (GetKey( olc::Key::SHIFT ).bHeld) fSpeedUp *= 4.0f;
        if (GetKey( olc::Key::CTRL  ).bHeld) fSpeedUp *= 0.25f;

        // rotate, and keep player angle in [0, 360) range
        if (GetKey( olc::D ).bHeld) { fPlayerA_deg += fSpeedUp * SPEED_ROTATE * fElapsedTime; }
        if (GetKey( olc::A ).bHeld) { fPlayerA_deg -= fSpeedUp * SPEED_ROTATE * fElapsedTime; }
        fPlayerA_deg = Mod360_deg( fPlayerA_deg );

        // adapt FoV
        if (GetKey( olc::Key::NP_ADD ).bPressed) fPlayerFoV_deg += 1;
        if (GetKey( olc::Key::NP_SUB ).bPressed) fPlayerFoV_deg -= 1;

        olc::vi2d RenderOrg = { 200, 200 };   // position of "virtual" map on the screen
        olc::vi2d TileSize  = {  1,  1 };

        // reposition object
        if (GetMouse( 0 ).bPressed) Object = GetMousePos() - RenderOrg;

        // step 2 - game logic
        // ===================


        // step 3 - render
        // ===============

        Clear( olc::BLACK );


        // draw the player
        RenderPlayer( RenderOrg, TileSize );

        // draw the object
        FillCircle( Object + RenderOrg, 6, olc::RED );

        // provide the info on screen
        RenderDebugInfo( RenderOrg, TileSize );

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
