#include <cstring>
#include <csignal>

#include <sstream>


#include <stdio.h>

#include <colors.h>

global_variable u32 Global_QuadVertexBuffer;
global_variable v3 GlobalLightPosition = {{0.20f, 1.0f, 1.0f}};

global_variable m4 NdcToScreenSpace = {
  V4(0.5, 0.0, 0.0, 0.0),
  V4(0.0, 0.5, 0.0, 0.0),
  V4(0.0, 0.0, 0.5, 0.0),
  V4(0.5, 0.5, 0.5, 1.0)
};

void
Init_Global_QuadVertexBuffer() {
  glGenBuffers(1, &Global_QuadVertexBuffer);
  Assert(Global_QuadVertexBuffer);

  glBindBuffer(GL_ARRAY_BUFFER, Global_QuadVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return;
}


void
RenderQuad()
{
  if (!Global_QuadVertexBuffer)
  {
    Init_Global_QuadVertexBuffer();
  }

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, Global_QuadVertexBuffer);
  glVertexAttribPointer(
    0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );

  Draw(6); // 2*3 indices starting at 0 -> 2 triangles

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(0);
}














void
InitSsaoKernel(v3 *Kernel, s32 Count, random_series *Entropy)
{
  for (int KernelIndex = 0;
       KernelIndex < Count;
       ++KernelIndex)
  {
    r32 Scale = (r32)KernelIndex/Count;
    Scale = Lerp(Scale * Scale, 0.1f, 1.0f);

    Kernel[KernelIndex] = V3(RandomBilateral(Entropy), RandomBilateral(Entropy), RandomUnilateral(Entropy));
    Kernel[KernelIndex] = Normalize( Kernel[KernelIndex]*Scale );
  }
}

void
InitSsaoNoise(v3 *Noise, s32 Count, random_series *Entropy)
{
  for (s32 NoiseIndex = 0;
       NoiseIndex < Count;
       ++NoiseIndex)
  {
    Noise[NoiseIndex] = Normalize( V3(RandomBilateral(Entropy), RandomBilateral(Entropy), 0.0f) );
  }
}

texture *
AllocateAndInitSsaoNoise(ao_render_group *AoGroup, memory_arena *GraphicsMemory)
{
  v2i SsaoNoiseDim = V2i(4,4);
  random_series SsaoEntropy;

  AoGroup->NoiseTile = V3(SCR_WIDTH/SsaoNoiseDim.x, SCR_HEIGHT/SsaoNoiseDim.y, 1);

  InitSsaoKernel(AoGroup->SsaoKernel, ArrayCount(AoGroup->SsaoKernel), &SsaoEntropy);

  // TODO(Jesse): Transient arena for this instead of stack allocation ?
  v3 *SsaoNoise = Allocate(v3, GraphicsMemory, 16);
  InitSsaoNoise(SsaoNoise, 16, &SsaoEntropy);

  texture *SsaoNoiseTexture = MakeTexture_RGB(SsaoNoiseDim, SsaoNoise, GraphicsMemory);
  return SsaoNoiseTexture;
}


shader
MakeLightingShader(memory_arena *GraphicsMemory,
    g_buffer_textures *gTextures, texture *ShadowMap, texture *Ssao,
    m4 *ShadowMVP, game_lights *Lights, camera *Camera)
{
  shader Shader = LoadShaders( "Lighting.vertexshader", "Lighting.fragmentshader", GraphicsMemory);

  shader_uniform **Current = &Shader.FirstUniform;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Color, "gColor");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Normal, "gNormal");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Position, "gPosition");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, ShadowMap, "shadowMap");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, Ssao, "Ssao");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, ShadowMVP, "ShadowMVP");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, Lights->ColorTex, "LightColors");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, Lights->PositionTex, "LightPositions");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, &Lights->IndexToUV, "LightIndexToUV");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, &Lights->Count, "LightCount");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, Camera, "CameraP");
  Current = &(*Current)->Next;

  AssertNoGlErrors;

#if 0
  if (Lights)
  {
    *Current = GetUniform(GraphicsMemory, &Shader, Lights->Lights, "Lights");
    Current = &(*Current)->Next;

    *Current = GetUniform(GraphicsMemory, &Shader, &Lights->Count, "LightCount");
    Current = &(*Current)->Next;
  }
#endif

  return Shader;
}

framebuffer
GenFramebuffer()
{
  framebuffer Framebuffer = {};
  glGenFramebuffers(1, &Framebuffer.ID);

  return Framebuffer;
}

ao_render_group *
CreateAoRenderGroup(memory_arena *Mem)
{
  ao_render_group *Result = Allocate(ao_render_group, Mem, 1);
  Result->FBO = GenFramebuffer();

  return Result;
}

g_buffer_render_group *
CreateGbuffer(memory_arena *Memory)
{
  g_buffer_render_group *gBuffer = Allocate(g_buffer_render_group, Memory, 1);
  gBuffer->FBO = GenFramebuffer();
  gBuffer->ViewProjection = IdentityMatrix;
  gBuffer->ShadowMVP = IdentityMatrix;

  glGenBuffers(1, &gBuffer->vertexbuffer);
  glGenBuffers(1, &gBuffer->colorbuffer);
  glGenBuffers(1, &gBuffer->normalbuffer);

  return gBuffer;
}

void
SetDrawBuffers(framebuffer *FBO)
{
  // TODO(Jesse): Transient storage?
  u32 Attachments[32] = {};
  for (u32 AttIndex = 0;
      AttIndex < FBO->Attachments;

      ++AttIndex)
  {
    Attachments[AttIndex] =  GL_COLOR_ATTACHMENT0 + AttIndex;
  }

  glDrawBuffers(FBO->Attachments, Attachments);
}

shader
CreateGbufferShader(memory_arena *GraphicsMemory, m4 *ViewProjection, camera *Camera)
{
  shader Shader = LoadShaders( "gBuffer.vertexshader",
                               "gBuffer.fragmentshader", GraphicsMemory);

  shader_uniform **Current = &Shader.FirstUniform;

  *Current = GetUniform(GraphicsMemory, &Shader, ViewProjection, "ViewProjection");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, &IdentityMatrix, "Model");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, &Camera->Frust.farClip, "FarClip");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, &Camera->Frust.nearClip, "NearClip");
  Current = &(*Current)->Next;

  return Shader;
}

shader
MakeSsaoShader(memory_arena *GraphicsMemory, g_buffer_textures *gTextures,
    texture *SsaoNoiseTexture, v3 *SsaoNoiseTile, m4 *ViewProjection)
{
  shader Shader = LoadShaders( "Passthrough.vertexshader",
                               "Ao.fragmentshader", GraphicsMemory);

  shader_uniform **Current = &Shader.FirstUniform;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Color, "gColor");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Normal, "gNormal");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, gTextures->Position, "gPosition");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, SsaoNoiseTexture, "SsaoNoiseTexture");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, SsaoNoiseTile, "SsaoNoiseTile");
  Current = &(*Current)->Next;

  *Current = GetUniform(GraphicsMemory, &Shader, ViewProjection, "ViewProjection");
  Current = &(*Current)->Next;

  AssertNoGlErrors;

  return Shader;
}

bool
InitAoRenderGroup(ao_render_group *AoGroup, memory_arena *GraphicsMemory)
{
  glBindFramebuffer(GL_FRAMEBUFFER, AoGroup->FBO.ID);

  v2i ScreenDim = V2i(SCR_WIDTH, SCR_HEIGHT);
  AssertNoGlErrors;

  AoGroup->Texture = MakeTexture_SingleChannel( ScreenDim, GraphicsMemory);

  FramebufferTexture(&AoGroup->FBO, AoGroup->Texture);
  SetDrawBuffers(&AoGroup->FBO);

  AssertNoGlErrors;

  if (!CheckAndClearFramebuffer())
    return false;

  return True;
}

bool
InitGbufferRenderGroup( g_buffer_render_group *gBuffer, memory_arena *GraphicsMemory)
{
  glBindFramebuffer(GL_FRAMEBUFFER, gBuffer->FBO.ID);

  gBuffer->Textures = Allocate(g_buffer_textures, GraphicsMemory, 1);

  v2i ScreenDim = V2i(SCR_WIDTH, SCR_HEIGHT);
  gBuffer->Textures->Color    = MakeTexture_RGBA( ScreenDim, (v4*)0, GraphicsMemory);

  // FIXME(Jesse): This makes GL 3 fail on the FRAMEBUFFER_COMPLETE check
  // if it's an RGB texture.  We only need three channels for normal so this
  // should probably be an RGB
  gBuffer->Textures->Normal   = MakeTexture_RGBA( ScreenDim, (v4*)0, GraphicsMemory);

  gBuffer->Textures->Position = MakeTexture_RGBA( ScreenDim, (v4*)0, GraphicsMemory);

  FramebufferTexture(&gBuffer->FBO, gBuffer->Textures->Color);
  FramebufferTexture(&gBuffer->FBO, gBuffer->Textures->Normal);
  FramebufferTexture(&gBuffer->FBO, gBuffer->Textures->Position);
  SetDrawBuffers(&gBuffer->FBO);

  texture *DepthTexture    = MakeDepthTexture( ScreenDim, GraphicsMemory );
  FramebufferDepthTexture(DepthTexture);

  b32 Result = CheckAndClearFramebuffer();
  return Result;
}

