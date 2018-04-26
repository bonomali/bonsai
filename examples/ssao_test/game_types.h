struct world;
struct platform;
struct entity;
struct memory_arena;
struct random_series;

struct game_state
{
  world         *World;
  platform      *Plat;
  entity        *Player;
  memory_arena *Memory;

  random_series Entropy;

  server_state ServerState = {};

  game_mode Mode;
  model   *Models;
  noise_3d *Turb;
  event_queue EventQueue;
  logical_frame_state Frame;
  aabb *FolieTable;
  entity *EntityTable[TOTAL_ENTITY_COUNT];
};

