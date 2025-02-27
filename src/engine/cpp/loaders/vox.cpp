// NOTE(Jesse): These chunk identifiers are actually present in the file in the
// reverse byte order.. ie. ID_VOX == "VOX " in the file.
enum Chunk_ID
{
  ID_NONE = 0,

  ID_VOX  = ' XOV',
  ID_MAIN = 'NIAM',
  ID_PACK = 'KCAP',
  ID_SIZE = 'EZIS',
  ID_XYZI = 'IZYX',
  ID_RGBA = 'ABGR'
};

inline u8
ReadChar(FILE* File, int* byteCounter)
{
  u8 c;
  ReadBytesIntoBuffer(File, sizeof(u8), &c);
  *byteCounter -= sizeof(u8);
  return c;
}

inline int
ReadInt(FILE* File)
{
  s32 i;
  ReadBytesIntoBuffer(File, sizeof(s32), (u8*)&i);
  return i;
}

inline int
ReadInt(FILE* File, int* byteCounter)
{
  *byteCounter -= sizeof(s32);
  return ReadInt(File);
}

inline Chunk_ID
GetHeaderType(FILE* File, int* byteCounter)
{
  int ID = ReadInt(File, byteCounter);

  Assert( ID == ID_VOX  ||
          ID == ID_MAIN ||
          ID == ID_PACK ||
          ID == ID_SIZE ||
          ID == ID_XYZI ||
          ID == ID_RGBA );

  return (Chunk_ID)ID;
}

inline void
ReadVoxChunk(FILE *File)
{
  s32 Version = s32_MAX;
  s32 ID = ReadInt(File);
  Assert( ID == ID_VOX );

  Version = ReadInt(File);
  Assert(Version == 150);

  return;
}

inline int
ReadMainChunk(FILE *File)
{
  int ID = ReadInt(File);

  if ( ID != ID_MAIN )
  {
    Error("Invalid Main Chunk");
    return 0;
  }

  int selfSize = ReadInt(File);
  if ( selfSize != 0 ) 
  {
    Error("Invalid Main Chunk");
    return 0;
  }

  int childSize = ReadInt(File);
  if (childSize == 0 )
  {
    Error("Invalid Main Chunk");
    return 0;
  }

  return childSize;
}

inline chunk_dimension
ReadSizeChunk(FILE *File, int* byteCounter)
{
  int ChunkContentBytes = ReadInt(File, byteCounter);
  int ChildChunkCount = ReadInt(File, byteCounter);

  int chunkX = ReadInt(File, byteCounter);
  int chunkY = ReadInt(File, byteCounter);
  int chunkZ = ReadInt(File, byteCounter);

  chunk_dimension Result = Chunk_Dimension(chunkX, chunkY, chunkZ);
  return Result;
}

inline int
ReadPackChunk(FILE *File, int* byteCounter)
{
  int ChunkContentBytes = ReadInt(File, byteCounter);
  int ChildChunkCount = ReadInt(File, byteCounter);

  int TotalChunkCount = ReadInt(File, byteCounter);
  return TotalChunkCount;
}

inline int
ReadXYZIChunk(FILE *File, int* byteCounter)
{
  int ChunkContentBytes = ReadInt(File, byteCounter);
  int ChildChunkCount = ReadInt(File, byteCounter);

  int nVoxels = ReadInt(File, byteCounter);
  return nVoxels;
}