void
DoLight(game_lights *Lights, v3 Position, v3 Color)
{
  Assert(Lights->Count < MAX_LIGHTS);

  if (Lights->Count < MAX_LIGHTS)
  {
    light *Light = Lights->Lights + Lights->Count++;
    Light->Position = Position;
    Light->Color = Color;
  }

  return;
}

b32
InitializeShadowBuffer(shadow_render_group *SG, memory_arena *GraphicsMemory)
{
  // The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
  glGenFramebuffers(1, &SG->FramebufferName);
  glBindFramebuffer(GL_FRAMEBUFFER, SG->FramebufferName);

  SG->ShadowMap = MakeDepthTexture( V2i(SHADOW_MAP_RESOLUTION_X, SHADOW_MAP_RESOLUTION_Y), GraphicsMemory);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, SG->ShadowMap->ID, 0);

  // TODO(Jesse): Not present on ES2 .. should we use them?
  // No color output in the bound framebuffer, only depth.
  /* glDrawBuffer(GL_NONE); */
  /* glReadBuffer(GL_NONE); */

  // For debug-only visualization of this texture
  SG->DebugTextureShader = MakeSimpleTextureShader(SG->ShadowMap, GraphicsMemory);

  SG->DepthShader = LoadShaders( "DepthRTT.vertexshader", "DepthRTT.fragmentshader", GraphicsMemory);
  SG->MVP_ID = GetShaderUniform(&SG->DepthShader, "depthMVP");

  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return false;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

 return true;
}

void
InitCamera(camera* Camera, float FocalLength)
{
  Camera->Frust.farClip = FocalLength;
  Camera->Frust.nearClip = 0.1f;
  Camera->Frust.width = 30.0f;
  Camera->Frust.FOV = 45.0f;
  Camera->Up = WORLD_Z;
  Camera->Right = WORLD_X;
  Camera->Front = V3(0,0,0);
  Camera->Pitch = PIf;
  return;
}

game_lights *
LightingInit(memory_arena *GraphicsMemory)
{
  game_lights *Lights = Allocate(game_lights, GraphicsMemory, 1);
  Lights->Lights      = Allocate(light, GraphicsMemory, MAX_LIGHTS);

  Lights->ColorTex    = MakeTexture_RGB(V2i(MAX_LIGHTS, 1), 0, GraphicsMemory);
  Lights->PositionTex = MakeTexture_RGB(V2i(MAX_LIGHTS, 1), 0, GraphicsMemory);
  AssertNoGlErrors;

  Lights->IndexToUV = 1.0f / MAX_LIGHTS;

  return Lights;
}


void
UpdateLightingTextures(game_lights *Lights)
{
  // TODO(Jesse): Allocate lights such that this swizzle is unneeded
  v3 *PosData = AllocateProtection(v3, TranArena, MAX_LIGHTS, False);
  v3 *ColorData = AllocateProtection(v3, TranArena, MAX_LIGHTS, False);

  for (u32 LightIndex = 0;
      LightIndex < Lights->Count;
      ++LightIndex)
  {
    PosData[LightIndex] = Lights->Lights[LightIndex].Position;
    ColorData[LightIndex] = Lights->Lights[LightIndex].Color;
  }

  u32 Type = GL_TEXTURE_2D;

  glBindTexture(Type, Lights->PositionTex->ID);
  glTexImage2D( Type, 0, GL_RGB32F,
                Lights->PositionTex->Dim.x, Lights->PositionTex->Dim.y,
                0,  GL_RGB, GL_FLOAT, PosData);
  AssertNoGlErrors;

  glBindTexture(Type, Lights->ColorTex->ID);
  glTexImage2D( Type, 0, GL_RGB32F,
                Lights->ColorTex->Dim.x, Lights->ColorTex->Dim.y,
                0,  GL_RGB, GL_FLOAT, ColorData);
  AssertNoGlErrors;

  return;
}

graphics *
GraphicsInit(memory_arena *GraphicsMemory)
{
  graphics *Result = Allocate(graphics, GraphicsMemory, 1);
  Result->Memory = GraphicsMemory;

  Result->Lights = LightingInit(GraphicsMemory);

  Result->Camera = Allocate(camera, GraphicsMemory, 1);
  InitCamera(Result->Camera, 1000.0f);

  shadow_render_group *SG = Allocate(shadow_render_group, GraphicsMemory, 1);
  if (!InitializeShadowBuffer(SG, GraphicsMemory))
  {
    Error("Initializing Shadow Buffer"); return False;
  }


  g_buffer_render_group *gBuffer = CreateGbuffer(GraphicsMemory);
  if (!InitGbufferRenderGroup(gBuffer, GraphicsMemory))
  {
    Error("Initializing g_buffer_render_group"); return False;
  }


  ao_render_group *AoGroup = CreateAoRenderGroup(GraphicsMemory);
  if (!InitAoRenderGroup(AoGroup, GraphicsMemory))
  {
    Error("Initializing ao_render_group"); return False;
  }

  texture *SsaoNoiseTexture = AllocateAndInitSsaoNoise(AoGroup, GraphicsMemory);

  gBuffer->LightingShader =
    MakeLightingShader(GraphicsMemory, gBuffer->Textures, SG->ShadowMap,
                       AoGroup->Texture, &gBuffer->ShadowMVP, Result->Lights, Result->Camera);

  gBuffer->gBufferShader =
    CreateGbufferShader(GraphicsMemory, &gBuffer->ViewProjection, Result->Camera);

  AoGroup->Shader =
    MakeSsaoShader(GraphicsMemory, gBuffer->Textures, SsaoNoiseTexture,
                   &AoGroup->NoiseTile, &gBuffer->ViewProjection);

  AoGroup->SsaoKernelUniform = GetShaderUniform(&AoGroup->Shader, "SsaoKernel");

  { // To keep these here or not to keep these here..
    gBuffer->DebugColorTextureShader    = MakeSimpleTextureShader(gBuffer->Textures->Color    , GraphicsMemory);
    gBuffer->DebugNormalTextureShader   = MakeSimpleTextureShader(gBuffer->Textures->Normal   , GraphicsMemory);
    gBuffer->DebugPositionTextureShader = MakeSimpleTextureShader(gBuffer->Textures->Position , GraphicsMemory);
    AoGroup->DebugSsaoShader            = MakeSimpleTextureShader(AoGroup->Texture            , GraphicsMemory);
  }

  // FIXME(Jesse): Our face widing is totally fucked
  /* glEnable(GL_CULL_FACE); */
  /* glCullFace(GL_BACK); */

  AssertNoGlErrors;

  Result->AoGroup = AoGroup;
  Result->gBuffer = gBuffer;
  Result->SG = SG;

  return Result;
}

inline m4
GetShadowMapMVP(light *GlobalLight)
{
  // Compute the MVP matrix from the light's point of view
  /* v3 Translate = GetRenderP(Camera->Target, Camera); */
  m4 depthProjectionMatrix = Orthographic(SHADOW_MAP_X,
                                          SHADOW_MAP_Y,
                                          SHADOW_MAP_Z_MIN,
                                          SHADOW_MAP_Z_MAX);

  v3 Front = Normalize(GlobalLight->Position);
  v3 Right = Cross( Front, V3(0,1,0) );
  v3 Up = Cross(Right, Front);

  v3 Target = V3(0);
  m4 depthViewMatrix =  LookAt(GlobalLight->Position, Target, Up);

  return depthProjectionMatrix * depthViewMatrix;
}

void
DrawTexturedQuad(shader *SimpleTextureShader)
{
  r32 Scale = 1.0f;

  glDepthFunc(GL_LEQUAL);

  texture *Texture = SimpleTextureShader->FirstUniform->Texture;
  SetViewport( V2(Texture->Dim.x, Texture->Dim.y)*Scale );

  glUseProgram(SimpleTextureShader->ID);

  BindShaderUniforms(SimpleTextureShader);

  RenderQuad();
  AssertNoGlErrors;

  return;
}

void
RenderAoTexture(ao_render_group *AoGroup)
{
  glBindFramebuffer(GL_FRAMEBUFFER, AoGroup->FBO.ID);
  SetViewport( V2(SCR_WIDTH, SCR_HEIGHT) );

  glUseProgram(AoGroup->Shader.ID);

  glUniform3fv(AoGroup->SsaoKernelUniform, SSAO_KERNEL_SIZE, (r32*)AoGroup->SsaoKernel);

  BindShaderUniforms(&AoGroup->Shader);

  RenderQuad();

  AssertNoGlErrors;

  return;
}

void
DrawGBufferToFullscreenQuad( platform *Plat, graphics *Graphics)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  SetViewport(V2(Plat->WindowWidth, Plat->WindowHeight));

  glUseProgram(Graphics->gBuffer->LightingShader.ID);

  UpdateLightingTextures(Graphics->Lights);
  Graphics->gBuffer->ShadowMVP = NdcToScreenSpace * GetShadowMapMVP(&Graphics->Lights->Lights[0]);

  BindShaderUniforms(&Graphics->gBuffer->LightingShader);

  RenderQuad();

  AssertNoGlErrors;

  return;
}

#if 0
void
DEBUG_CopyTextureToMemory(texture *Texture)
{
  glBindTexture(GL_TEXTURE_2D, Texture->ID);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  u8 *raw_img = (u8*)calloc(sizeof(u8), Texture->Dim.x * Texture->Dim.y * 4);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, raw_img);
  //glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, raw_img);
  AssertNoGlErrors;
  free(raw_img);
  return;
}
#endif

