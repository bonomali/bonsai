
#include <bonsai.h>

#include <constants.hpp>

#include <vox_loader.cpp>
#include <perlin.cpp>

#include <shader.cpp>
#include <render.cpp>
#include <texture.cpp> // Only used for font .DDS atm
#include <debug.cpp>

GLOBAL_VARIABLE PerlinNoise GlobalNoise;

  // TODO(Jesse): Re-enable this!
#if 0
void
OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
	if (UseDebugCamera)
	{
		DEBUG_CAMERA_FOCAL_LENGTH -= (yoffset * DEBUG_CAMERA_SCROLL_SPEED);
	}
	else
	{
		CAMERA_FOCAL_LENGTH -= (yoffset * PLAYER_ACCEL_MULTIPLIER);
	}

  return;
}
#endif

void
InitializeVoxels( game_state *GameState, world_chunk *WorldChunk )
{
  Assert(WorldChunk);

#if DEBUG_OPTIMIZE_WORLD_GC
  // If the chunk was marked as garbage before it had been initialized we can
  // simply mark it as collected and skip it.
  if ( IsSet(WorldChunk->Data->flags, Chunk_Garbage) )
  {
    WorldChunk->Data->flags = SetFlag(WorldChunk->Data->flags, Chunk_Collected);
    return;
  }
#endif

  ZeroChunk(WorldChunk->Data);

  chunk_data *chunk = WorldChunk->Data;
  /* CALLGRIND_TOGGLE_COLLECT; */

  chunk_dimension Dim = GameState->world->ChunkDim;

  for ( int z = 0; z < Dim.z; ++ z)
  {
    for ( int y = 0; y < Dim.y; ++ y)
    {
      for ( int x = 0; x < Dim.x; ++ x)
      {
        int i = GetIndex(Voxel_Position(x,y,z), chunk, Dim);
        chunk->Voxels[i].flags = 0;

        voxel_position P = Voxel_Position(x,y,z);

        chunk->Voxels[i] = SetVoxelP(chunk->Voxels[i], P);
        chunk->Voxels[i] = SetVoxelColor(chunk->Voxels[i], 42);

        Assert( GetVoxelP(chunk->Voxels[i]) == P );
        Assert( NotSet( chunk->Voxels[i].flags, Voxel_Filled) );



#if DEBUG_WORLD_GENERATION
        if ( (y == 0 && WorldChunk->WorldP.y == 3) )
        {
          chunk->Voxels[i].flags = SetFlag(chunk->Voxels[i].flags, Voxel_Filled);
        }
#else
        v3 NoiseInputs =
          ( ( V3(x,y,z) + (CHUNK_DIMENSION*(WorldChunk->WorldP))) ) / NOISE_FREQUENCY;

        double InX = (double)NoiseInputs.x;
        double InY = (double)NoiseInputs.y;
        double InZ = (double)NoiseInputs.z;

        float noiseValue = (float)GlobalNoise.noise(InX, InY, InZ);

        int Noise01 = Floori(noiseValue + 0.5f);

        Assert(Noise01 == 0 || Noise01 == 1);

        chunk->Voxels[i].flags = SetFlag( chunk->Voxels[i].flags, Noise01 * Voxel_Filled );

        if (Noise01 == 0)
        {
          Assert( NotSet( chunk->Voxels[i].flags, Voxel_Filled) );
        }
        else
        {
          Assert( IsSet( chunk->Voxels[i].flags, Voxel_Filled) );
        }

        voxel_position AssignedP = GetVoxelP(chunk->Voxels[i]);
        Assert( AssignedP == P );
#endif
      }
    }
  }

  /* CALLGRIND_TOGGLE_COLLECT; */

  chunk->flags = SetFlag(chunk->flags, Chunk_Initialized);

  BuildWorldChunkBoundaryVoxels(GameState, WorldChunk);

  return;
}

void
InitializeVoxels(void *Input)
{
  work_queue_entry *Params = (work_queue_entry *)Input;
  Assert(Params);

  game_state *GameState = Params->GameState;
  world_chunk *Chunk = (world_chunk *)Params->Input;

  Assert(GameState);
  Assert(Chunk);

  InitializeVoxels(GameState, Chunk);

  return;
}

