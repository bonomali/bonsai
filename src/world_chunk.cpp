inline b32
ChunkIsGarbage(world_chunk* Chunk)
{
  Assert( IsSet(Chunk, Chunk_Queued) );
  b32 Garbage = False;

  if (IsSet(Chunk, Chunk_Garbage))
  {
    Chunk->Data->Flags = Chunk_Collected;
    Garbage = True;
  }

  return Garbage;
}

function world_chunk*
AllocateWorldChunk(memory_arena *Storage, world_position WorldP, chunk_dimension Dim = WORLD_CHUNK_DIM)
{
  u32 MaxLodMeshVerts = POINT_BUFFER_SIZE*3;

  world_chunk *Result = AllocateAlignedProtection(world_chunk, Storage, 1, 64, false);
  // FIXME(Jesse): The *2048 is an unnecessary debugging crutch .. take it out
  Result->LodMesh     = AllocateMesh(Storage, MaxLodMeshVerts*2048);
  Result->Data        = AllocateChunk(Storage, Dim);
  Result->WorldP      = WorldP;

  Result->CurrentTriangles = AllocateCurrentTriangles(2*4096, Storage);
  Result->CurrentTriangles->SurfacePoints = AllocateAlignedProtection(boundary_voxels, Storage, 1, 64, False);

  // TODO(Jesse): Allocate in a more sensible way?
  Result->CurrentTriangles->SurfacePoints->Points = AllocateAlignedProtection(voxel_position, Storage, Volume(WORLD_CHUNK_DIM), 64, False);

  SeedTriangulation(Result->CurrentTriangles, Storage);

  return Result;
}

inline u32
GetWorldChunkHash(world_position P)
{
  /* TIMED_FUNCTION(); */

  u32 I =
    (u32)((P.x) +
         (P.y*VR_X) +
         (P.z*VR_X*VR_Y)) * WORLD_HASH_SIZE_MULTIPLIER;

  u32 HashIndex = I % WORLD_HASH_SIZE;
  return HashIndex;
}

function void
InsertChunkIntoWorld(world *World, world_chunk *Chunk)
{
  u32 HashIndex = GetWorldChunkHash(Chunk->WorldP);
  u32 StartingHashIndex = HashIndex;
#if BONSAI_INTERNAL
  u32 BucketsSkipped = 0;
#endif
  world_chunk **Current = World->ChunkHash + HashIndex;

  while (*Current)
  {
    HashIndex = (HashIndex + 1) % WORLD_HASH_SIZE;
    Current = World->ChunkHash + HashIndex;

    if (HashIndex == StartingHashIndex)
    {
      Error("Hashtable full!");
      break;
    }

#if BONSAI_INTERNAL
    ++ BucketsSkipped;
#endif

  }

#if BONSAI_INTERNAL
  if (BucketsSkipped > 5)
  {
    Warn("%u Collisions encountered while inserting chunk into world", BucketsSkipped);
  }
#endif

  *Current = Chunk;

  return;
}

function world_chunk*
GetWorldChunkFor(memory_arena *Storage, world *World, world_position P)
{
  /* TIMED_FUNCTION(); */
  world_chunk *Result = 0;

  if (World->FreeChunkCount == 0)
  {
    Result = AllocateWorldChunk(Storage, P);
  }
  else
  {
    Result = World->FreeChunks[--World->FreeChunkCount];
  }

  Assert(Result->Data->Flags == Chunk_Uninitialized);

  Result->WorldP = P;
  InsertChunkIntoWorld(World, Result);

  return Result;
}

inline u32
GetNextWorldChunkHash(u32 HashIndex)
{
  u32 Result = (HashIndex + 1) % WORLD_HASH_SIZE;
  return Result;
}

function void
DeallocateMesh(world_chunk* Chunk, mesh_freelist* MeshFreelist, memory_arena* Memory)
{
  Chunk->Mesh->At = 0;
  free_mesh* Container = Unlink_TS(&MeshFreelist->Containers);
  if (!Container)
  {
    Container = Allocate(free_mesh, Memory, 1);
  }

  Container->Mesh = Chunk->Mesh;
  Chunk->Mesh = 0;

  Link_TS(&MeshFreelist->FirstFree, Container);

  return;
}

function void
FreeWorldChunk(world *World, world_chunk *Chunk , mesh_freelist* MeshFreelist, memory_arena* Memory)
{
  TIMED_FUNCTION();
  if ( Chunk->Data->Flags == Chunk_MeshComplete || Chunk->Data->Flags == Chunk_Collected  )
  {
    Chunk->LodMesh_Complete = False;
    Chunk->LodMesh->At = 0;

    if (Chunk->Mesh)
    {
      DeallocateMesh(Chunk, MeshFreelist, Memory);
    }

    Assert(World->FreeChunkCount < FREELIST_SIZE);
    World->FreeChunks[World->FreeChunkCount++] = Chunk;


    // FIXME(Jesse): Memoryleak
    SeedTriangulation( Chunk->CurrentTriangles, Memory);
    Chunk->CurrentTriangles->CurrentPointIndex = 0;
    Chunk->CurrentTriangles->SurfacePoints->End = 0;

    ZeroChunk(Chunk->Data);
  }
  else
  {
    SetFlag(Chunk, Chunk_Garbage);
  }

  return;
}