#if 0
void
RenderShadowMap(untextured_3d_geometry_buffer *Mesh, graphics *Graphics)
{
  TIMED_FUNCTION();
  SetViewport(V2(SHADOW_MAP_RESOLUTION_X, SHADOW_MAP_RESOLUTION_Y));

  m4 MVP = GetShadowMapMVP(Camera, &SG->GameLights[0]);

  glBindFramebuffer(GL_FRAMEBUFFER, SG->FramebufferName);

  glUseProgram(SG->DepthShader.ID);
  glUniformMatrix4fv(SG->MVP_ID, 1, GL_FALSE, &MVP.E[0].E[0]);

  // 1rst attribute buffer : vertices
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, RG->vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, Mesh->At*sizeof(v3), Mesh->VertexData, GL_STATIC_DRAW);
  glVertexAttribPointer(
    0,                  // The attribute we want to configure
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );

  Draw(Mesh->At);

  glDisableVertexAttribArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return;
}
#endif

void
RenderWorldToGBuffer(untextured_3d_geometry_buffer *Mesh, g_buffer_render_group *RG)
{
  TIMED_FUNCTION();
  glBindFramebuffer(GL_FRAMEBUFFER, RG->FBO.ID);
  glUseProgram(RG->gBufferShader.ID);

  SetViewport( V2(SCR_WIDTH, SCR_HEIGHT) );

  BindShaderUniforms(&RG->gBufferShader);

  TIMED_BLOCK("gBuffer - Bind and buffer data");
    u32 AttributeIndex = 0;
    BufferVertsToCard(RG->vertexbuffer, Mesh, &AttributeIndex);
    BufferColorsToCard(RG->colorbuffer, Mesh, &AttributeIndex);
    BufferNormalsToCard(RG->normalbuffer, Mesh, &AttributeIndex);
  END_BLOCK("gBuffer - Bind and buffer data");

  Draw(Mesh->At);
  Mesh->At = 0;

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);

  return;
}

inline void
RenderGBuffer(untextured_3d_geometry_buffer *Mesh, graphics *Graphics)
{
  TIMED_FUNCTION();

  /* RenderShadowMap(Mesh, Graphics); */

  RenderWorldToGBuffer(Mesh, Graphics->gBuffer);

  AssertNoGlErrors;

  return;
}

inline void
RenderPostBuffer(post_processing_group *PostGroup, untextured_3d_geometry_buffer *Mesh)
{
  glBindFramebuffer(GL_FRAMEBUFFER, PostGroup->FBO.ID);
  glUseProgram(PostGroup->Shader.ID);

  SetViewport( V2(SCR_WIDTH, SCR_HEIGHT) );

  BindShaderUniforms(&PostGroup->Shader);

  TIMED_BLOCK("PostBuffer - Bind and buffer data");
    u32 AttributeIndex = 0;
    BufferVertsToCard(PostGroup->VertexBuffer, Mesh, &AttributeIndex);
    BufferColorsToCard(PostGroup->ColorBuffer, Mesh, &AttributeIndex);
  END_BLOCK("PostBuffer - Bind and buffer data");

  Draw(Mesh->At);
  Mesh->At = 0;

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

inline void
DrawVoxel( untextured_3d_geometry_buffer *Mesh, graphics *Graphics,
           v3 RenderP, int ColorIndex, v3 Diameter, r32 Emission = 1.0f)
{
  TIMED_FUNCTION();

  v4 FaceColors[FACE_VERT_COUNT];
  FillColorArray(ColorIndex, FaceColors, FACE_VERT_COUNT, Emission);

  v3 VertexData[6];

  v3 Center = RenderP - (Diameter*0.5);
  RightFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, RightFaceNormalData, FaceColors);

  LeftFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, LeftFaceNormalData, FaceColors);

  BottomFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, BottomFaceNormalData, FaceColors);

  TopFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, TopFaceNormalData, FaceColors);

  FrontFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, FrontFaceNormalData, FaceColors);

  BackFaceVertexData( Center, Diameter, VertexData);
  BufferVertsChecked(Mesh, Graphics, 6, VertexData, BackFaceNormalData, FaceColors);

  return;
}

inline bool
IsRightChunkBoundary( chunk_dimension ChunkDim, int idx )
{
  return (idx+1) % (int)ChunkDim.x == 0;
}

inline bool
IsLeftChunkBoundary( chunk_dimension ChunkDim, int idx )
{
  return (idx) % (int)ChunkDim.x == 0;
}

inline bool
IsTopChunkBoundary( chunk_dimension ChunkDim, int idx )
{
  return ((idx/(int)ChunkDim.x)+1) % (int)ChunkDim.y == 0;
}

inline bool
IsBottomChunkBoundary( chunk_dimension ChunkDim, int idx )
{
  return (idx/(int)ChunkDim.x) % (int)ChunkDim.y == 0;
}

inline float
GetTheta(v3 P1, v3 P2)
{
  float DotP1P2 = Dot(P1,P2);

  float LP1, LP2;

  LP1 = Length(P1);
  LP2 = Length(P2);

  Assert(LP1 != 0);
  Assert(LP2 != 0);

  float cosTheta = DotP1P2 / (LP1*LP2);
  cosTheta = ClampBilateral(cosTheta);
  float theta = acos( cosTheta );

  Assert(theta >= -1 || theta <= 1);
  return theta;
}

inline Quaternion
RotatePoint(v3 P1, v3 P2)
{
  P1 = Normalize(P1);
  P2 = Normalize(P2);
  v3 Axis = Normalize(Cross(P1, P2));

  float theta = GetTheta(P1, P2);

  Quaternion Result = Quaternion((Axis*sin(theta/2)), cos(theta/2));

  if (Length(Result.xyz) == 0)  // The resulting rotation was inconsequential
    Result = Quaternion();

  return Result;
}

inline void
DEBUG_DrawLine( untextured_3d_geometry_buffer *Mesh, graphics *Graphics,
                v3 P1, v3 P2, int ColorIndex, float Thickness )
{
  // 2 verts per line, 3 floats per vert

  v3 localNormalData[] =
  {
    {{0, 0, 0}},
    {{0, 0, 0}},
    {{0, 0, 0}},

    {{0, 0, 0}},
    {{0, 0, 0}},
    {{0, 0, 0}},
  };

  P1.x = P1.x - (Thickness/2.0f);
  P2.x = P2.x - (Thickness/2.0f);

  v4 FaceColors[FACE_VERT_COUNT];
  FillColorArray(ColorIndex, FaceColors, FACE_VERT_COUNT);;

  {
    v3 localVertexData[] =
    {
      {{P1.x, P1.y, P1.z}},
      {{P2.x, P2.y, P2.z}},
      {{P1.x + Thickness, P1.y,  P1.z}},

      {{P2.x, P2.y, P2.z}},
      {{P1.x + Thickness, P1.y, P1.z}},
      {{P2.x + Thickness, P2.y, P2.z}},
    };


    BufferVertsChecked(Mesh,
        Graphics,
        6,
        localVertexData,
        localNormalData,
        FaceColors);
  }

  P1.x = P1.x + (Thickness/2.0f);
  P2.x = P2.x + (Thickness/2.0f);

  P1.y = P1.y - (Thickness/2.0f);
  P2.y = P2.y - (Thickness/2.0f);

  {
    v3 localVertexData[] =
    {
      {{P1.x, P1.y, P1.z}},
      {{P2.x, P2.y, P2.z}},
      {{P1.x, P1.y + Thickness,  P1.z}},

      {{P2.x, P2.y, P2.z}},
      {{P1.x, P1.y + Thickness, P1.z}},
      {{P2.x, P2.y + Thickness, P2.z}},
    };


    BufferVertsChecked(Mesh, Graphics,
        6,
        localVertexData,
        localNormalData,
        FaceColors);
  }

#if 0
  // This is for anti-aliasing the lines; it draws extra triangles along the edges which can be set to alpha 0
  {
    float localVertexData[] =
    {
      P1.x + Thickness, P1.y + Thickness, P1.z + Thickness,
      P2.x, P2.y, P2.z,
      P2.x + Thickness, P2.y + Thickness, P2.z + Thickness,

      P2.x + Thickness, P2.y + Thickness, P2.z + Thickness,
      P1.x + Thickness, P1.y + Thickness, P1.z + Thickness,
      P1.x, P1.y, P1.z,
    };

    BufferVerts(world, gBuffer, SG, Camera,
        6
        localVertexData,
        sizeof(localVertexData),
        localNormalData,
        sizeof(localVertexData),
        FaceColors);
  }
#endif

  return;
}

inline void
DEBUG_DrawVectorAt(untextured_3d_geometry_buffer *Mesh, graphics *Graphics, v3 Offset, v3 Vector, int ColorIndex, float Thickness )
{
  DEBUG_DrawLine(Mesh, Graphics, Offset, Vector + Offset, ColorIndex, Thickness );
}

inline void
DEBUG_DrawLine(untextured_3d_geometry_buffer *Mesh, graphics *Graphics, line Line, int ColorIndex, float Thickness )
{
  DEBUG_DrawLine(Mesh, Graphics, Line.MinP, Line.MaxP, ColorIndex, Thickness);
  return;
}