inline b32
IsFilledInChunk( chunk_data *Chunk, voxel_position VoxelP, chunk_dimension Dim)
{
  b32 isFilled = True;

  if (Chunk && IsSet(Chunk->flags, Chunk_Initialized) )
  {
    int i = GetIndex(VoxelP, Chunk, Dim);

    Assert(i > -1);
    Assert(i < Volume(Dim));
    Assert(VoxelP == GetVoxelP(Chunk->Voxels[i]));

    isFilled = IsSet(Chunk->Voxels[i].flags, Voxel_Filled);
  }

  return isFilled;
}

inline b32
NotFilledInChunk( chunk_data *Chunk, voxel_position VoxelP, chunk_dimension Dim)
{
  b32 Result = !IsFilledInChunk(Chunk, VoxelP, Dim);
  return Result;
}

inline b32
IsFilledInWorld( World *world, world_chunk *chunk, canonical_position VoxelP )
{
  b32 isFilled = true;

  if ( chunk )
  {
    world_chunk *localChunk = chunk;

    if ( chunk->WorldP != VoxelP.WorldP )
    {
      localChunk = GetWorldChunk(world, VoxelP.WorldP);
    }

    isFilled = IsFilledInChunk(localChunk->Data, Voxel_Position(VoxelP.Offset), world->ChunkDim );
  }

  return isFilled;
}

inline b32
NotFilledInWorld( World *world, world_chunk *chunk, canonical_position VoxelP )
{
  b32 Result = !(IsFilledInWorld(world, chunk, VoxelP));
  return Result;
}

inline void
QueueChunkForInit(game_state *GameState, world_chunk *Chunk)
{
  Assert( NotSet(Chunk->Data->flags, Chunk_Queued ) );
  Assert( NotSet(Chunk->Data->flags, Chunk_Initialized) );

  work_queue_entry Entry;
  Entry.Callback = &InitializeVoxels;
  Entry.Input = (void*)Chunk;
  Entry.GameState = GameState;

  Chunk->Data->flags = SetFlag(Chunk->Data->flags, Chunk_Queued);

  GameState->Plat->PushWorkQueueEntry(&GameState->Plat->Queue, &Entry);

  return;
}

inline v3
GetInputsFromController(platform *Plat, Camera_Object *Camera)
{
  v3 right = Camera->Right;
  v3 forward = Camera->Front;

  v3 UpdateDir = V3(0,0,0);

  // Forward
  if ( Plat->Input.W )
    UpdateDir += forward;

  // Backward
  if ( Plat->Input.S )
    UpdateDir -= forward;

  // Right
  if ( Plat->Input.D )
    UpdateDir += right;

  // Left
  if ( Plat->Input.A )
    UpdateDir -= right;

  UpdateDir.y = 0;

  UpdateDir = Normalize(UpdateDir, Length(UpdateDir));

  return UpdateDir;
}

collision_event
GetCollision( World *world, canonical_position TestP, chunk_dimension ModelDim)
{
  collision_event Collision;
  Collision.didCollide = false;

  TestP = Canonicalize(world, TestP);

  voxel_position MinP = Voxel_Position(TestP.Offset);
  voxel_position MaxP = Voxel_Position(TestP.Offset + ModelDim);

  // We need to check if the TestP is exactly on a voxel boundary.
  // if it is, don't include the next voxel in our detection.
  if ( TestP.Offset.x == Floorf(TestP.Offset.x) )
    MaxP.x -= 1;

  if ( TestP.Offset.y == Floorf(TestP.Offset.y) )
    MaxP.y -= 1;

  if ( TestP.Offset.z == Floorf(TestP.Offset.z) )
    MaxP.z -= 1;

  for ( int z = MinP.z; z <= MaxP.z; z++ )
  {
    for ( int y = MinP.y; y <= MaxP.y; y++ )
    {
      for ( int x = MinP.x; x <= MaxP.x; x++ )
      {
        canonical_position LoopTestP = Canonicalize( world, V3(x,y,z), TestP.WorldP );

        world_chunk *chunk = GetWorldChunk( world, LoopTestP.WorldP );


        // TODO(Jesse): Can we somehow atomically pull this one off the queue
        // and initialize it on demand?
#if 0
        if (chunk && NotSet(chunk->Data->flags, Chunk_Initialized) )
        {
          chunk->Data->flags = (chunk->Data->flags, Chunk_Queued);
          InitializeVoxels(chunk);
        }
#endif

        if ( IsFilledInChunk(chunk->Data, Voxel_Position(LoopTestP.Offset), world->ChunkDim) )
        {
          Collision.CP = LoopTestP;
          Collision.didCollide = true;
          goto end;
        }

      }
    }
  }
  end:

  return Collision;
}

