#define PALETTE_SIZE 256

/* Note that the default MagicaVoxel format is AA GG BB RR, so something will
 * have to be done if we continue to load this format.  I manually rearranged
 * the columns in this lookup table to match what OpenGL expects: RR GG BB AA
 */

#define BLACK    0
#define WHITE    1
#define RED      3
#define DARK_RED 144
#define DARK_DARK_RED 180
#define GREEN  186
#define LIGHT_GREEN  75
#define GRASS_GREEN  2
#define BLUE   4
#define TEAL   6
#define YELLOW  5
#define PINK    7
#define ORANGE  8
#define GREY   6

//  RR GG BB AA
static v4 DefaultPalette[] = {
  V4(0x00, 0x00, 0x00, 0x00), // BLACK
  V4(0xff, 0xff, 0xff, 0xff), // WHITE
  V4(0x00, 0x7B, 0x0C, 0xff), // GRASS_GREEN
  V4(0xff, 0x00, 0x00, 0xff), // RED
  V4(0x00, 0x00, 0x66, 0xff), // BLUE
  V4(0xff, 0xcc, 0xff, 0xff), // YELLOW
  V4(0x00, 0x80, 0x80, 0xff), // TEAL
  V4(0xff, 0x33, 0x99, 0xff), // PINK
  V4(0xff, 0x99, 0x00, 0xff), // ORANGE
  V4(0x77, 0x77, 0x77, 0xff), // GREY
  V4(0xff, 0xff, 0x99, 0xff),
  V4(0xff, 0xff, 0x66, 0xff),
  V4(0xff, 0xff, 0x33, 0xff),
  V4(0xff, 0xff, 0x00, 0xff),
  V4(0xff, 0xcc, 0xcc, 0xff),
  V4(0xff, 0xcc, 0x99, 0xff),
  V4(0xff, 0xcc, 0x66, 0xff),
  V4(0xff, 0xcc, 0x33, 0xff),
  V4(0xff, 0xcc, 0x00, 0xff),
  V4(0xff, 0x99, 0xff, 0xff),
  V4(0xff, 0x99, 0xcc, 0xff),
  V4(0xff, 0x99, 0x99, 0xff),
  V4(0xff, 0x99, 0x66, 0xff),
  V4(0xff, 0x99, 0x33, 0xff),
  V4(0xff, 0x66, 0xff, 0xff),
  V4(0xff, 0x66, 0xcc, 0xff),
  V4(0xff, 0x66, 0x99, 0xff),
  V4(0xff, 0x66, 0x66, 0xff),
  V4(0xff, 0x66, 0x33, 0xff),
  V4(0xff, 0x66, 0x00, 0xff),
  V4(0xff, 0x33, 0xff, 0xff),
  V4(0xff, 0x33, 0xcc, 0xff),
  V4(0xff, 0x33, 0x66, 0xff),
  V4(0xff, 0x33, 0x33, 0xff),
  V4(0xff, 0x33, 0x00, 0xff),
  V4(0xff, 0x00, 0xff, 0xff),
  V4(0xff, 0x00, 0xcc, 0xff),
  V4(0xff, 0x00, 0x99, 0xff),
  V4(0xff, 0x00, 0x66, 0xff),
  V4(0xff, 0x00, 0x33, 0xff),
  V4(0xcc, 0xff, 0xff, 0xff),
  V4(0xcc, 0xff, 0xcc, 0xff),
  V4(0xcc, 0xff, 0x99, 0xff),
  V4(0xcc, 0xff, 0x66, 0xff),
  V4(0xcc, 0xff, 0x33, 0xff),
  V4(0xcc, 0xff, 0x00, 0xff),
  V4(0xcc, 0xcc, 0xff, 0xff),
  V4(0xcc, 0xcc, 0xcc, 0xff),
  V4(0xcc, 0xcc, 0x99, 0xff),
  V4(0xcc, 0xcc, 0x66, 0xff),
  V4(0xcc, 0xcc, 0x33, 0xff),
  V4(0xcc, 0xcc, 0x00, 0xff),
  V4(0xcc, 0x99, 0xff, 0xff),
  V4(0xcc, 0x99, 0xcc, 0xff),
  V4(0xcc, 0x99, 0x99, 0xff),
  V4(0xcc, 0x99, 0x66, 0xff),
  V4(0xcc, 0x99, 0x33, 0xff),
  V4(0xcc, 0x99, 0x00, 0xff),
  V4(0xcc, 0x66, 0xff, 0xff),
  V4(0xcc, 0x66, 0xcc, 0xff),
  V4(0xcc, 0x66, 0x99, 0xff),
  V4(0xcc, 0x66, 0x66, 0xff),
  V4(0xcc, 0x66, 0x33, 0xff),
  V4(0xcc, 0x66, 0x00, 0xff),
  V4(0xcc, 0x33, 0xff, 0xff),
  V4(0xcc, 0x33, 0xcc, 0xff),
  V4(0xcc, 0x33, 0x99, 0xff),
  V4(0xcc, 0x33, 0x66, 0xff),
  V4(0xcc, 0x33, 0x33, 0xff),
  V4(0xcc, 0x33, 0x00, 0xff),
  V4(0xcc, 0x00, 0xff, 0xff),
  V4(0xcc, 0x00, 0xcc, 0xff),
  V4(0xcc, 0x00, 0x99, 0xff),
  V4(0xcc, 0x00, 0x66, 0xff),
  V4(0xcc, 0x00, 0x33, 0xff),
  V4(0xcc, 0x00, 0x00, 0xff),
  V4(0x99, 0xff, 0xff, 0xff),
  V4(0x99, 0xff, 0xcc, 0xff),
  V4(0x99, 0xff, 0x99, 0xff), // LIGHTGREEN
  V4(0x99, 0xff, 0x66, 0xff),
  V4(0x99, 0xff, 0x33, 0xff),
  V4(0x99, 0xff, 0x00, 0xff),
  V4(0x99, 0xcc, 0xff, 0xff),
  V4(0x99, 0xcc, 0xcc, 0xff),
  V4(0x99, 0xcc, 0x99, 0xff),
  V4(0x99, 0xcc, 0x66, 0xff),
  V4(0x99, 0xcc, 0x33, 0xff),
  V4(0x99, 0xcc, 0x00, 0xff),
  V4(0x99, 0x99, 0xff, 0xff),
  V4(0x99, 0x99, 0xcc, 0xff),
  V4(0x99, 0x99, 0x99, 0xff),
  V4(0x99, 0x99, 0x66, 0xff),
  V4(0x99, 0x99, 0x33, 0xff),
  V4(0x99, 0x99, 0x00, 0xff),
  V4(0x99, 0x66, 0xff, 0xff),
  V4(0x99, 0x66, 0xcc, 0xff),
  V4(0x99, 0x66, 0x99, 0xff),
  V4(0x99, 0x66, 0x66, 0xff),
  V4(0x99, 0x66, 0x33, 0xff),
  V4(0x99, 0x66, 0x00, 0xff),
  V4(0x99, 0x33, 0xff, 0xff),
  V4(0x99, 0x33, 0xcc, 0xff),
  V4(0x99, 0x33, 0x99, 0xff),
  V4(0x99, 0x33, 0x66, 0xff),
  V4(0x99, 0x33, 0x33, 0xff),
  V4(0x99, 0x33, 0x00, 0xff),
  V4(0x99, 0x00, 0xff, 0xff),
  V4(0x99, 0x00, 0xcc, 0xff),
  V4(0x99, 0x00, 0x99, 0xff),
  V4(0x99, 0x00, 0x66, 0xff),
  V4(0x99, 0x00, 0x33, 0xff),
  V4(0x99, 0x00, 0x00, 0xff),
  V4(0x66, 0xff, 0xff, 0xff),
  V4(0x66, 0xff, 0xcc, 0xff),
  V4(0x66, 0xff, 0x99, 0xff),
  V4(0x66, 0xff, 0x66, 0xff),
  V4(0x66, 0xff, 0x33, 0xff),
  V4(0x66, 0xff, 0x00, 0xff),
  V4(0x66, 0xcc, 0xff, 0xff),
  V4(0x66, 0xcc, 0xcc, 0xff),
  V4(0x66, 0xcc, 0x99, 0xff),
  V4(0x66, 0xcc, 0x66, 0xff),
  V4(0x66, 0xcc, 0x33, 0xff),
  V4(0x66, 0xcc, 0x00, 0xff),
  V4(0x66, 0x99, 0xff, 0xff),
  V4(0x66, 0x99, 0xcc, 0xff),
  V4(0x66, 0x99, 0x99, 0xff),
  V4(0x66, 0x99, 0x66, 0xff),
  V4(0x66, 0x99, 0x33, 0xff),
  V4(0x66, 0x99, 0x00, 0xff),
  V4(0x66, 0x66, 0xff, 0xff),
  V4(0x66, 0x66, 0xcc, 0xff),
  V4(0x66, 0x66, 0x99, 0xff),
  V4(0x66, 0x66, 0x66, 0xff),
  V4(0x66, 0x66, 0x33, 0xff),
  V4(0x66, 0x66, 0x00, 0xff),
  V4(0x66, 0x33, 0xff, 0xff),
  V4(0x66, 0x33, 0xcc, 0xff),
  V4(0x66, 0x33, 0x99, 0xff),
  V4(0x66, 0x33, 0x66, 0xff),
  V4(0x66, 0x33, 0x33, 0xff),
  V4(0x66, 0x33, 0x00, 0xff),
  V4(0x66, 0x00, 0xff, 0xff),
  V4(0x66, 0x00, 0xcc, 0xff),
  V4(0x66, 0x00, 0x99, 0xff),
  V4(0x66, 0x00, 0x66, 0xff),
  V4(0x66, 0x00, 0x33, 0xff),
  V4(0x66, 0x00, 0x00, 0xff),
  V4(0x33, 0xff, 0xff, 0xff),
  V4(0x33, 0xff, 0xcc, 0xff),
  V4(0x33, 0xff, 0x99, 0xff),
  V4(0x33, 0xff, 0x66, 0xff),
  V4(0x33, 0xff, 0x33, 0xff),
  V4(0x33, 0xff, 0x00, 0xff),
  V4(0x33, 0xcc, 0xff, 0xff),
  V4(0x33, 0xcc, 0xcc, 0xff),
  V4(0x33, 0xcc, 0x99, 0xff),
  V4(0x33, 0xcc, 0x66, 0xff),
  V4(0x33, 0xcc, 0x33, 0xff),
  V4(0x33, 0xcc, 0x00, 0xff),
  V4(0x33, 0x99, 0xff, 0xff),
  V4(0x33, 0x99, 0xcc, 0xff),
  V4(0x33, 0x99, 0x99, 0xff),
  V4(0x33, 0x99, 0x66, 0xff),
  V4(0x33, 0x99, 0x33, 0xff),
  V4(0x33, 0x99, 0x00, 0xff),
  V4(0x33, 0x66, 0xff, 0xff),
  V4(0x33, 0x66, 0xcc, 0xff),
  V4(0x33, 0x66, 0x99, 0xff),
  V4(0x33, 0x66, 0x66, 0xff),
  V4(0x33, 0x66, 0x33, 0xff),
  V4(0x33, 0x66, 0x00, 0xff),
  V4(0x33, 0x33, 0xff, 0xff),
  V4(0x33, 0x33, 0xcc, 0xff),
  V4(0x33, 0x33, 0x99, 0xff),
  V4(0x33, 0x33, 0x66, 0xff),
  V4(0x33, 0x33, 0x33, 0xff),
  V4(0x33, 0x33, 0x00, 0xff),
  V4(0x33, 0x00, 0xff, 0xff),
  V4(0x33, 0x00, 0xcc, 0xff),
  V4(0x33, 0x00, 0x99, 0xff),
  V4(0x33, 0x00, 0x33, 0xff),
  V4(0x33, 0x00, 0x00, 0xff),
  V4(0x00, 0xff, 0xff, 0xff),
  V4(0x00, 0xff, 0xcc, 0xff),
  V4(0x00, 0xff, 0x99, 0xff),
  V4(0x00, 0xff, 0x66, 0xff),
  V4(0x00, 0xff, 0x33, 0xff),
  V4(0x00, 0xff, 0x00, 0xff), // GREEN
  V4(0x00, 0xcc, 0xff, 0xff),
  V4(0x00, 0xcc, 0xcc, 0xff),
  V4(0x00, 0xcc, 0x99, 0xff),
  V4(0x00, 0xcc, 0x66, 0xff),
  V4(0x00, 0xcc, 0x33, 0xff),
  V4(0x00, 0xcc, 0x00, 0xff),
  V4(0x00, 0x99, 0xff, 0xff),
  V4(0x00, 0x99, 0xcc, 0xff),
  V4(0x00, 0x99, 0x99, 0xff),
  V4(0x00, 0x99, 0x66, 0xff),
  V4(0x00, 0x99, 0x33, 0xff),
  V4(0x00, 0x99, 0x00, 0xff),
  V4(0x00, 0x66, 0xff, 0xff),
  V4(0x00, 0x66, 0xcc, 0xff),
  V4(0x00, 0x66, 0x99, 0xff),
  V4(0x00, 0x66, 0x66, 0xff),
  V4(0x00, 0x66, 0x33, 0xff),
  V4(0x00, 0x66, 0x00, 0xff),
  V4(0x00, 0x33, 0xff, 0xff),
  V4(0x00, 0x33, 0xcc, 0xff),
  V4(0x00, 0x33, 0x99, 0xff),
  V4(0x00, 0x33, 0x66, 0xff),
  V4(0x00, 0x33, 0x33, 0xff),
  V4(0x00, 0x33, 0x00, 0xff),
  V4(0x00, 0x00, 0xff, 0xff),
  V4(0x00, 0x00, 0xcc, 0xff),
  V4(0x00, 0x00, 0x99, 0xff),
  V4(0x00, 0x00, 0x33, 0xff),
  V4(0xee, 0x00, 0x00, 0xff),
  V4(0xdd, 0x00, 0x00, 0xff),
  V4(0xbb, 0x00, 0x00, 0xff),
  V4(0xaa, 0x00, 0x00, 0xff),
  V4(0x88, 0x00, 0x00, 0xff),
  V4(0x77, 0x00, 0x00, 0xff),
  V4(0x55, 0x00, 0x00, 0xff),
  V4(0x44, 0x00, 0x00, 0xff),
  V4(0x22, 0x00, 0x00, 0xff),
  V4(0x11, 0x00, 0x00, 0xff),
  V4(0x00, 0xee, 0x00, 0xff),
  V4(0x00, 0xdd, 0x00, 0xff),
  V4(0x00, 0xbb, 0x00, 0xff),
  V4(0x00, 0xaa, 0x00, 0xff),
  V4(0x00, 0x88, 0x00, 0xff),
  V4(0x00, 0x77, 0x00, 0xff),
  V4(0x00, 0x55, 0x00, 0xff),
  V4(0x00, 0x44, 0x00, 0xff),
  V4(0x00, 0x22, 0x00, 0xff),
  V4(0x00, 0x11, 0x00, 0xff),
  V4(0x00, 0x00, 0xee, 0xff),
  V4(0x00, 0x00, 0xdd, 0xff),
  V4(0x00, 0x00, 0xbb, 0xff),
  V4(0x00, 0x00, 0xaa, 0xff),
  V4(0x00, 0x00, 0x88, 0xff),
  V4(0x00, 0x00, 0x77, 0xff),
  V4(0x00, 0x00, 0x55, 0xff),
  V4(0x00, 0x00, 0x44, 0xff),
  V4(0x00, 0x00, 0x22, 0xff),
  V4(0x00, 0x00, 0x11, 0xff),
  V4(0xee, 0xee, 0xee, 0xff), // Greys
  V4(0xdd, 0xdd, 0xdd, 0xff),
  V4(0xbb, 0xbb, 0xbb, 0xff),
  V4(0xaa, 0xaa, 0xaa, 0xff),
  V4(0x88, 0x88, 0x88, 0xff),
  V4(0x55, 0x55, 0x55, 0xff),
  V4(0x44, 0x44, 0x44, 0xff),
  V4(0x22, 0x22, 0x22, 0xff),
  V4(0x11, 0x11, 0x11, 0xff)
};

inline v4
GetColorData(u32 ColorIndex, r32 Emission = 0.0f)
{
  Assert(ColorIndex < PALETTE_SIZE);
  v4 Color = DefaultPalette[ColorIndex];
  v4 Result = V4(Color.rgb/255.0f, Emission);
  return Result;;
}

inline void
FillColorArray(u32 ColorIndex, v4 *Array, s32 Count, r32 Emission = 1.0f)
{
  while (Count--)
  {
    Array[Count] = GetColorData(ColorIndex, Emission);
  }

  return;
}