void
DEBUG_DrawAABB(untextured_3d_geometry_buffer *Mesh, graphics *Graphics, v3 MinP, v3 MaxP, int ColorIndex, float Thickness = DEFAULT_LINE_THICKNESS )
{
  /* v3 HalfDim = (GetRenderP(world, MaxCP) - GetRenderP(world, MinCP)) / 2; */

  // Start in model space
  /* v3 MinP = HalfDim * -1; */
  /* v3 MaxP = HalfDim; */

  // Compute verticies
  v3 TopRL = V3(MinP.x, MaxP.y, MinP.z);
  v3 TopRR = V3(MaxP.x, MaxP.y, MinP.z);
  v3 TopFL = V3(MinP.x, MaxP.y, MaxP.z);
  v3 TopFR = V3(MaxP.x, MaxP.y, MaxP.z);
  v3 BotRL = V3(MinP.x, MinP.y, MinP.z);
  v3 BotRR = V3(MaxP.x, MinP.y, MinP.z);
  v3 BotFL = V3(MinP.x, MinP.y, MaxP.z);
  v3 BotFR = V3(MaxP.x, MinP.y, MaxP.z);

  /* // Apply rotation to verts */
  /* TopRL = ((Rotation * Quaternion(TopRL, 1)) * Conjugate(Rotation)).xyz; */
  /* TopRR = ((Rotation * Quaternion(TopRR, 1)) * Conjugate(Rotation)).xyz; */
  /* TopFL = ((Rotation * Quaternion(TopFL, 1)) * Conjugate(Rotation)).xyz; */
  /* TopFR = ((Rotation * Quaternion(TopFR, 1)) * Conjugate(Rotation)).xyz; */
  /* BotRL = ((Rotation * Quaternion(BotRL, 1)) * Conjugate(Rotation)).xyz; */
  /* BotRR = ((Rotation * Quaternion(BotRR, 1)) * Conjugate(Rotation)).xyz; */
  /* BotFL = ((Rotation * Quaternion(BotFL, 1)) * Conjugate(Rotation)).xyz; */
  /* BotFR = ((Rotation * Quaternion(BotFR, 1)) * Conjugate(Rotation)).xyz; */

  /* // Translate into world space */
  /* TopRL += HalfDim + MinCP.Offset; */
  /* TopRR += HalfDim + MinCP.Offset; */
  /* TopFL += HalfDim + MinCP.Offset; */
  /* TopFR += HalfDim + MinCP.Offset; */
  /* BotRL += HalfDim + MinCP.Offset; */
  /* BotRR += HalfDim + MinCP.Offset; */
  /* BotFL += HalfDim + MinCP.Offset; */
  /* BotFR += HalfDim + MinCP.Offset; */

  /* // Translate into render space */
  /* TopRL = GetRenderP(world, Canonical_Position(TopRL, MinCP.WorldP)); */
  /* TopRR = GetRenderP(world, Canonical_Position(TopRR, MinCP.WorldP)); */
  /* TopFL = GetRenderP(world, Canonical_Position(TopFL, MinCP.WorldP)); */
  /* TopFR = GetRenderP(world, Canonical_Position(TopFR, MinCP.WorldP)); */
  /* BotRL = GetRenderP(world, Canonical_Position(BotRL, MinCP.WorldP)); */
  /* BotRR = GetRenderP(world, Canonical_Position(BotRR, MinCP.WorldP)); */
  /* BotFL = GetRenderP(world, Canonical_Position(BotFL, MinCP.WorldP)); */
  /* BotFR = GetRenderP(world, Canonical_Position(BotFR, MinCP.WorldP)); */

  // Render
  //
  // Top
  DEBUG_DrawLine(Mesh, Graphics, TopRL, TopRR, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, TopFL, TopFR, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, TopFL, TopRL, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, TopFR, TopRR, ColorIndex, Thickness);

  // Right
  DEBUG_DrawLine(Mesh, Graphics, TopFR, BotFR, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, TopRR, BotRR, ColorIndex, Thickness);

  // Left
  DEBUG_DrawLine(Mesh, Graphics, TopFL, BotFL, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, TopRL, BotRL, ColorIndex, Thickness);

  // Bottom
  DEBUG_DrawLine(Mesh, Graphics, BotRL, BotRR, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, BotFL, BotFR, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, BotFL, BotRL, ColorIndex, Thickness);
  DEBUG_DrawLine(Mesh, Graphics, BotFR, BotRR, ColorIndex, Thickness);

  return;
}

inline void
DEBUG_DrawAABB(untextured_3d_geometry_buffer *Mesh, graphics *Graphics,  aabb Rect, int ColorIndex, float Thickness = DEFAULT_LINE_THICKNESS )
{
  v3 MinP = Rect.Center - Rect.Radius;
  v3 MaxP = Rect.Center + Rect.Radius;
  DEBUG_DrawAABB( Mesh, Graphics, MinP, MaxP, ColorIndex, Thickness );
  return;
}

inline void
DEBUG_DrawChunkAABB( untextured_3d_geometry_buffer *Mesh, graphics *Graphics,
    world_position WorldP, chunk_dimension WorldChunkDim,
    s32 ColorIndex , r32 Thickness = DEFAULT_LINE_THICKNESS)
{
  v3 MinP = GetRenderP(WorldChunkDim, Canonical_Position(V3(0,0,0), WorldP), Graphics->Camera);
  v3 MaxP = GetRenderP(WorldChunkDim, Canonical_Position(WorldChunkDim, WorldP), Graphics->Camera);

  DEBUG_DrawAABB(Mesh, Graphics, MinP, MaxP, ColorIndex, Thickness);
  return;
}

inline void
DEBUG_DrawChunkAABB(untextured_3d_geometry_buffer *Mesh, graphics *Graphics,
    world_chunk *chunk, chunk_dimension WorldChunkDim, s32
    ColorIndex, r32 Thickness = DEFAULT_LINE_THICKNESS)
{
  v3 MinP = GetRenderP(WorldChunkDim, Canonical_Position(V3(0,0,0), chunk->WorldP), Graphics->Camera);
  v3 MaxP = GetRenderP(WorldChunkDim, Canonical_Position(WorldChunkDim, chunk->WorldP), Graphics->Camera);

  DEBUG_DrawAABB(Mesh, Graphics, MinP, MaxP, ColorIndex, Thickness);
  return;
}

inline v3
Rotate(v3 P, Quaternion Rotation)
{
  v3 Result = ((Rotation * Quaternion(P,0)) * Conjugate(Rotation)).xyz;
  return Result;
}

inline line
Rotate(line Line, Quaternion Rotation)
{
  line Result;

  Result.MinP = Rotate(Line.MinP, Rotation);
  Result.MaxP = Rotate(Line.MaxP, Rotation);

  return Result;
}

#if 0
inline float
DistanceToPlane(plane *Plane, v3 P)
{
  float x = Plane->P.x;
  float y = Plane->P.y;
  float z = Plane->P.z;

  float a = Plane->Normal.x;
  float b = Plane->Normal.y;
  float c = Plane->Normal.z;

  float d = Plane->d;
  Assert(a*x + b*y + c*z + d == 0);

  float Distance = a*P.x + b*P.y + c*P.z + d;
  return Distance;
}

inline bool
IsInfrustum(chunk_dimension WorldChunkDim, camera *Camera, canonical_position P)
{
  bool Result = true;

  v3 TestP = GetRenderP(WorldChunkDim, P, Camera);

  Result &= (DistanceToPlane(&Camera->Frust.Top, TestP)   > -1*WorldChunkDim.y);
  Result &= (DistanceToPlane(&Camera->Frust.Bot, TestP)   > -1*WorldChunkDim.y);
  Result &= (DistanceToPlane(&Camera->Frust.Left, TestP)  > -1*WorldChunkDim.x);
  Result &= (DistanceToPlane(&Camera->Frust.Right, TestP) > -1*WorldChunkDim.x);

  return Result;
}
#endif

inline voxel_position
Clamp01( voxel_position V )
{
  voxel_position Result = V;
  if (Result.x < 0) Result.x = 0;
  if (Result.y < 0) Result.y = 0;
  if (Result.z < 0) Result.z = 0;

  if (Result.x > 1) Result.x = 1;
  if (Result.y > 1) Result.y = 1;
  if (Result.z > 1) Result.z = 1;

  return Result;
}

#if 0
void
BuildExteriorBoundaryVoxels( world_chunk *chunk, chunk_dimension Dim, world_chunk *Neighbor, voxel_position NeighborVector )
{
  voxel_position nvSq = (NeighborVector*NeighborVector);

  voxel_position AbsInvNeighborVector = ((nvSq-1)*(nvSq-1));

  voxel_position LocalPlane = ClampPositive(Dim-1) * AbsInvNeighborVector + 1;

  voxel_position LocalOffset = ClampPositive(Dim*NeighborVector - nvSq);

  voxel_position Start = Voxel_Position(0,0,0);

  for ( int z = Start.z; z < LocalPlane.z; ++z )
  {
    for ( int y = Start.y; y < LocalPlane.y; ++y )
    {
      for ( int x = Start.x; x < LocalPlane.x; ++x )
      {
        voxel_position LocalVoxelP = Voxel_Position(x+LocalOffset.x, y+LocalOffset.y, z+LocalOffset.z);

        if ( chunk && !IsFilledInChunk(chunk->Data, LocalVoxelP, Dim) )
          continue;

        voxel_position NeighborP = ClampPositive(
            (LocalVoxelP - (Dim * NeighborVector))
            - nvSq);

        if ( Neighbor && !IsFilledInChunk(Neighbor->Data, NeighborP, Dim) )
        {
          voxel *Voxel = &chunk->Data->Voxels[GetIndex(LocalVoxelP, chunk->Data, Dim)];

          if (NeighborVector.x > 0)
            SetFlag(Voxel, Voxel_RightFace);

          if (NeighborVector.x < 0)
            SetFlag(Voxel, Voxel_LeftFace);

          if (NeighborVector.y > 0)
            SetFlag(Voxel, Voxel_TopFace);

          if (NeighborVector.y < 0)
            SetFlag(Voxel, Voxel_BottomFace);

          if (NeighborVector.z > 0)
            SetFlag(Voxel, Voxel_FrontFace);

          if (NeighborVector.z < 0)
            SetFlag(Voxel, Voxel_BackFace);

          boundary_voxel BoundaryVoxel(Voxel, LocalVoxelP);
          PushBoundaryVoxel( chunk->Data, &BoundaryVoxel, Dim);
        }
      }
    }
  }


  return;
}