function world_chunk*
GetWorldChunk( world *World, world_position P )
{
  /* TIMED_FUNCTION(); */
  u32 HashIndex = GetWorldChunkHash(P);

  world_chunk *Result = World->ChunkHash[HashIndex];

  while (Result)
  {
    if (Result->WorldP == P)
    {
      break;
    }

    HashIndex = (HashIndex + 1) % WORLD_HASH_SIZE;
    Result = World->ChunkHash[HashIndex];
  }

  return Result;
}

function void
CollectUnusedChunks(world *World, mesh_freelist* MeshFreelist, memory_arena* Memory)
{
  TIMED_FUNCTION();
  world_chunk ** WorldHash = World->ChunkHash;

  world_position CenterP = World->Center;
  world_position Min = CenterP - VISIBLE_REGION_RADIUS;
  world_position Max = CenterP + VISIBLE_REGION_RADIUS;

  for (u32 ChunkIndex = 0;
      ChunkIndex < WORLD_HASH_SIZE;
      ++ChunkIndex)
  {
    world_chunk** Chunk = WorldHash + ChunkIndex;

    if (*Chunk)
    {
      world_position ChunkP = (*Chunk)->WorldP;

      if ( !(ChunkP >= Min && ChunkP <= Max) )
      {

        if ( (*Chunk)->Data->Flags == Chunk_MeshComplete || (*Chunk)->Data->Flags == Chunk_Collected  )
        {
          /* ++ChunksCollected; */
          u32 ChunkHash = GetWorldChunkHash(ChunkP);

          world_chunk** Current = WorldHash + ChunkHash;
          u32 CurrentHash = GetWorldChunkHash((*Current)->WorldP);

          world_chunk** LastChunkOfSameHashValue = WorldHash + ChunkHash;

          while (*Current && GetWorldChunkHash((*Current)->WorldP) == ChunkHash)
          {
            LastChunkOfSameHashValue = Current;
            CurrentHash = GetNextWorldChunkHash(CurrentHash);
            Current = WorldHash + CurrentHash;
          }

          if (Chunk == LastChunkOfSameHashValue)
          {
            FreeWorldChunk(World, *Chunk, MeshFreelist, Memory);
            *Chunk = 0;
          }
          else
          {
            FreeWorldChunk(World, *Chunk, MeshFreelist, Memory);
            *Chunk = *LastChunkOfSameHashValue;
            *LastChunkOfSameHashValue = 0;
            --ChunkIndex;
          }

        }
        else
        {
          SetFlag(*Chunk, Chunk_Garbage);
        }

      }
    }
  }

  /* Print(ChunksPresentInHashtable); */
  /* if(ChunksCollected) */
  /* { */
  /*   Print(ChunksCollected); */
  /* } */

  return;
}

function inline b32
IsFilledInWorld( world *World, world_chunk *chunk, canonical_position VoxelP )
{
  /* TIMED_FUNCTION(); */
  b32 isFilled = true;

  if ( chunk )
  {
    world_chunk *localChunk = chunk;

    if ( chunk->WorldP != VoxelP.WorldP )
    {
      localChunk = GetWorldChunk(World, VoxelP.WorldP);
    }

    isFilled = localChunk && IsFilledInChunk(localChunk->Data, Voxel_Position(VoxelP.Offset), World->ChunkDim );
  }

  return isFilled;
}

inline b32
NotFilledInWorld( world *World, world_chunk *chunk, canonical_position VoxelP )
{
  /* TIMED_FUNCTION(); */
  b32 Result = !(IsFilledInWorld(World, chunk, VoxelP));
  return Result;
}

function void
CopyChunkOffset(world_chunk *Src, voxel_position SrcChunkDim, world_chunk *Dest, voxel_position DestChunkDim, voxel_position Offset)
{
  TIMED_FUNCTION();
  for ( s32 z = 0; z < DestChunkDim.z; ++ z)
  {
    for ( s32 y = 0; y < DestChunkDim.y; ++ y)
    {
      for ( s32 x = 0; x < DestChunkDim.x; ++ x)
      {
        s32 DestIndex = GetIndex(Voxel_Position(x,y,z), DestChunkDim);
        s32 SynIndex = GetIndex(Voxel_Position(x,y,z) + Offset, SrcChunkDim);

#if 1
        Dest->Data->Voxels[DestIndex] = Src->Data->Voxels[SynIndex];
#else
        voxel vSrc = Src->Data->Voxels[SynIndex];
        voxel *vDest = Dest->Data->Voxels[DestIndex];
        vDest = vSrc;
#endif

        Dest->FilledCount += Dest->Data->Voxels[DestIndex].Flags & Voxel_Filled;
        CAssert(Voxel_Filled == 1);
      }
    }
  }


  return;
}

function void
InitChunkPlane(s32 zIndex, world_chunk *Chunk, chunk_dimension ChunkDim, u8 Color )
{
  for ( s32 z = 0; z < ChunkDim.z; ++ z)
  {
    for ( s32 y = 0; y < ChunkDim.y; ++ y)
    {
      for ( s32 x = 0; x < ChunkDim.x; ++ x)
      {
        if (z == zIndex)
        {
          s32 Index = GetIndex(Voxel_Position(x,y,z), ChunkDim);
          Chunk->Data->Voxels[Index].Flags = Voxel_Filled;
          Chunk->Data->Voxels[Index].Color = Color;
        }
      }
    }
  }

  return;
}

