#include <bonsai_types.h>

#include <game_constants.h>
#include <game_types.h>

/* #define PLATFORM_THREADING_IMPLEMENTATIONS 1 */
/* #define PLATFORM_LIBRARY_AND_WINDOW_IMPLEMENTATIONS 1 */
/* #define PLATFORM_GL_IMPLEMENTATIONS 1 */
/* #define DEBUG_SYSTEM_API 1 */

#define RANDOM_HOTKEY_MASHING 0
#if RANDOM_HOTKEY_MASHING
static u32 HotkeyFrameTimeout = 0;
static random_series HotkeyEntropy = {};
static hotkeys StashedHotkeys = {};
#endif


model *
AllocateGameModels(game_state *GameState, memory_arena *Memory)
{
  model *Result = Allocate(model, GameState->Memory, ModelIndex_Count);

  Result[ModelIndex_Enemy] = LoadVoxModel(Memory, &GameState->Heap, ENEMY_MODEL);
  Result[ModelIndex_Player] = LoadCollada(Memory, &GameState->Heap, "models/two-axis-animated-cube.dae");
  /* Result[ModelIndex_Player] = LoadVoxModel(Memory, PLAYER_MODEL); */
  Result[ModelIndex_Loot] = LoadVoxModel(Memory, &GameState->Heap, LOOT_MODEL);

  /* chunk_dimension ProjectileDim = Chunk_Dimension(1,30,1); */
  /* Result[ModelIndex_Projectile].Chunk = AllocateChunk(Memory, &GameState->Heap, ProjectileDim); */
  /* Result[ModelIndex_Projectile].Dim = ProjectileDim; */
  /* FillChunk(Result[ModelIndex_Projectile].Chunk, ProjectileDim, GREEN); */

  Result[ModelIndex_Proton] = LoadVoxModel(Memory, &GameState->Heap, PROJECTILE_MODEL);

  return Result;
}

BONSAI_API_WORKER_THREAD_CALLBACK()
{
  switch (Entry->Type)
  {
    case type_work_queue_entry_noop: { InvalidCodePath(); } break;

    case type_work_queue_entry_init_asset:
    {
      NotImplemented;
    } break;

    case type_work_queue_entry_init_world_chunk:
    {
      world_chunk* DestChunk = Entry->work_queue_entry_init_world_chunk.Chunk;
      if (ChunkIsGarbage(DestChunk))
      {
        Chunk->Data->Flags = Chunk_MeshComplete;
      }
      else
      {
        s32 Amplititude = 100;
        s32 StartingZDepth = -100;
        InitializeWorldChunkPerlinPlane( Thread,
                                         DestChunk,
                                         WORLD_CHUNK_DIM,
                                         Amplititude,
                                         StartingZDepth );
      }
    } break;

    case type_work_queue_entry_copy_buffer:
    {
      work_queue_entry_copy_buffer *CopyJob = SafeAccess(work_queue_entry_copy_buffer, Entry);
      DoCopyJob(CopyJob);
    } break;

    case type_work_queue_entry_copy_buffer_set:
    {
      TIMED_BLOCK("Copy Set");
      work_queue_entry_copy_buffer_set *CopySet = SafeAccess(work_queue_entry_copy_buffer_set, Entry);
      for (u32 CopyIndex = 0; CopyIndex < CopySet->Count; ++CopyIndex)
      {
        work_queue_entry_copy_buffer CopyJob = CopySet->CopyTargets[CopyIndex];
        DoCopyJob(&CopyJob);
      }

      END_BLOCK("Copy Set");
    } break;
  }

  return;
}