inline bool
IsInfrustum( chunk_dimension WorldChunkDim, camera *Camera, world_chunk *Chunk )
{
  v3 ChunkMid = WorldChunkDim/2.0f;
  canonical_position P1 = Canonical_Position(  ChunkMid, Chunk->WorldP );
  bool Result = IsInfrustum(WorldChunkDim, Camera, P1 );
  return Result;
}
#endif

inline void
ClearFramebuffers(graphics *Graphics)
{
  TIMED_FUNCTION();
  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClearDepth(1.0f);

#if BONSAI_INTERNAL
  debug_text_render_group *TextRG = &GetDebugState()->TextRenderGroup;
  glBindFramebuffer(GL_FRAMEBUFFER, TextRG->FBO.ID);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

  // FIXME(Jesse): This is taking _forever_ on Linux (GLES) .. does it take
  // forever on other Linux systems?
  glBindFramebuffer(GL_FRAMEBUFFER, Graphics->gBuffer->FBO.ID);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBindFramebuffer(GL_FRAMEBUFFER, Graphics->SG->FramebufferName);
  glClear(GL_DEPTH_BUFFER_BIT);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return;
}

void
BufferChunkMesh(
    untextured_3d_geometry_buffer *Dest,
    chunk_dimension WorldChunkDim,
    chunk_data *Chunk,
    world_position WorldP,
    graphics *Graphics,
    r32 Scale = 1.0f,
    v3 Offset = V3(0)
  )
{
  TIMED_FUNCTION();

  if ( Chunk->Mesh.At == 0)
    return;

#if DEBUG_CHUNK_AABB
  DEBUG_DrawChunkAABB(Dest, Graphics, WorldP, WorldChunkDim, PINK, 0.1f);
#endif

  v3 ModelBasisP =
    GetRenderP( WorldChunkDim, Canonical_Position(Offset, WorldP), Graphics->Camera);

  BufferVertsChecked(Dest, Graphics, Chunk->Mesh.At,
      Chunk->Mesh.Verts, Chunk->Mesh.Normals, Chunk->Mesh.Colors,
      ModelBasisP, V3(Scale));

  return;
}

#if 0
line
FindIntersectingLine(
  world_chunk *Chunk,
  voxel_position OffsetVector,
  int FirstFilledIndex)
{
  voxel_position MinP = Chunk->Data->BoundaryVoxels[FirstFilledIndex].Offset;
  voxel_position MaxP = MinP;

  int CurrentMaxLen = 0;
  int CurrentMinLen = 0;

  for (int VoxelIndex = FirstFilledIndex;
      VoxelIndex < Chunk->Data->BoundaryVoxelCount;
      ++ VoxelIndex)
  {
    voxel_position P  = Chunk->Data->BoundaryVoxels[VoxelIndex].Offset;

    s32 OriginToP = LengthSq(P);

    // Max
    if ( OriginToP > CurrentMaxLen )
    {
      MaxP = P + 1;
      CurrentMaxLen = OriginToP;
    }

    // Min
    if ( OriginToP < CurrentMinLen )
    {
      MinP = P;
      CurrentMinLen = OriginToP;
    }

  }

  line Result(MinP, MaxP);

  return Result;
}
#endif

#if 0
inline void
SetupAndBuildExteriorBoundary(
  world *World,
  world_chunk *Chunk,
  voxel_position OffsetVector,
  chunk_flag Flag)
{
  if ( IsSet(Chunk, Flag ) )
  {
    world_chunk *Neighbor = GetWorldChunk( World, Chunk->WorldP + OffsetVector );

    if ( Neighbor && IsSet( Neighbor, Chunk_Initialized) )
    {
      UnSetFlag( Chunk, Flag );
      BuildExteriorBoundaryVoxels( Chunk, World->ChunkDim, Neighbor, OffsetVector);
    }
  }

}
#endif

#if 0
aabb
FindBoundaryVoxelsAABB(chunk_data *Chunk, chunk_dimension Dim)
{

  chunk_dimension MinP = Dim;
  chunk_dimension MaxP = {};

  for ( s32 VoxelIndex = 0;
        VoxelIndex < Chunk->BoundaryVoxelCount;
        ++VoxelIndex)
  {
    voxel_position P = Chunk->BoundaryVoxels[VoxelIndex].Offset;

    if (P.x < MinP.x)
      MinP.x = P.x;
    if (P.x > MaxP.x)
      MaxP.x = P.x;

    if (P.y < MinP.y)
      MinP.y = P.y;
    if (P.y > MaxP.y)
      MaxP.y = P.y;

    if (P.z < MinP.z)
      MinP.z = P.z;
    if (P.z > MaxP.z)
      MaxP.z = P.z;
  }

  v3 Center = V3(MaxP - MinP);
  v3 Radius = Center/2;

  return aabb( Center, Radius );
}
#endif

inline v3
GetSign(v3 P)
{
  v3 Result =
    V3( GetSign(P.x), GetSign(P.y), GetSign(P.z));

  return Result;
}

inline voxel_position
GetSign(voxel_position P)
{
  voxel_position Result =
    Voxel_Position( GetSign(P.x), GetSign(P.y), GetSign(P.z));

  return Result;
}

voxel_position
RayTraceCollision(chunk_data *Chunk, chunk_dimension Dim, v3 StartingP, v3 Ray, v3 CenteringRay)
{
  Assert(LengthSq(Ray) == 1);
  v3 Result = V3(-1,-1,-1);

  // Clamp magnitude of this ray to 1 in each axis
  CenteringRay = GetSign(CenteringRay);

  v3 CurrentP = StartingP;
  while ( IsInsideDim(Dim, CurrentP) )
  {
    v3 CachedP = CurrentP;

    while ( IsInsideDim(Dim, CurrentP) )
    {
      if ( IsFilledInChunk(Chunk, Voxel_Position(CurrentP), Dim) )
      {
        Result = CurrentP;
        goto finished;
      }

      CurrentP += Ray;
    }

    CurrentP = CachedP;
    CurrentP += CenteringRay;
  }

  finished:

  return Voxel_Position(Result);
}

inline void
BufferTriangle(untextured_3d_geometry_buffer *Mesh, graphics *Graphics, v3 *Verts, v3 Normal, s32 ColorIndex)
{
  v3 VertBuffer[3];
  v3 NormalBuffer[3] = {Normal, Normal, Normal};

  // TODO(Jesse): Is this necessary to avoid some pointer aliasing bug?
  memcpy( VertBuffer, Verts, 9 * sizeof(r32) );

  v4 FaceColors[FACE_VERT_COUNT];
  FillColorArray(ColorIndex, FaceColors, FACE_VERT_COUNT);;

  BufferVertsChecked(
    Mesh,
    Graphics,
    3,
    VertBuffer,
    NormalBuffer,
    FaceColors);

}

#if 0
#define POINT_BUFFER_SIZE 8
struct point_buffer
{
  s32 Count;
  voxel_position Points[POINT_BUFFER_SIZE];
};

inline void
FindBoundaryVoxelsAlongEdge(
    chunk_data *Data,
    chunk_dimension Dim,
    voxel_position Start,
    voxel_position Iter,
    point_buffer *PB
  )
{
  voxel_position CurrentP = Start;
  b32 StartIsFilled = IsFilledInChunk(Data, CurrentP, Dim);

  Assert(Length(V3(Iter)) == 1.0f);

  while ( IsInsideDim(Dim, CurrentP) )
  {

    b32 CurrentPIsFilled = IsFilledInChunk(Data, CurrentP, Dim);
    if (CurrentPIsFilled != StartIsFilled)
    {
      Assert(PB->Count < POINT_BUFFER_SIZE);
      PB->Points[PB->Count++] = CurrentP;
      StartIsFilled = CurrentPIsFilled;
    }

    CurrentP += Iter;
  }

  return;
}
#endif