function u32
InitChunkPerlinPlane(perlin_noise *Noise, world_chunk *WorldChunk, chunk_dimension Dim, u8 ColorIndex, u32 Amplitude, s64 zMin)
{
  TIMED_FUNCTION();

  u32 ChunkSum = 0;

  Assert(WorldChunk);

  chunk_data *ChunkData = WorldChunk->Data;
  for ( s32 z = 0; z < Dim.z; ++ z)
  {
    for ( s32 y = 0; y < Dim.y; ++ y)
    {
      for ( s32 x = 0; x < Dim.x; ++ x)
      {
        s32 VoxIndex = GetIndex(Voxel_Position(x,y,z), Dim);
        ChunkData->Voxels[VoxIndex].Flags = Voxel_Empty;
        Assert( NotSet(&ChunkData->Voxels[VoxIndex], Voxel_Filled) );

        double InX = ((double)x + ( (double)WORLD_CHUNK_DIM.x*(double)WorldChunk->WorldP.x))/NOISE_FREQUENCY;
        double InY = ((double)y + ( (double)WORLD_CHUNK_DIM.y*(double)WorldChunk->WorldP.y))/NOISE_FREQUENCY;
        double InZ = 1.0;

        s64 zAbsolute = z - (zMin-Amplitude) + (WORLD_CHUNK_DIM.z*WorldChunk->WorldP.z);
        r64 zSlicesAt = (1.0/(r64)Amplitude) * (r64)zAbsolute;

        r64 NoiseValue = Noise->noise(InX, InY, InZ);

        b32 NoiseChoice = NoiseValue > zSlicesAt ? True : False;
        SetFlag(&ChunkData->Voxels[VoxIndex], (voxel_flag)(NoiseChoice * Voxel_Filled));

        if (NoiseChoice)
        {
          ChunkData->Voxels[VoxIndex].Color = ColorIndex;
          ++ChunkSum;
          Assert( IsSet(&ChunkData->Voxels[VoxIndex], Voxel_Filled) );
        }
        else
        {
          Assert( NotSet(&ChunkData->Voxels[VoxIndex], Voxel_Filled) );
        }
      }
    }
  }

  return ChunkSum;
}

function void
InitChunkPerlin(perlin_noise *Noise, world_chunk *WorldChunk, chunk_dimension Dim, u8 ColorIndex)
{
  TIMED_FUNCTION();

  Assert(WorldChunk);

  chunk_data *ChunkData = WorldChunk->Data;
  for ( s32 z = 0; z < Dim.z; ++ z)
  {
    for ( s32 y = 0; y < Dim.y; ++ y)
    {
      for ( s32 x = 0; x < Dim.x; ++ x)
      {
        s32 i = GetIndex(Voxel_Position(x,y,z), Dim);
        ChunkData->Voxels[i].Flags = Voxel_Empty;

        Assert( NotSet(&ChunkData->Voxels[i], Voxel_Filled) );

        double InX = ((double)x + ( (double)WORLD_CHUNK_DIM.x*(double)WorldChunk->WorldP.x))/NOISE_FREQUENCY;
        double InY = ((double)y + ( (double)WORLD_CHUNK_DIM.y*(double)WorldChunk->WorldP.y))/NOISE_FREQUENCY;
        double InZ = ((double)z + ( (double)WORLD_CHUNK_DIM.z*(double)WorldChunk->WorldP.z))/NOISE_FREQUENCY;

        r32 noiseValue = (r32)Noise->noise(InX, InY, InZ);

        s32 NoiseChoice = Floori(noiseValue + 0.5f);

        Assert(NoiseChoice == 0 || NoiseChoice == 1);

        SetFlag(&ChunkData->Voxels[i], (voxel_flag)(NoiseChoice * Voxel_Filled));

        if (NoiseChoice)
        {
          ChunkData->Voxels[i].Color = ColorIndex;
          Assert( IsSet(&ChunkData->Voxels[i], Voxel_Filled) );
        }
        else
        {
          Assert( NotSet(&ChunkData->Voxels[i], Voxel_Filled) );
        }

      }
    }
  }

  return;
}