collision_event
GetCollision(World *world, Entity *entity, v3 Offset = V3(0,0,0) )
{
  Assert( entity->Spawned );
  Assert( entity->Model->BoundaryVoxelCount > 0 );

  collision_event C;
  C.didCollide = false;

  for (int i = 0; i < entity->Model->BoundaryVoxelCount; ++i)
  {
    canonical_position CP;
    CP.Offset = V3(GetVoxelP(entity->Model->BoundaryVoxels[i])) + entity->P.Offset + Offset;
    CP.WorldP = entity->P.WorldP;

    C = GetCollision(world, CP, Chunk_Dimension(1,1,1) );

    if (C.didCollide)
      return C;
  }

  return C;
}

inline b32
IsGrounded( World *world, Entity *entity)
{
  collision_event c = GetCollision(world, entity, V3(0.0f,-0.001f, 0.0f));
  return c.didCollide;
}

v3
GetAtomicUpdateVector( v3 Gross )
{
  v3 Result = Gross;

  if ( Gross.x > 1.0f )
  {
    Result.x = 1.0f;
  }
  if ( Gross.x < -1.0f )
  {
    Result.x = -1.0f;
  }


  if ( Gross.y > 1.0f )
  {
    Result.y = 1.0f;
  }
  if ( Gross.y < -1.0f )
  {
    Result.y = -1.0f;
  }


  if ( Gross.z > 1.0f )
  {
    Result.z = 1.0f;
  }
  if ( Gross.z < -1.0f )
  {
    Result.z = -1.0f;
  }

  return Result;
}

inline float
ClampMinus1toInfinity( float f )
{
  float Result = f;

  if (Result < -1 )
  {
    Result = -1;
  }

  return Result;
}

inline voxel_position
ClampMinus1toInfinity( voxel_position V )
{
  voxel_position Result = V;

  if ( V.x < 0 )
    Result.x = -1;

  if ( V.y < 0 )
    Result.y = -1;

  if ( V.z < 0 )
    Result.z = -1;

  return Result;
}

world_chunk*
GetFreeChunk(platform *Plat, World *world, world_position P)
{
  world_chunk *Result = 0;

  if (world->FreeChunkCount == 0)
  {
    Result = AllocateWorldChunk(Plat, world, P);
    Assert(Result);
  }
  else
  {
    Assert(world->FreeChunkCount > 0);
    Result = world->FreeChunks[--world->FreeChunkCount];
    Result->WorldP = P;

    Assert(Result->Next == 0);
    Assert(Result->Prev == 0);

    InsertChunkIntoWorld(world, Result);
  }

  Assert( NotSet(Result->Data->flags, Chunk_Queued) );
  Assert( NotSet(Result->Data->flags, Chunk_Initialized) );

  return Result;
}

void
QueueChunksForInit(game_state *GameState, world_position WorldDisp, Entity *Player)
{
  if (Length(V3(WorldDisp)) == 0) return;

  world_position PlayerP = Player->P.WorldP;

  world_position Iter = GetSign(WorldDisp);

  world_position InvAbsIter = ((Iter * Iter)-1) * ((Iter * Iter)-1);

  world_position VRHalfDim = World_Position(GameState->world->VisibleRegion/2);

  world_position SliceMin = PlayerP + (VRHalfDim * Iter) - (VRHalfDim * InvAbsIter) - ClampPositive(WorldDisp);
  world_position SliceMax = PlayerP + (VRHalfDim * Iter) + (VRHalfDim * InvAbsIter) - ClampPositive(Iter) - InvAbsIter - ClampNegative(WorldDisp) + ClampNegative(Iter);

  LastQueuedSlice = aabb(SliceMin*CHUNK_DIMENSION - 1, (SliceMax*CHUNK_DIMENSION + CHUNK_DIMENSION + 1));

  for (int z = SliceMin.z; z <= SliceMax.z; ++ z)
  {
    for (int y = SliceMin.y; y <= SliceMax.y; ++ y)
    {
      for (int x = SliceMin.x; x <= SliceMax.x; ++ x)
      {
        world_position P = World_Position(x,y,z);
        world_chunk* chunk = GetFreeChunk(GameState->Plat, GameState->world, P);
        QueueChunkForInit(GameState, chunk);
      }
    }
  }
}

