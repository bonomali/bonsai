#ifndef PLAYGROUNDH
#define PLAYGROUNDH

#include <perlin.h>
#include <stdio.h>
#include <types.h>
#include <math.h>

#define ArrayCount(a) (sizeof(a)/sizeof(a[0]))

#define InvalidDefaultCase default: { assert(false); break; }

#define Print(Pos) \
  Print_P( Pos, #Pos )

inline void
Print_P( canonical_position P, const char* name)
{
  printf(" -- %s\n", name);
  printf(" Offset: %f %f %f \n", P.Offset.x, P.Offset.y, P.Offset.z );
  printf(" WorldP: %d %d %d \n", P.WorldP.x, P.WorldP.y, P.WorldP.z );
}

inline void
Print_P( voxel_position P, const char* name)
{
  printf(" %s %d %d %d \n", name, P.x, P.y, P.z );
}

inline void
Print_P( v3 P, const char* name)
{
  printf(" %s %f %f %f \n", name, P.x, P.y, P.z );
}

inline void
Print_P( glm::vec3 P, const char* name)
{
  printf(" %s %f %f %f \n", name, P.x, P.y, P.z );
}

inline int
UnSetFlag( int Flags, int Flag )
{
  int Result = Flags &= ~Flag;
  return Result;
}

inline int
SetFlag( int Flags, int Flag )
{
  int Result = Flags |= Flag;
  return Result;
}

inline bool
IsSet( int Flags, int Flag )
{
  bool Result = ( (Flags & Flag) != 0 );
  return Result;
}

inline bool
NotSet( int Flags, int Flag )
{
  bool Result = !(IsSet(Flags, Flag));
  return Result;
}

enum ChunkFlags {
  Chunk_Uninitialized = 1 << 0,

  Chunk_Entity        = 1 << 1,
  Chunk_World         = 1 << 2
};


enum VoxelFlags {
  Voxel_Filled    = 1 << 0,

  Voxel_Yellow    = 1 << 1,
  Voxel_Red       = 1 << 2,
  Voxel_Green     = 1 << 3,
  Voxel_Teal      = 1 << 4,
  Voxel_White     = 1 << 5,
  Voxel_Purple    = 1 << 6
};

struct Voxel
{
  v3 Offset;
  int flags;
};

struct VertexBlock
{
  GLfloat *Data;
  int bytesAllocd;
  int filled;
};

struct Chunk
{
  Voxel *Voxels;

  Voxel *BoundaryVoxels;
  int BoundaryVoxelCount;

  chunk_dimension Dim;

  // Position in absolute world coordinates.  A chunk is one world coordinate
  voxel_position WorldP;

  // Position within the chunk this is contained in, if applicable
  v3 Offset;

  int flags;
};

struct ChunkStack
{
  Chunk *chunks; // This should be Volume(VisibleRegion) chunks
  int count = 0;
};

Chunk
PopChunkStack(ChunkStack *stack)
{
  Chunk Result = stack->chunks[--stack->count];

  assert(stack->count >= 0);
  return Result;
};

void
PushChunkStack(ChunkStack *stack, Chunk chunk)
{
  assert(stack->count + 1 < CHUNK_STACK_SIZE);

  stack->chunks[stack->count++] = chunk;
  return;
};

struct Frustum
{
  float farClip;
  float nearClip;
  float FOV;
};

struct Camera_Object
{
  Frustum Frust;

  canonical_position P;
  canonical_position Target; // TODO : Can this just be a v3?

  v3 Front;
};

struct World
{
  Chunk *Chunks;

  ChunkStack FreeChunks;

  // This is the number of chunks in xyz we're going to update and render
  chunk_dimension VisibleRegion;

  chunk_dimension ChunkDim;

  world_position VisibleRegionOrigin;

  PerlinNoise Noise;

  v3 Gravity;

  VertexBlock VertexData;
  VertexBlock ColorData;
  VertexBlock NormalData;

  int VertexCount; // How many verticies are we drawing
};

struct Entity
{
  Chunk Model;
  v3 Velocity;
  v3 Acceleration;
};

struct collision_event
{
  canonical_position CP;
  bool didCollide;
};



Chunk*
GetWorldChunk( World *world, world_position WorldP )
{
  Chunk *Result;

  if (
    WorldP.x < 0 ||
    WorldP.x >= world->VisibleRegion.x ||

    WorldP.y < 0 ||
    WorldP.y >= world->VisibleRegion.y ||

    WorldP.z < 0 ||
    WorldP.z >= world->VisibleRegion.z )
  {
    /* assert(false); // Requesting outside the initialized world; no bueno? */
    return nullptr;
  }

  int i =
    WorldP.x +
    (WorldP.y * world->VisibleRegion.x) +
    (WorldP.z * world->VisibleRegion.x * world->VisibleRegion.y);

  Result = &world->Chunks[i];

  assert( Result->WorldP == WorldP );

  return Result;
}

inline bool
IsFacingPoint( v3 FaceToPoint, v3 FaceNormal )
{
  bool Result = false;

  if ( Dot(FaceToPoint, FaceNormal) > 0 )
  {
    Result = true;
  }

  return Result;
}

inline bool
IsFacingPoint( glm::vec3 FaceToPoint, v3 FaceNormal )
{
  bool Result = IsFacingPoint(GLV3(FaceToPoint), FaceNormal);
  return Result;
}

inline bool
IsFilled( Chunk *chunk, voxel_position VoxelP )
{
  bool isFilled = true;

  if (chunk)
  {
    int i = VoxelP.x +
      (VoxelP.y*chunk->Dim.x) +
      (VoxelP.z*chunk->Dim.x*chunk->Dim.y);

    assert(i > -1);
    isFilled = IsSet(chunk->Voxels[i].flags, Voxel_Filled);
  }

  return isFilled;
}

inline bool
IsFilled( World *world, Chunk *chunk, canonical_position VoxelP )
{
  bool isFilled = true;

  if ( chunk )
  {
    Chunk *localChunk = chunk;

    if ( chunk->WorldP != VoxelP.WorldP )
    {
      localChunk = GetWorldChunk(world, VoxelP.WorldP);
    }

    isFilled = IsFilled( localChunk, Voxel_Position(VoxelP.Offset) );
  }

  return isFilled;

}

inline bool
NotFilled( World *world, Chunk *chunk, canonical_position VoxelP )
{
  bool Result;
  Result = !(IsFilled(world,chunk,VoxelP));

  return Result;
}

inline voxel_position
ClampPositive( voxel_position V )
{
  voxel_position Result = V;

  if ( V.x < 0 )
    Result.x = 0;

  if ( V.y < 0 )
    Result.y = 0;

  if ( V.z < 0 )
    Result.z = 0;

  return Result;
}


// NOTE : The maximum bound is non-inclusive; 0 is part of the chunk
// while the furthest point in x,y or z is the next chunk
inline canonical_position
Canonicalize( World *world, v3 Offset, world_position WorldP )
{
  canonical_position Result;

  Result.Offset = Offset;
  Result.WorldP = WorldP;

  if ( Result.Offset.x >= world->ChunkDim.x )
  {
    int ChunkWidths = Result.Offset.x / world->ChunkDim.x;
    Result.Offset.x -= world->ChunkDim.x*ChunkWidths;
    Result.WorldP.x += ChunkWidths;
  }
  if ( Result.Offset.y >= world->ChunkDim.y )
  {
    int ChunkWidths = Result.Offset.y / world->ChunkDim.y;
    Result.Offset.y -= world->ChunkDim.y*ChunkWidths;
    Result.WorldP.y += ChunkWidths;
  }
  if ( Result.Offset.z >= world->ChunkDim.z )
  {
    int ChunkWidths = Result.Offset.z / world->ChunkDim.z;
    Result.Offset.z -= world->ChunkDim.z*ChunkWidths;
    Result.WorldP.z += ChunkWidths;
  }

  if ( Result.Offset.x < 0 )
  {
    int ChunkWidths = (Result.Offset.x-world->ChunkDim.x) / -world->ChunkDim.x;
    Result.Offset.x += world->ChunkDim.x*ChunkWidths;
    Result.WorldP.x -= ChunkWidths;
  }
  if ( Result.Offset.y < 0 )
  {
    int ChunkWidths = (Result.Offset.y-world->ChunkDim.y) / -world->ChunkDim.y;
    Result.Offset.y += world->ChunkDim.y*ChunkWidths;
    Result.WorldP.y -= ChunkWidths;
  }
  if ( Result.Offset.z < 0 )
  {
    int ChunkWidths = (Result.Offset.z-world->ChunkDim.z) / -world->ChunkDim.z;
    Result.Offset.z += world->ChunkDim.z*ChunkWidths;
    Result.WorldP.z -= ChunkWidths;
  }

  return Result;
}

inline canonical_position
Canonicalize( World *world, canonical_position CP )
{
  canonical_position Result = Canonicalize( world, CP.Offset, CP.WorldP );
  return Result;
}

#endif