function void
BuildWorldChunkMesh(world_chunk *ReadChunk, chunk_dimension ReadChunkDim,
                    world_chunk *WriteChunk, chunk_dimension WriteChunkDim,
                    untextured_3d_geometry_buffer* DestGeometry)
{
  TIMED_FUNCTION();

  chunk_data *WriteChunkData = WriteChunk->Data;
  chunk_data *ReadChunkData = ReadChunk->Data;

  Assert(IsSet(ReadChunk, Chunk_Initialized));
  Assert(IsSet(WriteChunk, Chunk_Initialized));

  voxel_position rightVoxel;
  voxel_position leftVoxel;
  voxel_position topVoxel;
  voxel_position botVoxel;
  voxel_position frontVoxel;
  voxel_position backVoxel;

  s32 rightVoxelReadIndex;
  s32 leftVoxelReadIndex;
  s32 topVoxelReadIndex;
  s32 botVoxelReadIndex;
  s32 frontVoxelReadIndex;
  s32 backVoxelReadIndex;

  random_series ColorEntropy = {33453};

  for ( s32 z = 0; z < WriteChunkDim.z ; ++z )
  {
    for ( s32 y = 0; y < WriteChunkDim.y ; ++y )
    {
      for ( s32 x = 0; x < WriteChunkDim.x ; ++x )
      {
        voxel_position CurrentP  = Voxel_Position(x,y,z);

        /* v4 Perturb = 0.08f*V4(RandomBilateral(&ColorEntropy), */
        /*                       RandomBilateral(&ColorEntropy), */
        /*                       RandomBilateral(&ColorEntropy), */
        /*                       1.0f); */

        if ( NotFilledInChunk( WriteChunkData, CurrentP, WriteChunkDim ) )
          continue;

        v3 Diameter = V3(1.0f);
        v3 VertexData[FACE_VERT_COUNT];
        v4 FaceColors[FACE_VERT_COUNT];

        voxel *Voxel = &WriteChunkData->Voxels[GetIndex(CurrentP, WriteChunkDim)];
        FillColorArray(Voxel->Color, FaceColors, FACE_VERT_COUNT);
#if 0
        for (u32 ColorIndex = 0;
            ColorIndex < FACE_VERT_COUNT;
            ++ColorIndex)
        {
          FaceColors[ColorIndex] += Perturb*FaceColors[0];
        }
#endif

        rightVoxel = CurrentP + Voxel_Position(1, 0, 0);
        rightVoxelReadIndex = GetIndex(rightVoxel+1, ReadChunkDim);

        leftVoxel  = CurrentP - Voxel_Position(1, 0, 0);
        leftVoxelReadIndex  = GetIndex(leftVoxel+1, ReadChunkDim);

        topVoxel   = CurrentP + Voxel_Position(0, 0, 1);
        topVoxelReadIndex   = GetIndex(topVoxel+1, ReadChunkDim);

        botVoxel   = CurrentP - Voxel_Position(0, 0, 1);
        botVoxelReadIndex   = GetIndex(botVoxel+1, ReadChunkDim);

        frontVoxel = CurrentP + Voxel_Position(0, 1, 0);
        frontVoxelReadIndex = GetIndex(frontVoxel+1, ReadChunkDim);

        backVoxel  = CurrentP - Voxel_Position(0, 1, 0);
        backVoxelReadIndex  = GetIndex(backVoxel+1, ReadChunkDim);

        if ( NotFilledInChunk( ReadChunkData, rightVoxelReadIndex) )
        {
          RightFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, RightFaceNormalData, FaceColors);
        }
        if ( NotFilledInChunk( ReadChunkData, leftVoxelReadIndex) )
        {
          LeftFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, LeftFaceNormalData, FaceColors);
        }
        if ( NotFilledInChunk( ReadChunkData, botVoxelReadIndex) )
        {
          BottomFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, BottomFaceNormalData, FaceColors);
        }
        if ( NotFilledInChunk( ReadChunkData, topVoxelReadIndex) )
        {
          TopFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, TopFaceNormalData, FaceColors);
        }
        if ( NotFilledInChunk( ReadChunkData, frontVoxelReadIndex) )
        {
          FrontFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, FrontFaceNormalData, FaceColors);
        }
        if ( NotFilledInChunk( ReadChunkData, backVoxelReadIndex) )
        {
          BackFaceVertexData( V3(CurrentP), Diameter, VertexData);
          BufferVertsDirect(DestGeometry, 6, VertexData, BackFaceNormalData, FaceColors);
        }
      }
    }
  }

  return;
}

function void
BuildWorldChunkMesh(world *World, world_chunk *WorldChunk, chunk_dimension WorldChunkDim, untextured_3d_geometry_buffer* DestMesh)
{
  TIMED_FUNCTION();

  chunk_data *Chunk = WorldChunk->Data;

  Assert(IsSet(Chunk, Chunk_Initialized));

  Assert(NotSet(Chunk, Chunk_Queued));

  canonical_position rightVoxel;
  canonical_position leftVoxel;
  canonical_position topVoxel;
  canonical_position botVoxel;
  canonical_position frontVoxel;
  canonical_position backVoxel;

  random_series ColorEntropy = {33453};

  for ( s32 z = 0; z < WorldChunkDim.z ; ++z )
  {
    for ( s32 y = 0; y < WorldChunkDim.y ; ++y )
    {
      for ( s32 x = 0; x < WorldChunkDim.x ; ++x )
      {
        canonical_position CurrentP  = Canonical_Position(WorldChunkDim, V3(x,y,z), WorldChunk->WorldP);

        v4 Perturb = 0.08f*V4(RandomBilateral(&ColorEntropy),
                              RandomBilateral(&ColorEntropy),
                              RandomBilateral(&ColorEntropy),
                              1.0f);

        if ( !IsFilledInWorld( World, WorldChunk, CurrentP ) )
          continue;

        voxel *Voxel = &Chunk->Voxels[GetIndex(CurrentP.Offset, WorldChunkDim)];

        v3 Diameter = V3(1.0f);
        v3 VertexData[FACE_VERT_COUNT];
        v4 FaceColors[FACE_VERT_COUNT];
        FillColorArray(Voxel->Color, FaceColors, FACE_VERT_COUNT);


        for (u32 ColorIndex = 0;
            ColorIndex < FACE_VERT_COUNT;
            ++ColorIndex)
        {
          FaceColors[ColorIndex] += Perturb*FaceColors[0];
        }

        TIMED_BLOCK("Canonicalize");
          rightVoxel = Canonicalize(WorldChunkDim, CurrentP + V3(1, 0, 0));
          leftVoxel  = Canonicalize(WorldChunkDim, CurrentP - V3(1, 0, 0));
          topVoxel   = Canonicalize(WorldChunkDim, CurrentP + V3(0, 0, 1));
          botVoxel   = Canonicalize(WorldChunkDim, CurrentP - V3(0, 0, 1));
          frontVoxel = Canonicalize(WorldChunkDim, CurrentP + V3(0, 1, 0));
          backVoxel  = Canonicalize(WorldChunkDim, CurrentP - V3(0, 1, 0));
        END_BLOCK("Canonicalize");

        // FIXME(Jesse): This should use a BufferVertsChecked path
        if ( !IsFilledInWorld( World, WorldChunk, rightVoxel ) )
        {
          RightFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, RightFaceNormalData, FaceColors);
        }
        if ( !IsFilledInWorld( World, WorldChunk, leftVoxel ) )
        {
          LeftFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, LeftFaceNormalData, FaceColors);
        }
        if ( !IsFilledInWorld( World, WorldChunk, botVoxel   ) )
        {
          BottomFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, BottomFaceNormalData, FaceColors);
        }
        if ( !IsFilledInWorld( World, WorldChunk, topVoxel   ) )
        {
          TopFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, TopFaceNormalData, FaceColors);
        }
        if ( !IsFilledInWorld( World, WorldChunk, frontVoxel ) )
        {
          FrontFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, FrontFaceNormalData, FaceColors);
        }
        if ( !IsFilledInWorld( World, WorldChunk, backVoxel  ) )
        {
          BackFaceVertexData( CurrentP.Offset, Diameter, VertexData);
          BufferVertsDirect(DestMesh, 6, VertexData, BackFaceNormalData, FaceColors);
        }
      }
    }
  }
}