#if 0
void
Compute0thLod(world_chunk *WorldChunk, chunk_dimension WorldChunkDim)
{
  /* v3 RenderOffset = GetRenderP( WorldChunkDim, WorldChunk->WorldP, GameState->Camera); */

  v3 SurfaceNormal = {};
  v3 ChunkMidpoint = WorldChunkDim/2.0f;
  /* DEBUG_DrawPointMarker(world, ChunkMidpoint + RenderOffset, GREEN, 0.5f); */

  s32 WorldChunkVolume = Volume(WorldChunkDim);


  b32 HalfFull = WorldChunk->Filled >= WorldChunkVolume/2 ? True : False ;
  b32 HalfEmpty = !HalfFull;


  // Loop through the chunk and find the surface normal
  for ( s32 VoxelIndex = 0;
      VoxelIndex < WorldChunkVolume;
      ++VoxelIndex)
  {
    voxel_position VoxelP = GetPosition(VoxelIndex, WorldChunkDim);

    /* if (IsBoundaryVoxel(chunk, VoxelP, WorldChunkDim)) */
    /*   DEBUG_DrawPointMarker(world, V3(VoxelP) + RenderOffset, PINK, 0.25f); */

    // TODO(Jesse): Pretty sure we can do some XOR trickery or something
    // here to avoid this branch, which could be a large perf win
    if ( HalfFull && NotFilledInChunk( WorldChunk->Data, VoxelP, WorldChunkDim ) )
    {
      SurfaceNormal += Normalize( VoxelP - ChunkMidpoint );
    }
    else if ( HalfEmpty && IsFilledInChunk( WorldChunk->Data, VoxelP, WorldChunkDim ) )
    {
      SurfaceNormal += Normalize( VoxelP - ChunkMidpoint );
    }
  }

  if (HalfEmpty)
    SurfaceNormal *= -1.0f;

  // FIXME(Jesse): Sometimes the surface is perfectly balanced within the
  // chunk, in which case the normal turns out to be zero.
  SurfaceNormal = Normalize(SurfaceNormal);
  if ( Length(SurfaceNormal) == 0 )
  {
    // DEBUG_DrawChunkAABB( WorldChunk, GameState->Camera, WorldChunkDim, Quaternion() , RED );
    return;
  }

  WorldChunk->Normal = SurfaceNormal;

  /* DEBUG_DrawVectorAt(world, RenderOffset + ChunkMidpoint - (SurfaceNormal*10), SurfaceNormal*20, BLACK, 0.5f ); */

  point_buffer *PB = &WorldChunk->PB;

  {
    voxel_position Start = Voxel_Position(0, 0, 0);

    {
      voxel_position Iter  = Voxel_Position(1, 0, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 1, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0, 1);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
  }

  {
    voxel_position Start = Voxel_Position(0, WorldChunkDim.y-1, WorldChunkDim.z-1);

    {
      voxel_position Iter  = Voxel_Position(1, 0, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0,-1, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0,-1);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
  }

  {
    voxel_position Start = Voxel_Position(WorldChunkDim.x-1, WorldChunkDim.y-1, 0);

    {
      voxel_position Iter  = Voxel_Position(-1, 0, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0,-1, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0, 1);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
  }

  {
    voxel_position Start = Voxel_Position(WorldChunkDim.x-1, 0, WorldChunkDim.z-1);

    {
      voxel_position Iter  = Voxel_Position(-1, 0, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 1, 0);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
    {
      voxel_position Iter  = Voxel_Position(0, 0,-1);
      FindBoundaryVoxelsAlongEdge(WorldChunk->Data, WorldChunkDim, Start, Iter, PB);
    }
  }



  // Sort the vertices based on distance to closest points
  for (s32 PBIndexOuter = 0;
      PBIndexOuter < PB->Count;
      ++PBIndexOuter)
  {
    voxel_position CurrentVert = PB->Points[PBIndexOuter];
    s32 ShortestLength = INT_MAX;

    // Loop through remaining points and find next closest point, collapsing
    // points that are very close to each other
    for (s32 PBIndexInner = PBIndexOuter + 1;
        PBIndexInner < PB->Count;
        ++PBIndexInner)
    {
      s32 TestLength = LengthSq(CurrentVert - PB->Points[PBIndexInner]);

      /* if ( TestLength < 11 ) */
      /* { */
      /*   PB->Points[PBIndexOuter] = PB->Points[PB->Count-1]; */
      /*   PBIndexInner--; */
      /*   PB->Count--; */
      /*   continue; */
      /* } */

      if ( TestLength < ShortestLength )
      {
        ShortestLength = TestLength;

        voxel_position Temp = PB->Points[PBIndexOuter + 1];
        PB->Points[PBIndexOuter + 1] = PB->Points[PBIndexInner];
        PB->Points[PBIndexInner] = Temp;
      }
    }

  }

    /* if (PB->Count == 5) */
#if 0
    b32 FoundMidpoint = false;
    v3 SurfaceMid = {};

    //
    // Raytrace along the surface normal starting at the BoundaryVoxels AABB Midpoint
    // to find the surfaces mid point
    //
    aabb BoundaryVoxelsAABB = FindBoundaryVoxelsAABB(WorldChunk->Data, WorldChunkDim);
    v3 MidAABB = Midpoint(BoundaryVoxelsAABB);
    b32 MidpointIsFilled = IsFilledInChunk(WorldChunk->Data, Voxel_Position(MidAABB), WorldChunkDim);

    {
      v3 CurrentP = MidAABB;
      while( IsInsideDim(WorldChunkDim, CurrentP) )
      {
        DEBUG_DrawPointMarker(world, CurrentP + RenderOffset, PINK, 0.25f);

        b32 CurrentPIsFilled = IsFilledInChunk(WorldChunk->Data, Voxel_Position(CurrentP), WorldChunkDim);
        if (CurrentPIsFilled != MidpointIsFilled)
        {
          FoundMidpoint = true;
          SurfaceMid = CurrentPIsFilled ? CurrentP : CurrentP - SurfaceNormal;
          break;
        }
        CurrentP += SurfaceNormal;
      }
    }


    {
      if ( !FoundMidpoint )
      {
        v3 CurrentP = MidAABB;
        while( IsInsideDim(WorldChunkDim, CurrentP) )
        {
          DEBUG_DrawPointMarker(world, CurrentP + RenderOffset, PINK, 0.25f);

          b32 CurrentPIsFilled = IsFilledInChunk(WorldChunk->Data, Voxel_Position(CurrentP), WorldChunkDim);
          if (CurrentPIsFilled != MidpointIsFilled)
          {
            FoundMidpoint = true;
            SurfaceMid = CurrentPIsFilled ? CurrentP : CurrentP + SurfaceNormal;
            break;
          }
          CurrentP -= SurfaceNormal;
        }
      }
    }

    // FIXME(Jesse): Why would we fail to find a surface midpoint pray tell?
    if ( !FoundMidpoint )
    {
      DEBUG_DrawChunkAABB( world, WorldChunk, GameState->Camera, Quaternion() , RED );
    }
    else
    {
      v3 Front = SurfaceNormal;
      v3 Right = Cross( WORLD_Y, Front );
      v3 Up = Cross( Front, Right );

      DEBUG_DrawVectorAt(world, RenderOffset + SurfaceMid, Front*10, RED, 0.5f );
      DEBUG_DrawVectorAt(world, RenderOffset + SurfaceMid, Right*10, TEAL, 0.5f );
      DEBUG_DrawVectorAt(world, RenderOffset + SurfaceMid, Up*10, GREEN, 0.5f );

      v3 MidpointRenderP = RenderOffset + SurfaceMid;

      DEBUG_DrawPointMarker(world, MidpointRenderP, GREEN, 0.5f);

      {

        v3 UpRight = Normalize(Up + Right);
        v3 UpLeft = Normalize(Up - Right);

        v3 DownRight = Normalize(Right - Up);
        v3 DownLeft = Normalize((Up + Right) * -1.0f);

        v3 VertMaxMax = SurfaceMid;
        while ( IsInsideDim(WorldChunkDim, VertMaxMax) ) VertMaxMax += UpRight;
        VertMaxMax -= UpRight;

        v3 VertMaxMin = SurfaceMid;
        while ( IsInsideDim(WorldChunkDim, VertMaxMin) ) VertMaxMin += UpLeft;
        VertMaxMin -= UpLeft;

        v3 VertMinMax = SurfaceMid;
        while ( IsInsideDim(WorldChunkDim, VertMinMax) ) VertMinMax += DownRight;
        VertMinMax -= DownRight;

        v3 VertMinMin = SurfaceMid;
        while ( IsInsideDim(WorldChunkDim, VertMinMin) ) VertMinMin += DownLeft;
        VertMinMin -= DownLeft;

        v3 Verts[3];

        Verts[0] = VertMaxMax + RenderOffset;
        Verts[1] = VertMaxMin + RenderOffset;
        Verts[2] = VertMinMin + RenderOffset;
        BufferTriangle(world, &Verts[0], SurfaceNormal, 42);

        Verts[0] = VertMinMin + RenderOffset;
        Verts[1] = VertMinMax + RenderOffset;
        Verts[2] = VertMaxMax + RenderOffset;
        BufferTriangle(world, &Verts[0], SurfaceNormal, 42);

      }
    }

#endif

#if 0
    aabb BoundaryVoxelsAABB = FindBoundaryVoxelsAABB(WorldChunk->Data, WorldChunkDim);

    float xLen = BoundaryVoxelsAABB.Max.x - BoundaryVoxelsAABB.Min.x;
    float yLen = BoundaryVoxelsAABB.Max.y - BoundaryVoxelsAABB.Min.y;
    float zLen = BoundaryVoxelsAABB.Max.z - BoundaryVoxelsAABB.Min.z;

    // Find the widest axies and iterate orthogonal to them

    voxel_position IterAxis = {}; // Axis to iterate along

    // X Axis is widest
    if (xLen >= yLen && xLen >= zLen)
    {
      // Y is next widest
      if (yLen > zLen)
      {
        IterAxis.z = 1;
      }
      else // Z is next widest
      {
        IterAxis.y = 1;
      }
    }

    // Y axis is widest
    else if (yLen >= xLen && yLen >= zLen)
    {
      // x is next widest
      if (xLen > zLen)
      {
        IterAxis.z = 1;
      }
      else // z is next widest
      {
        IterAxis.x = 1;
      }
    }

    // Z is widest
    else if (zLen >= yLen && zLen >= xLen)
    {
      // X is next widest
      if (xLen > yLen)
      {
        IterAxis.y = 1;
      }
      else // Y is next widest
      {
        IterAxis.x = 1;
      }
    }
    else // Use an arbitrary axis
    {
      if (xLen > yLen)
      {
        IterAxis.y = 1;
      }
      else
      {
        IterAxis.x = 1;
      }
    }

    /* Assert(Length(IterAxis) == 1); */

    if ( GameState->Player->P.WorldP == WorldChunk->WorldP)
      DEBUG_DrawAABB( world, BoundaryVoxelsAABB + RenderOffset, Quaternion(), RED, 0.25f );

    //
    // Start iterating along the axis orthogonal to the widest axies
    voxel_position MaxP = IterAxis * World->ChunkDim;


    s32 AxisMax = max(MaxP.x, MaxP.y);
    AxisMax = max(AxisMax, MaxP.z);


    // Vector pointing towards the position to iterate from
    voxel_position MaxAxies = IterAxis ^ Voxel_Position(1,1,1);


    voxel_position StartingP = Voxel_Position(BoundaryVoxelsAABB.MinCorner + (((BoundaryVoxelsAABB.MaxCorner-BoundaryVoxelsAABB.MinCorner)*MaxAxies)/2));

    s32 MaxIterations = (s32)Length( (BoundaryVoxelsAABB.MaxCorner*IterAxis) - (BoundaryVoxelsAABB.MinCorner*IterAxis) );

    voxel_position CurrentP = StartingP;

    b32 FoundMidpoint = False;

    for (s32 Iterations = 0;
        Iterations < MaxIterations;
        ++Iterations)
    {

      if ( GameState->Player->P.WorldP == WorldChunk->WorldP)
        DEBUG_DrawPointMarker(world, V3(CurrentP + RenderOffset), BLUE, 0.25f);

      if (IsBoundaryVoxel(chunk, CurrentP, World->ChunkDim))
      {
        DEBUG_DrawPointMarker(world, V3(CurrentP + RenderOffset), GREEN, 1.2f);
        FoundMidpoint = True;
        break;
      }

      CurrentP += IterAxis;
    }

    if (FoundMidpoint)
    {
      voxel_position BoundaryPosition =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(-1, 0, 0));
      voxel_position BoundaryPosition2 =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(1, 0, 0));

      DEBUG_DrawPointMarker(world, V3(BoundaryPosition + RenderOffset)+0.1f, RED, 1.2f);
      DEBUG_DrawPointMarker(world, V3(BoundaryPosition2 + RenderOffset)-0.1f, RED, 1.2f);



      BoundaryPosition =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(0, 0, -1));
      BoundaryPosition2 =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(0, 0, 1));

      DEBUG_DrawPointMarker(world, V3(BoundaryPosition + RenderOffset)+0.1f, TEAL, 1.2f);
      DEBUG_DrawPointMarker(world, V3(BoundaryPosition2 + RenderOffset)-0.1f, TEAL, 1.2f);



      BoundaryPosition =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(0, -1, 0));
      BoundaryPosition2 =
        TraverseSurfaceToBoundary(world, chunk, CurrentP, Voxel_Position(0, 1, 0));

      DEBUG_DrawPointMarker(world, V3(BoundaryPosition + RenderOffset), GREEN, 1.2f);
      DEBUG_DrawPointMarker(world, V3(BoundaryPosition2 + RenderOffset), GREEN, 1.2f);


    }


    v3 MaxMaxStart = BoundaryVoxelsAABB.MaxCorner;

    v3 MaxMinStart = BoundaryVoxelsAABB.MaxCorner;
    MaxMinStart.x = BoundaryVoxelsAABB.MinCorner.x;

    v3 MinMaxStart = BoundaryVoxelsAABB.MaxCorner;
    MinMaxStart.z = BoundaryVoxelsAABB.MinCorner.z;

    v3 MinMinStart = BoundaryVoxelsAABB.MinCorner;
    MinMinStart.y = BoundaryVoxelsAABB.MaxCorner.y;


    voxel_position MaxMax = RayTraceCollision( chunk,
                                               WorldChunkDim,
                                               MaxMaxStart,
                                               V3(0,-1,0),
                                               MinMinStart - MaxMaxStart );

    voxel_position MaxMin = RayTraceCollision( chunk,
                                               WorldChunkDim,
                                               MaxMinStart,
                                               V3(0,-1,0),
                                               MinMaxStart - MaxMinStart );

    voxel_position MinMax = RayTraceCollision( chunk,
                                               WorldChunkDim,
                                               MinMaxStart,
                                               V3(0,-1,0),
                                               MaxMinStart - MinMaxStart );

    voxel_position MinMin = RayTraceCollision( chunk,
                                               WorldChunkDim,
                                               MinMinStart,
                                               V3(0,-1,0),
                                               MaxMaxStart - MinMinStart );


    v3 VertMaxMax = V3(MaxMax + RenderOffset + GetSign(MaxMax) );
    v3 VertMaxMin = V3(MaxMin + RenderOffset + GetSign(MaxMin) );
    v3 VertMinMin = V3(MinMin + RenderOffset + GetSign(MinMin) );
    v3 VertMinMax = V3(MinMax + RenderOffset + GetSign(MinMax) );

    /* DEBUG_DrawLine(world, VertMaxMax, VertMaxMin, BLUE, 0.1f); */
    /* DEBUG_DrawLine(world, VertMinMin, VertMinMax, RED,  0.1f); */

    /* DEBUG_DrawLine(world, VertMaxMax, VertMinMax, BLUE, 0.1f); */
    /* DEBUG_DrawLine(world, VertMinMin, VertMaxMin, RED,  0.1f); */

    v3 Verts[3];

    Verts[0] = VertMaxMax;
    Verts[1] = VertMaxMin;
    Verts[2] = VertMinMin;
    BufferTriangle(world, &Verts[0], 42);

    Verts[0] = VertMinMin;
    Verts[1] = VertMinMax;
    Verts[2] = VertMaxMax;
    BufferTriangle(world, &Verts[0], 42);

    // Draw Boundary voxels aabb
    /* aabb RenderCorrectedAABB = BoundaryVoxelsAABB + RenderOffset; */
    /* RenderCorrectedAABB.MaxCorner += V3(1,1,1); */
    /* DEBUG_DrawAABB( world, RenderCorrectedAABB, Quaternion(), TEAL, 0.10f ); */

