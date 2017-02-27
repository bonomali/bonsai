
#include <bonsai.h>
#include <render.h>

#include <bonsai.cpp>
#include <constants.hpp>

#include <time.h>

void
RegenerateGameWorld( World *world, Entity *Player )
{
  printf("\n\n\n\n\n");
  srand(time(NULL));
  world->VisibleRegionOrigin = World_Position(0,0,0);
  do
  {
    PerlinNoise Noise(rand());
    world->Noise = Noise;
    ZeroWorldChunks(world);
    GenerateVisibleRegion( world , Voxel_Position(0,0,0) );
  } while (!SpawnPlayer( world, Player ) );
}

void
GAME_UPDATE_AND_RENDER
(
    World *world,
    Entity *Player,
    Camera_Object *Camera,
    float dt,

    RenderGroup *RG,
    ShadowRenderGroup *SG
  )
{
  if ( glfwGetKey(window, GLFW_KEY_ENTER ) == GLFW_PRESS )
    RegenerateGameWorld(world, Player);

  // TODO : Bake these into the terrain/model ?
  v3 drag = V3(0.6f, 1.0f, 0.6f);

  v3 Input = GetInputsFromController(Camera);
  Player->Acceleration = Input * PLAYER_ACCEL_MULTIPLIER; // m/s2

  if (IsGrounded(world, Player) && (glfwGetKey( window, GLFW_KEY_SPACE ) == GLFW_PRESS))
      Player->Velocity.y += PLAYER_JUMP_STRENGTH; // Jump

  Player->Acceleration += world->Gravity * dt; // Apply Gravity
  Player->Velocity = (Player->Velocity + (Player->Acceleration )) * drag; // m/s

  v3 PlayerDelta = Player->Velocity * dt;

  UpdatePlayerP( world, Player, PlayerDelta );
  UpdateCameraP( world, Player, Camera );
  RG->Basis.ViewMatrix = GetViewMatrix(world, Camera);

  if (Length(Input) > 0)
    Player->Rotation = LookAt(Input);

  GlobalLightTheta += dt;

  //
  // Draw world

  ClearFramebuffers(RG, SG);

  for ( int i = 0; i < Volume(world->VisibleRegion); ++ i )
  {
#if DEBUG_SUSPEND_DRAWING_WORLD
    World_Chunk *chunk = &world->Chunks[i];
    DrawWorldChunk(
      world,
      chunk,
      Camera,
      RG,
      SG
    );
#endif

  }

  DrawEntity(
    world,
    Player,
    Camera,
    RG,
    SG
  );

  FlushRenderBuffers(world, RG, SG, Camera);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(RG->LightingShader);
  glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

  glm::vec3 GlobalLightDirection =  glm::vec3( sin(GlobalLightTheta), 1.0, -2.0);
  GlobalLightDirection = glm::normalize( GlobalLightDirection );

  glUniform3fv(RG->GlobalLightDirectionID, 1, &GlobalLightDirection[0]);

  glm::mat4 biasMatrix(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
  );

  glm::mat4 depthBiasMVP = biasMatrix * GetDepthMVP(world, Camera);
  glUniformMatrix4fv(RG->DepthBiasMVPID, 1, GL_FALSE, &depthBiasMVP[0][0]);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, RG->ColorTexture);
  glUniform1i(RG->ColorTextureUniform, 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, RG->NormalTexture);
  glUniform1i(RG->NormalTextureUniform, 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, RG->PositionTexture);
  glUniform1i(RG->PositionTextureUniform, 2);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, SG->Texture);
  glUniform1i(RG->ShadowMapTextureUniform, 3);

#if DEBUG_DRAW_SHADOW_MAP_TEXTURE
  glUseProgram(RG->SimpleTextureShaderID);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, SG->Texture);
  glUniform1i(RG->SimpleTextureUniform, 0);

#endif

  RenderQuad(RG);


  glfwSwapBuffers(window);
  glfwPollEvents();


  /* printf("%d Triangles drawn\n", tris ); */
  /* tris=0; */

  /* printf("%d Voxels Indexed\n", VoxelsIndexed ); */
  /* VoxelsIndexed=0; */

  /* printf("%d Boundary Voxels Indexed\n", BoundaryVoxelsIndexed ); */
  /* BoundaryVoxelsIndexed=0; */
}

void
FillChunk(Chunk *chunk)
{
  for (int i = 0; i < Volume(chunk->Dim); ++i)
  {
    chunk->Voxels[i].flags = SetFlag(chunk->Voxels[i].flags , Voxel_Filled);
  }
}