function void
InitializeWorldChunkPerlin(perlin_noise *Noise,
                           world_chunk *DestChunk, untextured_3d_geometry_buffer* DestGeometry,
                           memory_arena *TempMemory)
{
  TIMED_FUNCTION();
  Assert( IsSet(DestChunk, Chunk_Queued) );

  if (IsSet(DestChunk, Chunk_Garbage))
  {
    DestChunk->Data->Flags = Chunk_Collected;
    return;
  }

#if 0
  // Don't blow out the Flags for this chunk or risk assertions on other
  // threads that rely on that flag being set for every item on the queue
  ZeroChunk(DestChunk->Data, Volume(WORLD_CHUNK_DIM));
#else
  for ( s32 VoxelIndex = 0;
        VoxelIndex < Volume(WORLD_CHUNK_DIM);
        ++VoxelIndex)
  {
    voxel *Voxel = &DestChunk->Data->Voxels[VoxelIndex];
    Voxel->Flags = Voxel_Empty;
    Voxel->Color = 0;
  }
#endif

  chunk_dimension SynChunkDim = WORLD_CHUNK_DIM + 2;
  chunk_dimension SynChunkP = DestChunk->WorldP - 1;

  world_chunk *SyntheticChunk = AllocateWorldChunk(TempMemory, SynChunkP, SynChunkDim );

  InitChunkPerlin(Noise, SyntheticChunk, SynChunkDim, GRASS_GREEN);
  CopyChunkOffset(SyntheticChunk, SynChunkDim, DestChunk, WORLD_CHUNK_DIM, Voxel_Position(1));

  SetFlag(DestChunk, Chunk_Initialized);
  SetFlag(SyntheticChunk, Chunk_Initialized);

  BuildWorldChunkMesh(SyntheticChunk, SynChunkDim, DestChunk,  WORLD_CHUNK_DIM, DestGeometry);
  DestChunk->Mesh = DestGeometry;

  DestChunk->Data->Flags = Chunk_MeshComplete;

  return;
}

function void
InitializeWorldChunkPlane(world_chunk *DestChunk, untextured_3d_geometry_buffer* DestGeometry, memory_arena* TempMemory)
{
  TIMED_FUNCTION();
  Assert( IsSet(DestChunk, Chunk_Queued) );

  if (IsSet(DestChunk, Chunk_Garbage))
  {
    DestChunk->Data->Flags = Chunk_Collected;
    return;
  }

#if 0
  // Don't blow out the Flags for this chunk or risk assertions on other
  // threads that rely on that flag being set for every item on the queue
  ZeroChunk(DestChunk->Data, Volume(WORLD_CHUNK_DIM));
#else
  for ( s32 VoxelIndex = 0;
        VoxelIndex < Volume(WORLD_CHUNK_DIM);
        ++VoxelIndex)
  {
    voxel *Voxel = &DestChunk->Data->Voxels[VoxelIndex];
    Voxel->Flags = Voxel_Empty;
    Voxel->Color = 0;
  }
#endif

  chunk_dimension SynChunkDim = WORLD_CHUNK_DIM + 2;
  chunk_dimension SynChunkP = DestChunk->WorldP - 1;

  world_chunk *SyntheticChunk = AllocateWorldChunk(TempMemory, SynChunkP, SynChunkDim );

  InitChunkPlane(1, SyntheticChunk, SynChunkDim, GREEN);
  CopyChunkOffset(SyntheticChunk, SynChunkDim, DestChunk, WORLD_CHUNK_DIM, Voxel_Position(1));

  SetFlag(DestChunk, Chunk_Initialized);
  SetFlag(SyntheticChunk, Chunk_Initialized);

  BuildWorldChunkMesh(SyntheticChunk, SynChunkDim, DestChunk, WORLD_CHUNK_DIM, DestGeometry);

  DestChunk->Data->Flags = Chunk_MeshComplete;

  return;
}