#endif


/* chunk->flags = SetFlag(chunk->flags, Chunk_LodGenerated); */

  return;
}
#endif

inline b32
IsBoundaryVoxel(chunk_data *Chunk, voxel_position Offset, chunk_dimension Dim)
{
  s32 VoxelIndex = GetIndex(Offset, Dim);
  voxel *V = &Chunk->Voxels[VoxelIndex];

  b32 Result = False;
  Result |= IsSet( V, Voxel_BackFace);
  Result |= IsSet( V, Voxel_FrontFace);
  Result |= IsSet( V, Voxel_TopFace);
  Result |= IsSet( V, Voxel_BottomFace);
  Result |= IsSet( V, Voxel_LeftFace);
  Result |= IsSet( V, Voxel_RightFace);

  return Result;
}

inline void
CheckAndIncrementCurrentP(
    chunk_data *Chunk,
    chunk_dimension Dim,
    voxel_position *CurrentP,
    s32 *CurrentClosestDistanceSq,
    voxel_position TargetP,
    voxel_position TestP
  )
{
  if ( IsInsideDim(Dim, TestP) )
  {
    s32 DistSq = LengthSq(TargetP - TestP);

    if ( (DistSq > *CurrentClosestDistanceSq) && IsBoundaryVoxel(Chunk, TestP, Dim) )
    {
      *CurrentP = TestP;
      *CurrentClosestDistanceSq = DistSq;
    }
  }

  return;
}