void
UpdateVisibleRegion(game_state *GameState, world_position OriginalPlayerP, Entity *Player)
{
  if ( OriginalPlayerP != Player->P.WorldP && DEBUG_SCROLL_WORLD ) // We moved to the next chunk
  {
    world_position WorldDisp = ( Player->P.WorldP - OriginalPlayerP );
    QueueChunksForInit(GameState, World_Position(WorldDisp.x, 0, 0), Player);
    QueueChunksForInit(GameState, World_Position(0, WorldDisp.y, 0), Player);
    QueueChunksForInit(GameState, World_Position(0, 0, WorldDisp.z), Player);
  }

  return;
}

void
SpawnPlayer( World *world, platform *Plat, Entity *Player )
{
  Player->Acceleration = V3(0,0,0);
  Player->Velocity = V3(0,0,0);

  canonical_position TestP;
  collision_event Collision;

  int rX = rand() % (world->ChunkDim.x);
  int rY = rand() % (world->ChunkDim.y);
  int rZ = rand() % (world->ChunkDim.z);

  v3 Offset = V3( rX, rY, rZ );
  TestP = Canonicalize(world, Offset, Player->P.WorldP);

  Collision = GetCollision( world, TestP, Player->ModelDim);

  if (!Collision.didCollide)
  {
    Player->P = TestP;
    Player->Spawned = true;
  }
  else
  {
    // world_position OriginalP = Player->P.WorldP;
    // Player->P.WorldP += World_Position(1,0,0);
    // UpdateVisibleRegion(world, Plat, OriginalP, Player);
  }

  return;
}

void
UpdatePlayerP(game_state *GameState, Entity *Player, v3 GrossUpdateVector)
{
  TIMED_FUNCTION();

  World *world = GameState->world;

  v3 Remaining = GrossUpdateVector;
  /* Remaining = V3(-16, 0, 0); */
  world_position OriginalPlayerP = Player->P.WorldP;

  collision_event C;
  while ( Remaining != V3(0,0,0) )
  {
    Assert(LengthSq(Remaining) >= 0);

    v3 UpdateVector = GetAtomicUpdateVector(Remaining);
    Remaining -= UpdateVector;


    Player->P.Offset.y += UpdateVector.y;
    Player->P = Canonicalize(world, Player->P);
    C = GetCollision(world, Player);
    if (C.didCollide)
    {
      Player->Velocity.y = 0;

      Player->P.Offset.y = C.CP.Offset.y;
      Player->P.WorldP.y = C.CP.WorldP.y;

      if (UpdateVector.y > 0)
        Player->P.Offset.y -= (Player->ModelDim.y-1);

      Player->P = Canonicalize(world, Player->P);
    }


    Player->P.Offset.x += UpdateVector.x;
    Player->P = Canonicalize(world, Player->P);
    C = GetCollision(world, Player);
    if (C.didCollide)
    {
      C = GetCollision(world, Player, V3(0,1,0));

      if (C.didCollide)
      {
        Player->Velocity.x = 0;

        Player->P.Offset.x = C.CP.Offset.x;
        Player->P.WorldP.x = C.CP.WorldP.x;

        if (UpdateVector.x > 0)
          Player->P.Offset.x -= (Player->ModelDim.x-1);
      }
      else
      {
        Player->P.Offset += V3(0,1,0);
      }
      Player->P = Canonicalize(world, Player->P);
    }


    Player->P.Offset.z += UpdateVector.z;
    Player->P = Canonicalize(world, Player->P);
    C = GetCollision(world, Player);
    if (C.didCollide)
    {
      C = GetCollision(world, Player, V3(0,1,0));
      if (C.didCollide)
      {
        Player->Velocity.z = 0;

        Player->P.Offset.z = C.CP.Offset.z;
        Player->P.WorldP.z = C.CP.WorldP.z;

        if (UpdateVector.z > 0)
          Player->P.Offset.z -= (Player->ModelDim.z-1);
      }
      else
      {
        Player->P.Offset += V3(0,1,0);
      }
      Player->P = Canonicalize(world, Player->P);
    }

  }

  UpdateVisibleRegion(GameState, OriginalPlayerP, Player);

  Player->P = Canonicalize(world, Player->P);
  Assert ( GetCollision(world, Player ).didCollide == false );

  return;
}

inline v2
GetMouseDelta(platform *Plat)
{
  float mouseSpeed = 1.00f;

  v2 Result = {};

  if (Plat->Input.LMB)
    Result = Plat->MouseDP * mouseSpeed;

  return Result;
}