function inline untextured_3d_geometry_buffer*
GetMeshForChunk(mesh_freelist* Freelist, memory_arena* PermMemory)
{
  free_mesh* MeshContainer = Unlink_TS(&Freelist->FirstFree);
  untextured_3d_geometry_buffer* Result = 0;

  if (MeshContainer)
  {
    Result = MeshContainer->Mesh;
    Assert(Result);

    MeshContainer->Mesh = 0;
    FullBarrier;
    Link_TS(&Freelist->Containers, MeshContainer);
  }
  else
  {
    Result = AllocateMesh(PermMemory, (u32)Kilobytes(64));
    Assert(Result);
  }

  return Result;
}

function void
ClipAndDisplaceToMinDim(untextured_3d_geometry_buffer* Buffer, v3 Min, v3 Dim)
{
  v3 Max = Min+Dim;
  for (u32 VertIndex = 0;
      VertIndex < Buffer->At;
      ++VertIndex)
  {
    v3* Vert = Buffer->Verts + VertIndex;
    for (u32 AxisIndex = 0;
        AxisIndex < 3;
        ++AxisIndex)
    {
      if (Vert->E[AxisIndex] > Max.E[AxisIndex])
      {
        Vert->E[AxisIndex] = Dim.E[AxisIndex];
      }
      else if (Vert->E[AxisIndex] < Min.E[AxisIndex])
      {
        Vert->E[AxisIndex] = 0;
      }
      else
      {
        Vert->E[AxisIndex] -= Min.E[AxisIndex];
      }
    }
  }
}