voxel_position
TraverseSurfaceToBoundary(
    world *World,
    chunk_data *Chunk,
    voxel_position StartingP,
    voxel_position IterDir
  )
{
  s32 CurrentClosestDistanceSq = 0;
  voxel_position TargetP = (IterDir * World->ChunkDim) - IterDir;

  voxel_position CurrentP = StartingP;

  voxel_position Up      = Voxel_Position(WORLD_Y);
  voxel_position Right   = Voxel_Position(WORLD_X);
  voxel_position Forward = Voxel_Position(WORLD_Z);


  voxel_position LoopStartingP = {};
  while (IsInsideDim(World->ChunkDim, CurrentP) )
  {
    LoopStartingP = CurrentP;

    // Single axies
    voxel_position PUp      = CurrentP + Up;
    voxel_position PDown    = CurrentP - Up;

    voxel_position PRight    = CurrentP + Right;
    voxel_position PLeft     = CurrentP - Right;

    voxel_position PForward = CurrentP + Forward;
    voxel_position PBack    = CurrentP - Forward;

    // Diagonal axies
    voxel_position PUpRight   = PUp   + Right;
    voxel_position PUpLeft    = PUp   - Right;
    voxel_position PDownRight = PDown + Right;
    voxel_position PDownLeft  = PDown - Right;

    // Forward diagonals
    voxel_position FwdUpRight   = PUpRight   + Forward;
    voxel_position FwdUpLeft    = PUpLeft    + Forward;
    voxel_position FwdDownRight = PDownRight + Forward;
    voxel_position FwdDownLeft  = PDownLeft  + Forward;

    // Backward diagonals
    voxel_position BackUpRight   = PUpRight   - Forward;
    voxel_position BackUpLeft    = PUpLeft    - Forward;
    voxel_position BackDownRight = PDownRight - Forward;
    voxel_position BackDownLeft  = PDownLeft  - Forward;

    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PUp);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PDown);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PLeft);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PForward);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PBack);

    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PUpRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PUpLeft);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PDownRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, PDownLeft);

    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, FwdUpRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, FwdUpLeft);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, FwdDownRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, FwdDownLeft);

    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, BackUpRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, BackUpLeft);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, BackDownRight);
    CheckAndIncrementCurrentP(Chunk, World->ChunkDim, &CurrentP, &CurrentClosestDistanceSq, TargetP, BackDownLeft);

    if (LoopStartingP == CurrentP)
      break;
  }

  return CurrentP;
}

#if 0
inline void
Draw0thLod(untextured_3d_geometry_buffer *Mesh, g_buffer_render_group *gBuffer, shadow_render_group *SG, camera *Camera, world_chunk *Chunk, v3 RenderOffset)
{
  /* for ( s32 PointIndex = 0; PointIndex < Chunk->PB.Count; ++PointIndex ) */
  /*   DEBUG_DrawPointMarker(world, V3(Chunk->PB.Points[PointIndex]) + RenderOffset, Color--, 1.0f); */


  v3 Verts[3] = {};
  Verts[0] = V3(Chunk->PB.Points[0]) + RenderOffset;


  s32 Color = 42;
  s32 VertIndex = 1;
  while ( (VertIndex + 1) < Chunk->PB.Count )
  {
    Verts[1] = V3(Chunk->PB.Points[VertIndex]) + RenderOffset;
    Verts[2] = V3(Chunk->PB.Points[++VertIndex]) + RenderOffset;
    BufferTriangle(Mesh, gBuffer, SG, Camera, &Verts[0], Chunk->Normal, Color);
  }

  return;
}
#endif

b32
CanBuildWorldChunkBoundary(world *World, world_chunk *Chunk)
{
  // FIXME(Jesse): This is _real_ bad for the cache!
  b32 Result = True;

  world_position ChunkP = Chunk->WorldP;

  // Bail early to save the cache most of the time.. does this even help?
  if (!Result) return Result;

  world_position Left  = ChunkP - Voxel_Position(1, 0, 0);
  world_position Right = ChunkP + Voxel_Position(1, 0, 0);
  world_position Top   = ChunkP + Voxel_Position(0, 1, 0);
  world_position Bot   = ChunkP - Voxel_Position(0, 1, 0);
  world_position Front = ChunkP + Voxel_Position(0, 0, 1);
  world_position Back  = ChunkP - Voxel_Position(0, 0, 1);

  world_chunk *TestChunk = GetWorldChunk( World, Left );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  TestChunk = GetWorldChunk( World, Right );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  TestChunk = GetWorldChunk( World, Top );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  TestChunk = GetWorldChunk( World, Bot );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  TestChunk = GetWorldChunk( World, Front );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  TestChunk = GetWorldChunk( World, Back );
  Result &= TestChunk && IsSet(TestChunk, Chunk_Initialized);

  return Result;
}

void
DrawFolie(untextured_3d_geometry_buffer *Mesh, graphics *Graphics, aabb *AABB)
{
  v3 RenderP = AABB->Center;
  DrawVoxel( Mesh, Graphics, RenderP, GREY, AABB->Radius*2);

  return;
}

#if 1
void
DrawParticle(
    untextured_3d_geometry_buffer *Source,
    untextured_3d_geometry_buffer *Dest,
    graphics *Graphics,
    u8 ColorIndex
  )
{
  v4 FaceColors[FACE_VERT_COUNT];
  FillColorArray(ColorIndex, FaceColors, FACE_VERT_COUNT);;
#if 1
  BufferVerts( Source, Dest, Graphics);
#else
  RightFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, RightFaceNormalData, FaceColors);

  LeftFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, LeftFaceNormalData, FaceColors);

  BottomFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, BottomFaceNormalData, FaceColors);

  TopFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, TopFaceNormalData, FaceColors);

  FrontFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, FrontFaceNormalData, FaceColors);

  BackFaceVertexData( MinP, V3(Diameter), VertexData);
  BufferVerts(Mesh, gBuffer, SG, Camera, 6, VertexData, BackFaceNormalData, FaceColors);
#endif

  return;
}
#endif

void
BufferEntity(
    untextured_3d_geometry_buffer *Mesh,
    entity *Entity,
    graphics *Graphics,
    chunk_dimension WorldChunkDim,
    r32 dt
  )
{
  TIMED_FUNCTION();
  // Debug light code
  /* v3 LightP = GetRenderP(world, Entity->P + Entity->Model.Dim/2); */
  /* glUniform3fv(RG->LightPID, 1, &LightP[0]); */
  //

  chunk_data *Model = Entity->Model.Chunk;
  if (Model && Spawned(Entity))
  {
#if DEBUG_DRAW_COLLISION_VOLUMES
    aabb AABB = GetRenderSpaceAABB(WorldChunkDim, Entity, Graphics->Camera);
    DEBUG_DrawAABB(Mesh, Graphics, AABB, PINK);
#endif

    animation* Animation = &Entity->Model.Animation;
    v3 AnimationOffset = {};
    Animation->t += dt;
    AnimationOffset = GetInterpolatedPosition(Animation);

    if (IsSet(Model, Chunk_Initialized))
    {
      BufferChunkMesh(Mesh, WorldChunkDim, Model, Entity->P.WorldP, Graphics, Entity->Scale, Entity->P.Offset + AnimationOffset);
    }
  }

  return;
}

void
BufferWorldChunk(
    world *World,
    world_chunk *Chunk,
    graphics *Graphics
  )
{
  chunk_data *ChunkData = Chunk->Data;
#if 1
  if (ChunkData->Flags == Chunk_Complete)
  {
    BufferChunkMesh( &World->Mesh, World->ChunkDim, ChunkData, Chunk->WorldP, Graphics);
  }
  else if (IsSet(ChunkData, Chunk_Queued))
  {
    DEBUG_DrawChunkAABB(&World->Mesh, Graphics, Chunk, WORLD_CHUNK_DIM, TEAL, 0.1f);
  }
  else
  {
    DEBUG_DrawChunkAABB(&World->Mesh, Graphics, Chunk, WORLD_CHUNK_DIM, RED, 0.1f);
  }

#else
  if (CanBuildWorldChunkBoundary(world, Chunk))
  {
    BuildWorldChunkMesh(world, Chunk);
    Compute0thLod(GameState, Chunk);
  }

  if ( Length(ChunkRenderOffset - CameraRenderOffset ) < MIN_LOD_DISTANCE )
  {
    r32 Scale = 1.0f;
    BufferChunkMesh( GameState->Plat, World, ChunkData, Chunk->WorldP, RG, Camera, Scale);
  }

  else
  {
    Draw0thLod( GameState, Chunk, ChunkRenderOffset);
  }

#endif

  return;
}

void
BufferWorld(world *World, graphics *Graphics)
{
  TIMED_FUNCTION();

  world_position VisibleRadius = World_Position(VR_X, VR_Y, VR_Z)/2;

  world_position Min = World->Center - VisibleRadius;
  world_position Max = World->Center + VisibleRadius + 1;

  for ( s32 ChunkIndex = 0;
        ChunkIndex < WORLD_HASH_SIZE;
        ++ChunkIndex)
  {
    world_chunk *Chunk = World->ChunkHash[ChunkIndex];

    while (Chunk)
    {
      if ( (Chunk->WorldP >= Min && Chunk->WorldP < Max) )
      {
        BufferWorldChunk(World, Chunk, Graphics);
        Chunk = Chunk->Next;
      }
      else
      TIMED_BLOCK("Free World Chunk");
        DEBUG_DrawChunkAABB(&World->Mesh, Graphics, Chunk, WORLD_CHUNK_DIM, PINK, 0.1f);
        world_chunk *ChunkToFree = Chunk;
        Chunk = Chunk->Next;
        FreeWorldChunk(World, ChunkToFree);
      END_BLOCK("Free World Chunk");
    }
  }
}

void
BufferEntities( entity **EntityTable, untextured_3d_geometry_buffer *Mesh,
                graphics *Graphics, world *World, r32 dt)
{
  TIMED_FUNCTION();
  for ( s32 EntityIndex = 0;
        EntityIndex < TOTAL_ENTITY_COUNT;
        ++EntityIndex)
  {
    entity *Entity = EntityTable[EntityIndex];
    BufferEntity( Mesh, Entity, Graphics, World->ChunkDim, dt);
  }

  return;
}