void
UpdateDebugCamera(platform *Plat, World *world, v3 TargetDelta, Camera_Object *Camera)
{
  float FocalLength = DEBUG_CAMERA_FOCAL_LENGTH;

  if (Plat->Input.Q)
    TargetDelta -= WORLD_Y;

  if (Plat->Input.E)
    TargetDelta += WORLD_Y;

  Camera->Right = Normalize(Cross(Camera->Front, WORLD_Y));
  Camera->Up = Normalize(Cross(Camera->Front, Camera->Right));

  v2 MouseDelta = GetMouseDelta(Plat);
  v3 UpdateRight = Camera->Right * MouseDelta.x;
  v3 UpdateUp = Camera->Up * MouseDelta.y;

  TargetDelta = (TargetDelta * DEBUG_CAMERA_SCROLL_SPEED) + UpdateRight + UpdateUp;

  Camera->P.Offset += (TargetDelta + UpdateRight + (UpdateUp));
  Camera->Target.Offset += TargetDelta;

  Camera->P = Canonicalize(world, Camera->P);
  Camera->Target = Canonicalize(world, Camera->Target);

  v3 TargetToCamera = Normalize(GetRenderP(world, Camera->P, Camera) - GetRenderP(world, Camera->Target, Camera));
  Camera->P.Offset = Camera->Target.Offset + (TargetToCamera * FocalLength);
  Camera->P.WorldP = Camera->Target.WorldP;

  Camera->Front = Normalize( GetRenderP(world, Camera->Target, Camera) - GetRenderP(world, Camera->P, Camera) );

  return;
}

void
UpdateCameraP(platform *Plat, World *world, Entity *Player, Camera_Object *Camera)
{
#if DEBUG_CAMERA_FOCUS_ORIGIN
  canonical_position NewTarget = Canonical_Position( V3(0,0,0), World_Position(0,0,0) );
#else
  canonical_position NewTarget = Canonicalize(world, Player->P.Offset, Player->P.WorldP) + (Player->ModelDim/2);
#endif



  v3 TargetDelta = GetRenderP(world, NewTarget, Camera) - GetRenderP(world, Camera->Target, Camera);

  float FocalLength = CAMERA_FOCAL_LENGTH;

  Camera->Right = Normalize(Cross(Camera->Front, WORLD_Y));
  Camera->Up = Normalize(Cross(Camera->Front, Camera->Right));

  v2 MouseDelta = GetMouseDelta(Plat);
  v3 UpdateRight = Camera->Right * MouseDelta.x;
  v3 UpdateUp = Camera->Up * MouseDelta.y;

  Camera->P.Offset += (TargetDelta + UpdateRight + (UpdateUp));
  Camera->Target.Offset += TargetDelta;

  Camera->P = Canonicalize(world, Camera->P);
  Camera->Target = Canonicalize(world, Camera->Target);

  v3 TargetToCamera = Normalize(GetRenderP(world, Camera->P, Camera) - GetRenderP(world, Camera->Target, Camera));
  Camera->P.Offset = Camera->Target.Offset + (TargetToCamera * FocalLength);
  Camera->P.WorldP = Camera->Target.WorldP;

  Camera->Front = Normalize( GetRenderP(world, Camera->Target, Camera) - GetRenderP(world, Camera->P, Camera) );
  Camera->Up = Normalize(Cross(Camera->Front, Camera->Right));

  // Frustum computation
  //
  v3 FrustLength = V3(0.0f,0.0f, Camera->Frust.farClip);
  v3 FarHeight = ( V3( 0.0f, ((Camera->Frust.farClip - Camera->Frust.nearClip)/cos(Camera->Frust.FOV/2)) * sin(Camera->Frust.FOV/2), 0.0f));
  v3 FarWidth = V3( FarHeight.y, 0.0f, 0.0f);

  v3 MaxMax = FrustLength + FarHeight + FarWidth;
  v3 MaxMin = FrustLength + FarHeight - FarWidth;
  v3 MinMax = FrustLength - FarHeight + FarWidth;
  v3 MinMin = FrustLength - FarHeight - FarWidth;

  v3 Front = V3(0,0,1);
  v3 Target = Camera->Front;

  Quaternion GrossRotation = RotatePoint(Front, Target);

  // We've got to correct the rotation so it ends pointing the frustum in the cameras 'up' direction
  v3 UpVec = V3(0, 1, 0);
  v3 RotatedUp = Rotate(UpVec, GrossRotation);
  Quaternion DesiredUp = RotatePoint(RotatedUp, Camera->Up);

  Quaternion FinalRotation = DesiredUp * GrossRotation;

  MaxMin = Rotate(MaxMin, FinalRotation);
  MaxMax = Rotate(MaxMax, FinalRotation);
  MinMin = Rotate(MinMin, FinalRotation);
  MinMax = Rotate(MinMax, FinalRotation);

  v3 CameraRenderP = GetRenderP(world, Camera->P, Camera);

  plane Top(CameraRenderP,   Normalize(Cross(MaxMax, MaxMin)));
  plane Bot(CameraRenderP,   Normalize(Cross(MinMin, MinMax)));
  plane Left(CameraRenderP,  Normalize(Cross(MinMax, MaxMax)));
  plane Right(CameraRenderP, Normalize(Cross(MaxMin, MinMin)));

  Camera->Frust.Top = Top;
  Camera->Frust.Bot = Bot;
  Camera->Frust.Left = Left;
  Camera->Frust.Right = Right;

  // TODO(Jesse): Cull these as well?
  /* plane Near; */
  /* plane Far; */

  if (UseDebugCamera)
  {
    MaxMin = MaxMin + CameraRenderP;
    MaxMax = MaxMax + CameraRenderP;
    MinMin = MinMin + CameraRenderP;
    MinMax = MinMax + CameraRenderP;

    DEBUG_DrawLine(world, CameraRenderP, MaxMax, RED, 1.0f);
    DEBUG_DrawLine(world, CameraRenderP, MaxMin, GREEN, 1.0f);
    DEBUG_DrawLine(world, CameraRenderP, MinMax, BLUE, 1.0f);
    DEBUG_DrawLine(world, CameraRenderP, MinMin, WHITE, 1.0f);

    DEBUG_DrawPointMarker(world, MaxMax, GREEN, 5.0f);
    DEBUG_DrawPointMarker(world, MaxMin, GREEN, 5.0f);
    DEBUG_DrawPointMarker(world, MinMax, GREEN, 5.0f);
    DEBUG_DrawPointMarker(world, MinMin, GREEN, 5.0f);
  }

  return;
}