int
main( void )
{
  int width, height;

  width = 1920;
  height = 1080;

  initWindow(width, height);

  ShadowRenderGroup SG = {};
  if (!InitializeShadowBuffer(&SG)) { printf("Error initializing Shadow Buffer\n"); return False; }

  RenderGroup RG = {};
  if (!InitializeRenderGroup(&RG)) { printf("Error initializing RenderGroup\n"); return False; }

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK); // Cull back-facing triangles -> draw only front-facing triangles

  // This is necessary!
  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);

  glfwWindowHint(GLFW_SAMPLES, 4);

  World world;
  AllocateWorld(&world);

  Entity Player;

  /* Player.Model = LoadVox("./chr_knight.vox"); */
  /* Player.Model = LoadVox("./ephtracy.vox"); */
  /* Player.Model = LoadVox("./chr_sword.vox"); */
  /* Player.Model = LoadVox("./shelf.vox"); */
  /* Player.Model = LoadVox("./3x3x3.vox"); */
  /* Player.Model = LoadVox("./8x8x8.vox"); */
  /* Player.Model = LoadVox("./alien_bot2.vox"); */
  Player.Model = LoadVox("./chr_rain.vox");
  /* Player.Model = LoadVox("./chr_old.vox"); */
  /* Player.Model = AllocateChunk(Chunk_Dimension(13,7,7), World_Position(0,0,0)); */
  /* FillChunk(&Player.Model); */
  Player.Model.flags = SetFlag( Player.Model.flags, Chunk_Entity);

  Camera_Object Camera = {};
  Camera.Frust.farClip = 500.0f;
  Camera.Frust.nearClip = 0.1f;
  Camera.Frust.width = 30.0f;
  Camera.Frust.FOV = 45.0f;
  Camera.P = CAMERA_INITIAL_P;

  glm::mat4 Projection = glm::perspective(
      glm::radians(Camera.Frust.FOV),
      (float)width/(float)height, // display ratio
      Camera.Frust.nearClip,
      Camera.Frust.farClip);

  do
  {
    srand(time(NULL));
    PerlinNoise Noise(rand());
    world.Noise = Noise;

    ZeroWorldChunks(&world);
    GenerateVisibleRegion( &world , Voxel_Position(0,0,0) );
    /* printf("spawning plyawer\n"); */
  } while (!SpawnPlayer( &world, &Player ) );

  double lastTime = glfwGetTime();


  /*
   *  Main Render loop
   *
   */
  do
  {
    double currentTime = glfwGetTime();
    float dt = (float)(currentTime - lastTime);
    lastTime = currentTime;

    accumulatedTime += dt;
    numFrames ++;


    timespec T1;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &T1);

    CALLGRIND_START_INSTRUMENTATION;
    CALLGRIND_TOGGLE_COLLECT;

    RG.Basis.ProjectionMatrix = Projection;

    GAME_UPDATE_AND_RENDER(
      &world,
      &Player,
      &Camera,
      dt,

      &RG,
      &SG
    );

    CALLGRIND_TOGGLE_COLLECT;

    timespec T2;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &T2);

    if ( T2.tv_sec - T1.tv_sec > 0 ) T1.tv_nsec -= 1000000000;

    /* printf(" %d ms this frame \n\n\n", */
    /*     (int)(T2.tv_nsec -T1.tv_nsec)/1000000 ); */

    /* printf(" %d triangles \n", tris); */
    tris=0;

  } // Check if the ESC key was pressed or the window was closed
  while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
         glfwGetKey(window, GLFW_KEY_Q )      != GLFW_PRESS &&
         glfwWindowShouldClose(window) == 0                 &&
         DEBUG_FRAMES_TO_RUN != numFrames );

  CALLGRIND_DUMP_STATS;

  glDeleteBuffers(1, &RG.vertexbuffer);
  glDeleteBuffers(1, &RG.colorbuffer);
  glDeleteBuffers(1, &RG.normalbuffer);

  glDeleteVertexArrays(1, &VertexArrayID);
  glDeleteProgram(RG.ShaderID);

  // Close OpenGL window and terminate GLFW
  glfwTerminate();
  /* glfwDestroyWindow(window); */

  for ( int i = 0; i < Volume(world.VisibleRegion) ; ++ i )
  {
    free( world.Chunks[i].Data.Voxels );
    free( world.Chunks[i].Data.BoundaryVoxels );
  }

  free( world.VertexData.Data );
  free( world.ColorData.Data );
  free( world.NormalData.Data );

  free( world.Chunks );
  free( world.FreeChunks.chunks );

  free(Player.Model.Voxels);
  free(Player.Model.BoundaryVoxels);

  return 0;
}

