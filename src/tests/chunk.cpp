
#include <bonsai_types.h>
#include <bonsai_stdlib/test/utils.h>


s32
main(s32 ArgCount, const char** Args)
{
  TestSuiteBegin("Chunk", ArgCount, Args);

  memory_arena *Memory = AllocateArena(Megabytes(32));

  chunk_dimension DestChunkDim = Chunk_Dimension(8);
  world_position  DestChunkP = World_Position(0);

  chunk_dimension SynChunkDim = DestChunkDim+2;
  world_position  SynChunkP = World_Position(0);

  { // Ensure copy works as intended
    world_chunk *SyntheticChunk = AllocateWorldChunk(Memory, SynChunkP, SynChunkDim);
    world_chunk *DestChunk = AllocateWorldChunk(Memory, DestChunkP, DestChunkDim);

    InitChunkPlane(0, SyntheticChunk, SynChunkDim, GRASS_GREEN);
    CopyChunkOffset(SyntheticChunk, SynChunkDim, DestChunk, DestChunkDim, Voxel_Position(1));
    SetFlag(DestChunk, Chunk_Initialized);
    SetFlag(SyntheticChunk, Chunk_Initialized);

    for ( int z = 0; z < DestChunkDim.z; ++ z)
    {
      for ( int y = 0; y < DestChunkDim.y; ++ y)
      {
        for ( int x = 0; x < DestChunkDim.x; ++ x)
        {
          TestThat( NotFilledInChunk(SyntheticChunk->Data, Voxel_Position(x+1,y+1,z+1), SynChunkDim) );
          TestThat( NotFilledInChunk(DestChunk->Data, Voxel_Position(x,y,z), DestChunkDim) );
        }
      }
    }

  }

  { // Ensure copy works as intended
    world_chunk *SyntheticChunk = AllocateWorldChunk(Memory, SynChunkP, SynChunkDim);
    world_chunk *DestChunk = AllocateWorldChunk(Memory, DestChunkP, DestChunkDim);

    InitChunkPlane(1, SyntheticChunk, SynChunkDim, GRASS_GREEN);
    CopyChunkOffset(SyntheticChunk, SynChunkDim, DestChunk, DestChunkDim, Voxel_Position(1));
    SetFlag(DestChunk, Chunk_Initialized);
    SetFlag(SyntheticChunk, Chunk_Initialized);

    for ( int z = 0; z < DestChunkDim.z; ++ z)
    {
      for ( int y = 0; y < DestChunkDim.y; ++ y)
      {
        for ( int x = 0; x < DestChunkDim.x; ++ x)
        {
          if ( z == 0 )
          {
            TestThat( IsFilledInChunk(DestChunk->Data, Voxel_Position(x,y,z), DestChunkDim) );
            TestThat( IsFilledInChunk(SyntheticChunk->Data, Voxel_Position(x+1,y+1,z+1), SynChunkDim) );
          }
          else
          {
            TestThat( NotFilledInChunk(DestChunk->Data, Voxel_Position(x,y,z), DestChunkDim) );
            TestThat( NotFilledInChunk(SyntheticChunk->Data, Voxel_Position(x+1,y+1,z+1), SynChunkDim) );
          }
        }
      }
    }

  }


  TestSuiteEnd();
}