World *
AllocateWorld( game_state *GameState, world_position Midpoint)
{
  platform *Plat = GameState->Plat;

  /*
   *  Allocate stuff
   */
  World *world = PUSH_STRUCT_CHECKED(World, Plat->Memory, 1 );
  GameState->world = world;

  Assert(world->WorldStorage.Next == 0);

  world->WorldStorage.Arena = PUSH_STRUCT_CHECKED(memory_arena, Plat->Memory, 1);
  AllocateAndInitializeArena(world->WorldStorage.Arena, WORLD_STORAGE_SIZE);

  world->ChunkHash = PUSH_STRUCT_CHECKED(world_chunk*, Plat->Memory, WORLD_HASH_SIZE );
  world->FreeChunks = PUSH_STRUCT_CHECKED(world_chunk*, Plat->Memory, FREELIST_SIZE );

  Assert(world->FreeChunkCount == 0);

  /*
   *  Initialize stuff
   */

  world->ChunkDim = CHUNK_DIMENSION;
  world->VisibleRegion = VISIBLE_REGION;

  world->Gravity = WORLD_GRAVITY;

  {
    int BufferVertices = 100*(VOLUME_VISIBLE_REGION * VERT_PER_VOXEL);

    world->Mesh.VertexData = PUSH_STRUCT_CHECKED(GLfloat, Plat->Memory, BufferVertices );
    world->Mesh.ColorData = PUSH_STRUCT_CHECKED(GLfloat,  Plat->Memory, BufferVertices );
    world->Mesh.NormalData = PUSH_STRUCT_CHECKED(GLfloat, Plat->Memory, BufferVertices );

    world->Mesh.bytesAllocd = BufferVertices*sizeof(r32);
    Assert(world->Mesh.filled == 0);
    Assert(world->Mesh.VertexCount == 0);
  }

  world_position Min = Midpoint - (world->VisibleRegion/2);
  world_position Max = Midpoint + (world->VisibleRegion/2);

  for ( int z = Min.z; z < Max.z; ++ z )
  {
    for ( int y = Min.y; y < Max.y; ++ y )
    {
      for ( int x = Min.x; x < Max.x; ++ x )
      {
        world_chunk *chunk = AllocateWorldChunk(Plat, world, World_Position(x,y,z));
        Assert(chunk);
        QueueChunkForInit(GameState, chunk);
      }
    }
  }

  return world;
}