vox_data
LoadVoxData(memory_arena *WorldStorage, heap_allocator *Heap, char const *filepath)
{
  vox_data Result = {};
  chunk_dimension ReportedDim = {};

  native_file ModelFile = OpenFile(filepath, "rb");

  if (ModelFile.Handle)
  {
    // Ensure we're dealing with a .vox file
    ReadVoxChunk(ModelFile.Handle);

    s32 totalChunkBytes = ReadMainChunk(ModelFile.Handle);
    s32 bytesRemaining = totalChunkBytes;

    int TotalChunkCount = 1; // Sometimes overwritten if theres a 'PACK' chunk
    int TotalChunksRead = 0;

    while (bytesRemaining > 0)
    {
      Chunk_ID CurrentId = GetHeaderType(ModelFile.Handle, &bytesRemaining);
      switch ( CurrentId )
      {
        case ID_RGBA:
        {
          int ChunkContentBytes = ReadInt(ModelFile.Handle, &bytesRemaining);
          int ChildChunkCount = ReadInt(ModelFile.Handle, &bytesRemaining);

          Assert(ChunkContentBytes == 256*4);
          Assert(ChildChunkCount == 0);

          Result.Palette = (v4*)HeapAllocate(Heap, 256);

          for (u32 PaletteIndex = 0; PaletteIndex < 256; ++PaletteIndex)
          {
            u8 R = ReadChar(ModelFile.Handle, &bytesRemaining);
            u8 G = ReadChar(ModelFile.Handle, &bytesRemaining);
            u8 B = ReadChar(ModelFile.Handle, &bytesRemaining);
            u8 A = ReadChar(ModelFile.Handle, &bytesRemaining);
            Result.Palette[PaletteIndex].r = R;
            Result.Palette[PaletteIndex].g = G;
            Result.Palette[PaletteIndex].b = B;
            Result.Palette[PaletteIndex].a = A;
          }

        } break;

        case ID_PACK:
        {
          TotalChunkCount = ReadPackChunk(ModelFile.Handle, &bytesRemaining);
        } break;

        case ID_SIZE:
        {
          ReportedDim = ReadSizeChunk(ModelFile.Handle, &bytesRemaining);
        } break;

        case ID_XYZI:
        {
          ++TotalChunksRead;

          s32 ReportedVoxelCount = ReadXYZIChunk(ModelFile.Handle, &bytesRemaining);

          s32 ActualVoxelCount = 0;

          s32 maxX = s32_MIN;
          s32 maxY = s32_MIN;
          s32 maxZ = s32_MIN;

          s32 minX = s32_MAX;
          s32 minY = s32_MAX;
          s32 minZ = s32_MAX;

          boundary_voxel *LocalVoxelCache = (boundary_voxel *)calloc((umm)ReportedVoxelCount, sizeof(boundary_voxel) );
          for( s32 VoxelCacheIndex = 0;
               VoxelCacheIndex < ReportedVoxelCount;
               ++VoxelCacheIndex)
          {
            s32 X = (s32)ReadChar(ModelFile.Handle, &bytesRemaining);
            s32 Y = (s32)ReadChar(ModelFile.Handle, &bytesRemaining);
            s32 Z = (s32)ReadChar(ModelFile.Handle, &bytesRemaining);
            u8 W = ReadChar(ModelFile.Handle, &bytesRemaining); // Color

            voxel_position TestP = Voxel_Position(X,Y,Z);
            if (IsInsideDim(ReportedDim, TestP))
            {
              ++ActualVoxelCount;
              maxX = Max(X, maxX);
              maxY = Max(Y, maxY);
              maxZ = Max(Z, maxZ);

              minX = Min(X, minX);
              minY = Min(Y, minY);
              minZ = Min(Z, minZ);

              LocalVoxelCache[VoxelCacheIndex] = boundary_voxel(X,Y,Z,W);
              SetFlag(&LocalVoxelCache[VoxelCacheIndex], Voxel_Filled);
            }
            else
            {
              BUG("Voxel detected outside the dimention it should be in!");
            }
          }

          s32 IndexToPosition = 1;  // max(X,Y,Z) are indicies, convert to positions
          chunk_dimension Max = Chunk_Dimension(maxX+IndexToPosition, maxY+IndexToPosition, maxZ+IndexToPosition);
          chunk_dimension Min = Chunk_Dimension(minX, minY, minZ);

          chunk_dimension ModelDim = Max - Min;

          Result.ChunkData = AllocateChunk(WorldStorage, ModelDim);
          Result.ChunkData->Dim = ModelDim;

          for( int VoxelCacheIndex = 0;
               VoxelCacheIndex < ActualVoxelCount;
               ++VoxelCacheIndex)
          {
            boundary_voxel *Voxel = &LocalVoxelCache[VoxelCacheIndex];
            Voxel->Offset -= Min;
            s32 Index = GetIndex(Voxel->Offset, ModelDim);
            Result.ChunkData->Voxels[Index] = Voxel->V;
          }

          free(LocalVoxelCache);

          FullBarrier;

          SetFlag(Result.ChunkData, Chunk_VoxelsInitialized);
        } break;

        InvalidDefaultCase;
      }
    }

    Assert(bytesRemaining == 0);

    CloseFile(&ModelFile);
  }
  else
  {
    Error("Couldn't read model file '%s' .", filepath);
  }

  return Result;
}

model
LoadVoxModel(memory_arena *WorldStorage, heap_allocator *Heap, char const *filepath)
{
  TIMED_FUNCTION();

  model Result = {};
  vox_data Vox = LoadVoxData(WorldStorage, Heap, filepath);
  chunk_data *ChunkData = Vox.ChunkData;

  Result.Dim = ChunkData->Dim;

  // TODO(Jesse): This wastes a shit-ton of memory.  Should probably have a way
  // of realloc-ing, or sum up how much memory we'll need first?
  AllocateMesh(&Result.Mesh, 6*VERTS_PER_FACE*(u32)Volume(Result.Dim), WorldStorage);

  v4 *ColorPalette = Vox.Palette ? Vox.Palette : DefaultPalette;
  BuildEntityMesh(ChunkData, &Result.Mesh, ColorPalette, Result.Dim);

  return Result;
}