BONSAI_API_MAIN_THREAD_CALLBACK()
{
  TIMED_FUNCTION();

#if DEBUG_SYSTEM_API
  if (!GetDebugState) { GetDebugState = (get_debug_state_proc)Plat->GetDebugStateProc; }
  GetDebugState()->Plat = GameState->Plat;
  GetDebugState()->GameState = GameState;
#endif

  /* DebugPrint(*GameState->Plat); */

  GL.Disable(GL_CULL_FACE);

  world                 *World         = GameState->World;
  graphics              *Graphics      = GameState->Graphics;
  g_buffer_render_group *gBuffer       = Graphics->gBuffer;
  ao_render_group       *AoGroup       = Graphics->AoGroup;
  camera                *Camera        = Graphics->Camera;

  Graphics->GpuBufferWriteIndex = (Graphics->GpuBufferWriteIndex + 1) % 2;
  gpu_mapped_element_buffer* GpuMap = Graphics->GpuBuffers + Graphics->GpuBufferWriteIndex;

  MapGpuElementBuffer(GpuMap);

  entity *Player = GameState->Player;
  ClearFramebuffers(Graphics);

#if RANDOM_HOTKEY_MASHING
  if (HotkeyFrameTimeout == 0)
  {
    Hotkeys->Left = RandomChoice(&HotkeyEntropy);
    Hotkeys->Right = RandomChoice(&HotkeyEntropy);
    Hotkeys->Forward = RandomChoice(&HotkeyEntropy);
    Hotkeys->Backward = RandomChoice(&HotkeyEntropy);
    StashedHotkeys = *Hotkeys;
    HotkeyFrameTimeout = 100;
  }
  else
  {
    HotkeyFrameTimeout--;
    *Hotkeys = StashedHotkeys;
  }
#endif


#if DEBUG_DRAW_WORLD_AXIES
  {
    untextured_3d_geometry_buffer CopyDest = ReserveBufferSpace(&GpuMap->Buffer, VERTS_PER_LINE*3);
    DEBUG_DrawLine(&CopyDest, V3(0,0,0), V3(10000, 0, 0), RED, 0.2f );
    DEBUG_DrawLine(&CopyDest, V3(0,0,0), V3(0, 10000, 0), GREEN, 0.2f );
    DEBUG_DrawLine(&CopyDest, V3(0,0,0), V3(0, 0, 10000), BLUE, 0.2f );
  }
#endif

  if (Hotkeys->Player_Spawn)
  {
    Unspawn(Player);
    world_position PlayerChunkP = World_Position(0, 0, -11);
    SpawnPlayer(GameState->Models, Player,  Canonical_Position(V3(0,0,2), World_Position(0,0,0)), &GameState->Entropy);
    World->Center = PlayerChunkP;
  }

  TIMED_BLOCK("Game Simulation");

  CollectUnusedChunks(World, &GameState->MeshFreelist, GameState->Memory, g_VisibleRegion);

  v2 MouseDelta = GetMouseDelta(Plat);
  input* GameInput = &Plat->Input;

#if DEBUG_SYSTEM_API
#if 0
  if (GetDebugState()->UiGroup.PressedInteractionId != StringHash("GameViewport"))
  {
    GameInput = 0;
  }
#endif
#endif

  UpdateGameCamera(MouseDelta, GameInput, Player->P, Camera, World->ChunkDim);
  SimulateEntities(World, GameState->EntityTable, Plat->dt, g_VisibleRegion);
  SimulateAndRenderParticleSystems(GameState->EntityTable, World->ChunkDim, &GpuMap->Buffer, Graphics, Plat->dt);

  END_BLOCK("Game Simulation");

  gBuffer->ViewProjection =
    ProjectionMatrix(Camera, Plat->WindowWidth, Plat->WindowHeight) *
    ViewMatrix(World->ChunkDim, Camera);

  DEBUG_COMPUTE_PICK_RAY(Plat, &gBuffer->ViewProjection);

  TIMED_BLOCK("BufferMeshes");
    BufferWorld(Plat, &GpuMap->Buffer, World, Graphics, g_VisibleRegion);
    BufferEntities( GameState->EntityTable, &GpuMap->Buffer, Graphics, World, Plat->dt);
  END_BLOCK("BufferMeshes");

#if DEBUG_SYSTEM_API
  for (u32 ChunkIndex = 0;
      ChunkIndex < GetDebugState()->PickedChunkCount;
      ++ChunkIndex)
  {
    world_chunk *Chunk = GetDebugState()->PickedChunks[ChunkIndex];
    untextured_3d_geometry_buffer CopyDest = ReserveBufferSpace(&GpuMap->Buffer, VERTS_PER_AABB);
    u8 Color = GREEN;

    if (Chunk == GetDebugState()->HotChunk)
    {
      Color = PINK;
    }

    DEBUG_DrawChunkAABB(&CopyDest, Graphics, Chunk, World->ChunkDim, Color, 0.35f);
  }
#endif

  TIMED_BLOCK("Wait for worker threads");
    for (;;) { if (QueueIsEmpty(&Plat->HighPriority)) { break; } }
  END_BLOCK("Wait for worker threads");

  TIMED_BLOCK("RenderToScreen");
    RenderGBuffer(GpuMap, Graphics);
    RenderAoTexture(AoGroup);
    DrawGBufferToFullscreenQuad(Plat, Graphics);
  END_BLOCK("RenderToScreen");

  Graphics->Lights->Count = 0;

  return;
}

BONSAI_API_MAIN_THREAD_INIT_CALLBACK()
{
  Info("Initializing Game");

  GL = *GL_in;

  Init_Global_QuadVertexBuffer();

  game_state *GameState = Allocate(game_state, GameMemory, 1);
  GameState->Memory = GameMemory;
  GameState->Noise = perlin_noise(DEBUG_NOISE_SEED);

  GameState->Graphics = GraphicsInit(GameMemory);
  if (!GameState->Graphics) { Error("Initializing Graphics"); return False; }

  StandardCamera(GameState->Graphics->Camera, 10000.0f, 300.0f);

  GameState->Plat = Plat;
  GameState->Entropy.Seed = DEBUG_NOISE_SEED;

  world_position WorldCenter = World_Position(0, 0, 0);

  GameState->Heap = InitHeap(Gigabytes(4));
  GameState->World = AllocateAndInitWorld(WorldCenter, WORLD_CHUNK_DIM, g_VisibleRegion);

  GameState->EntityTable = AllocateEntityTable(GameMemory, TOTAL_ENTITY_COUNT);

  GameState->Models = AllocateGameModels(GameState, GameState->Memory);

  GameState->Player = GetFreeEntity(GameState->EntityTable);
  SpawnPlayer(GameState->Models, GameState->Player, Canonical_Position(Voxel_Position(0), WorldCenter), &GameState->Entropy);

  return GameState;
}

BONSAI_API_WORKER_THREAD_INIT_CALLBACK()
{
  Thread->MeshFreelist = &GameState->MeshFreelist;
  Thread->Noise = &GameState->Noise;
}