function void
FindEdgeIntersections(point_buffer* Dest, chunk_data* ChunkData, chunk_dimension ChunkDim)
{
  {
    voxel_position Start = Voxel_Position(0, 0, 0);

    {
      voxel_position Iter  = Voxel_Position(1, 0, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 1, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0, 1);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
  }

  {
    voxel_position Start = Voxel_Position(0, ChunkDim.y-1, ChunkDim.z-1);

    {
      voxel_position Iter  = Voxel_Position(1, 0, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0,-1, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0,-1);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
  }

  {
    voxel_position Start = Voxel_Position(ChunkDim.x-1, ChunkDim.y-1, 0);

    {
      voxel_position Iter  = Voxel_Position(-1, 0, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0,-1, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0, 1);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
  }

  {
    voxel_position Start = Voxel_Position(ChunkDim.x-1, 0, ChunkDim.z-1);

    {
      voxel_position Iter  = Voxel_Position(-1, 0, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 1, 0);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0,-1);
      FindBoundaryVoxelsAlongEdge(ChunkData, ChunkDim, Start, Iter, Dest);
    }
  }

  return;
}

function b32
VertsAreCoplanar(voxel_position* V1, voxel_position* V2)
{
  b32 Result = ( V1->x == V2->x && (V1->x == 0 || V1->x == WORLD_CHUNK_DIM.x) ) ||
               ( V1->y == V2->y && (V1->y == 0 || V1->y == WORLD_CHUNK_DIM.y) ) ||
               ( V1->z == V2->z && (V1->z == 0 || V1->z == WORLD_CHUNK_DIM.z) )  ;
  return Result;
}

function b32
VertsAreNotCoplanar(voxel_position* V1, voxel_position* V2)
{
  b32 Result = !VertsAreCoplanar(V1, V2);
  return Result;
}

function voxel_position*
GetPrevCoplanarVertex(voxel_position* Query, point_buffer* Points)
{
  voxel_position* Result = 0;

  voxel_position* Current = Query + 1;
  voxel_position* Last = Points->Points + Points->Count-1;

  while (Current != Query)
  {
    if (Current < Points->Points) { Current = Last; }

    if ( VertsAreCoplanar(Current, Query) )
    {
      Result = Current;
      break;
    }

    --Current;
  }

  return Result;
}

function voxel_position*
GetNextCoplanarVertex(voxel_position* Query, point_buffer* Points)
{
  voxel_position* Result = 0;

  voxel_position* Current = Query + 1;
  voxel_position* OnePastLast = Points->Points + Points->Count;

  while (Current != Query)
  {
    if (Current == OnePastLast) { Current = Points->Points; }

    if ( VertsAreCoplanar(Current, Query) )
    {
      Result = Current;
      break;
    }

    ++Current;
  }

  return Result;
}

function voxel_position*
GetClosestCoplanarVertex(voxel_position* Query, point_buffer* Points)
{
  r32 ShortestLength = FLT_MAX;
  voxel_position* Result = 0;

  voxel_position* Current = Points->Points;
  voxel_position* OnePastLast = Points->Points + Points->Count;

  while (Current != Query)
  {
    if (Current == OnePastLast) { Current = Points->Points; }

    r32 CurrentLength = LengthSq(*Query - *Current);
    if ( VertsAreCoplanar(Current, Query) &&
         CurrentLength < ShortestLength )
    {
      ShortestLength = CurrentLength;
      Result = Current;
    }

    ++Current;
  }

  return Result;
}

#define MAX_COPLANAR_VERT_COUNT 32
function u32
GetAllCoplanarVerts(voxel_position* Query, point_buffer* Points, voxel_position* Results)
{
  u32 Count = 0;
  voxel_position* Current = Points->Points;
  voxel_position* OnePastLast = Points->Points + Points->Count;

  while (Current != OnePastLast)
  {
    Assert(Count < MAX_COPLANAR_VERT_COUNT);
    if (Current == Query) { ++Current; continue; }
    if ( VertsAreCoplanar(Current, Query) ) { Results[Count++] = *Current; }

    ++Current;
  }

  return Count;
}

function voxel_position*
GetClosestAngularDistanceTo(voxel_position* Query, point_buffer* Points)
{
  voxel_position* Result = 0;
  voxel_position* Current = Points->Points;
  voxel_position* OnePastLast = Points->Points + Points->Count;

  r32 ClosestTheta = FLT_MAX;
  for ( ; Current < OnePastLast; ++Current)
  {
    if (Current == Query) { continue; }

    r32 CosTheta = ArcCos(Dot(Normalize(*Query), Normalize(*Current)));

    if (CosTheta < ClosestTheta)
    {
      ClosestTheta = CosTheta;
      Result = Current;
    }

    continue;
  }

  return Result;
}

function b32
TrianglesAreEqual(triangle* T1, triangle* T2)
{
  b32 Result = (
    ( T1->Points[0] == T2->Points[0] || T1->Points[0] == T2->Points[1] || T1->Points[0] == T2->Points[2] ) &&
    ( T1->Points[1] == T2->Points[0] || T1->Points[1] == T2->Points[1] || T1->Points[1] == T2->Points[2] ) &&
    ( T1->Points[2] == T2->Points[0] || T1->Points[2] == T2->Points[1] || T1->Points[2] == T2->Points[2] ) );

  return Result;
}

function b32
TriangleIsUniqueInSet(triangle* Query, triangle** Set, u32 SetCount)
{
  b32 Result = True;

  for (u32 TestIndex = 0;
      TestIndex < SetCount;
      ++TestIndex)
  {
    triangle* Test = Set[TestIndex];

    if (TrianglesAreEqual(Query, Test))
    {
      Result = False;
      break;
    }
  }



  return Result;
}

function void
InitializeWorldChunkPerlinPlane(thread_local_state *Thread,
                                world_chunk *DestChunk, s32 Amplititude, s32 zMin)
{
  TIMED_FUNCTION();
#if 0
  // Don't blow out the Flags for this chunk or risk assertions on other
  // threads that rely on that flag being set for every item on the queue
  ZeroChunk(DestChunk->Data, Volume(WORLD_CHUNK_DIM));
#else
  for ( s32 VoxelIndex = 0;
        VoxelIndex < Volume(WORLD_CHUNK_DIM);
        ++VoxelIndex)
  {
    voxel *Voxel = &DestChunk->Data->Voxels[VoxelIndex];
    Voxel->Flags = Voxel_Empty;
    Voxel->Color = 0;
  }
#endif

  chunk_dimension SynChunkDim = WORLD_CHUNK_DIM + 2;
  chunk_dimension SynChunkP = DestChunk->WorldP - 1;

  world_chunk *SyntheticChunk = AllocateWorldChunk(Thread->TempMemory, SynChunkP, SynChunkDim );

  u32 SyntheticChunkSum = InitChunkPerlinPlane(Thread->Noise, SyntheticChunk, SynChunkDim, GRASS_GREEN, Amplititude, zMin);
  CopyChunkOffset(SyntheticChunk, SynChunkDim, DestChunk, WORLD_CHUNK_DIM, Voxel_Position(1));

  SetFlag(DestChunk, Chunk_Initialized);
  SetFlag(SyntheticChunk, Chunk_Initialized);

  if (SyntheticChunkSum > 0 && SyntheticChunkSum < (u32)Volume(SynChunkDim))
  {
    Assert(!DestChunk->Mesh);
    DestChunk->Mesh = GetMeshForChunk(Thread->MeshFreelist, Thread->PermMemory);
    BuildWorldChunkMesh(SyntheticChunk, SynChunkDim, DestChunk, WORLD_CHUNK_DIM, DestChunk->Mesh);
  }


  { // Compute 0th LOD

    chunk_dimension NewSynChunkDim = WORLD_CHUNK_DIM+Voxel_Position(1);
    CopyChunkOffset(SyntheticChunk, SynChunkDim, SyntheticChunk, NewSynChunkDim, Voxel_Position(1));
    SynChunkDim = NewSynChunkDim;

    point_buffer TempBuffer = {};
    point_buffer *EdgeBoundaryVoxels = &TempBuffer;

    TempBuffer.Min = Voxel_Position(INT_MAX);
    TempBuffer.Max = Voxel_Position(INT_MIN);

    /* FindEdgeIntersections(EdgeBoundaryVoxels, DestChunk->Data, WORLD_CHUNK_DIM); */
    FindEdgeIntersections(EdgeBoundaryVoxels, SyntheticChunk->Data, NewSynChunkDim);

    // Find closest bounding point to the midpoint of the bounding volume
    u32 WorldChunkVolume = Volume(WORLD_CHUNK_DIM);
    boundary_voxels* BoundingPoints = AllocateBoundaryVoxels(WorldChunkVolume, Thread->TempMemory);
    GetBoundingVoxels(DestChunk, BoundingPoints);

    voxel_position BoundingVoxelMidpoint = EdgeBoundaryVoxels->Min + ((EdgeBoundaryVoxels->Max - EdgeBoundaryVoxels->Min)/2.0f);

    voxel_position FoundCenterPoint = BoundingVoxelMidpoint;
    r32 ShortestLength = FLT_MAX;
    for ( u32 PointIndex = 0;
          PointIndex < BoundingPoints->At;
          ++PointIndex)
    {
      voxel_position* TestP = BoundingPoints->Points + PointIndex;
      /* DrawVoxel(DestChunk->LodMesh, V3(*TestP), RED, V3(0.25f)); */

      r32 TestLength = LengthSq(V3(*TestP) - BoundingVoxelMidpoint);
      if  (TestLength < ShortestLength)
      {
        ShortestLength = TestLength;
        FoundCenterPoint = *TestP;
      }
    }

#if 1
    random_series Entropy = {6543};
    for ( s32 PointIndex = 0;
          PointIndex < EdgeBoundaryVoxels->Count;
          ++PointIndex )
    {
      DrawVoxel( DestChunk->LodMesh,
                 V3(EdgeBoundaryVoxels->Points[PointIndex]),
                 RandomU32(&Entropy) % (ArrayCount(DefaultPalette)-1),
                 V3(0.6f));
    }
    ClipAndDisplaceToMinDim(DestChunk->LodMesh, V3(0), V3(WORLD_CHUNK_DIM) );
#endif

    if (EdgeBoundaryVoxels->Count)
    {
      /* DEBUG_DrawAABB(DestChunk->LodMesh, V3(0), V3(WORLD_CHUNK_DIM), PINK); */
      DrawVoxel(DestChunk->LodMesh, V3(FoundCenterPoint), PINK, V3(0.35f));

#if 1
      const u32 MaxTriangles = 128;
      u32 TriangleCount = 0;
      triangle* Triangles[MaxTriangles];

      if (EdgeBoundaryVoxels->Count)
      {
        voxel_position* CurrentVert = EdgeBoundaryVoxels->Points;
        voxel_position* OnePastLastVert = EdgeBoundaryVoxels->Points + EdgeBoundaryVoxels->Count;

        voxel_position FirstVert = *CurrentVert;

        while (EdgeBoundaryVoxels->Count > 1)
        {
          voxel_position* LowestAngleBetween = GetClosestAngularDistanceTo(CurrentVert, EdgeBoundaryVoxels);

          if (LowestAngleBetween)
          {
            triangle* TestTriangle = Triangle(&FoundCenterPoint, CurrentVert, LowestAngleBetween, Thread->TempMemory);

            /* Assert( TriangleIsUniqueInSet(TestTriangle, Triangles, TriangleCount) ); */
            Assert(TriangleCount < MaxTriangles);

            Triangles[TriangleCount++] = TestTriangle;

            OnePastLastVert--;
            EdgeBoundaryVoxels->Count--;

            *CurrentVert = *OnePastLastVert;

            CurrentVert = LowestAngleBetween;

          }
          else
          {
            break;
          }
        }

        voxel_position* LastVert = EdgeBoundaryVoxels->Points;

        triangle* TestTriangle = Triangle(&FoundCenterPoint, &FirstVert, LastVert, Thread->TempMemory);

        /* Assert( TriangleIsUniqueInSet(TestTriangle, Triangles, TriangleCount) ); */
        Assert(TriangleCount < MaxTriangles);

        Triangles[TriangleCount++] = TestTriangle;
      }

      u32 Color = 8;
      for (u32 TriIndex  = 0;
          TriIndex < TriangleCount;
          ++TriIndex)
      {
        BufferTriangle(DestChunk->LodMesh, Triangles[TriIndex], V3(0,0,1), Color);
        ++Color;
      }
      /* Print(TriangleCount); */
#endif


#if 0
      // Draw
      if (EdgeBoundaryVoxels->Count)
      {
        v3 Verts[3] = {};

        Verts[0] = FoundCenterPoint;

        s32 Color = 42;
        s32 VertIndex = 0;
        while ( (VertIndex+1) < EdgeBoundaryVoxels->Count )
        {
          Verts[1] = V3(EdgeBoundaryVoxels->Points[VertIndex]);
          Verts[2] = V3(EdgeBoundaryVoxels->Points[VertIndex + 1]);
          BufferTriangle(DestChunk->LodMesh, Verts, V3(0,0,1), Color);
          ++VertIndex;
          Color += 10;
        }

        Verts[1] = V3(EdgeBoundaryVoxels->Points[VertIndex]);
        Verts[2] = V3(EdgeBoundaryVoxels->Points[0]);
        BufferTriangle(DestChunk->LodMesh, Verts, V3(0,0,1), Color);
      }

#endif
    }
  }

  DestChunk->LodMesh_Complete = True;
  DestChunk->Data->Flags = Chunk_MeshComplete;

  return;
}

function void
InitializeWorldChunkEmpty(world_chunk *DestChunk)
{
  TIMED_FUNCTION();
  Assert( IsSet(DestChunk, Chunk_Queued) );

  if (IsSet(DestChunk, Chunk_Garbage))
  {
    DestChunk->Data->Flags = Chunk_Collected;
    return;
  }

#if 0
  // Don't blow out the Flags for this chunk or risk assertions on other
  // threads that rely on that flag being set for every item on the queue
  ZeroChunk(DestChunk->Data, Volume(WORLD_CHUNK_DIM));
#else
  for ( s32 VoxelIndex = 0;
        VoxelIndex < Volume(WORLD_CHUNK_DIM);
        ++VoxelIndex)
  {
    voxel *Voxel = &DestChunk->Data->Voxels[VoxelIndex];
    Voxel->Flags = Voxel_Empty;
    Voxel->Color = 0;
  }
#endif

  DestChunk->Data->Flags = Chunk_MeshComplete;

  return;
}
