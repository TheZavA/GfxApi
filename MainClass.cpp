
#include "MainClass.h"

#include "GfxApi.h"

//#include <GLFW/glfw3.h>

#include <boost/make_shared.hpp>

#include "voxel/NodeChunk.h"

#include "voxel/ChunkManager.h"

#include "Input.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <btBulletDynamicsCommon.h>

#include <math.h>
#include <minmax.h>



boost::shared_ptr<GfxApi::Texture> textureFromBmp( const std::string& path )
{
   int width = 100;
   int height = 100;
   using namespace GfxApi;

   int depth = 1;


   printf( "Reading image %s\n", path.c_str() );

   // Data read from the header of the BMP file
   unsigned char header[54];
   unsigned int dataPos;
   unsigned int imageSize;

   // Actual RGB data
   unsigned char * data;

   FILE * file = fopen( path.c_str(), "rb" );
   if( !file )
   {
      throw std::runtime_error( "Image path could not be opened. Are you in the right directory ? " );
   }

   // Read the header, i.e. the 54 first bytes

   // If less than 54 bytes are read, problem
   if( fread( header, 1, 54, file ) != 54 )
   {
      throw std::runtime_error( "Not a correct BMP file" );
   }

   // A BMP files always begins with "BM"
   if( header[0] != 'B' || header[1] != 'M' )
   {
      throw std::runtime_error( "Not a correct BMP file" );
   }

   // Make sure this is a 24bpp file
   if( *( int* ) &( header[0x1E] ) != 0 )
   {
      throw std::runtime_error( "Not a correct BMP file\n" );
   }

   if( *( int* ) &( header[0x1C] ) != 24 )
   {
      throw std::runtime_error( "Not a correct BMP file\n" );
   }

   // Read the information about the image
   dataPos = *( int* ) &( header[0x0A] );
   imageSize = 0;
   //imageSize = *(int*) &(header[0x22]);
   width = *( int* ) &( header[0x12] );
   height = *( int* ) &( header[0x16] );

   // Some BMP files are misformatted, guess missing information
   if( imageSize == 0 )    imageSize = width*height * 3; // 3 : one byte for each Red, Green and Blue component
   if( dataPos == 0 )      dataPos = 54; // The BMP header is done that way

                               // Create a buffer
   data = new unsigned char[imageSize];

   // Read the actual data from the file into the buffer
   fread( data, 1, imageSize, file );

   // Everything is in memory now, the file wan be closed
   fclose( file );



   auto texture = boost::make_shared<Texture>( TextureType::Texture2D,
                                               TextureInternalFormat::RGB8,
                                               ResourceUsage::UsageImmutable,
                                               width, height, 1, int( floor( math::Log2( max( width, height ) ) ) + 1 ) );



   auto texture_bind = make_bind_guard( *texture );


   texture->UpdateToGpu( width, height, 1
                         , TextureFormat( TextureElementType::UNSIGNED_BYTE, TexturePixelFormat::BGR )
                         , data
                         , imageSize );
   texture->GenMipmaps();


   delete[] data;

   return texture;
}


MainClass::MainClass() :
   m_exiting( false ),
   m_windowTitle( "GfxApi" ),
   m_bNoCamUpdate( false ),
   m_bHideTerrain( false ),
   m_bShowTree( false ),
   m_bHideSeams( false )
{

}

MainClass::~MainClass( void )
{

}

int MainClass::initGLFW( int screenWidth, int screenHeight )
{
   if( !glfwInit() )
      throw std::runtime_error( "Error initializing GLFW" );

   m_pWindow = glfwCreateWindow( screenWidth, screenHeight, m_windowTitle.c_str(), NULL, NULL );

   if( !m_pWindow )
   {
      glfwTerminate();
      throw std::runtime_error( "Could not create GLFW window" );
   }
   // glfwSetWindowUserPointer(window, &context);
   // thats useful for key-callbacks

    /* Make the window's context current */
   glfwMakeContextCurrent( m_pWindow );

   glfwSetInputMode( m_pWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );

   return 0;
}


int MainClass::initGLEW()
{
   glewExperimental = GL_TRUE;
   GLenum err = glewInit();
   if( GLEW_OK != err )
   {
      /* Problem: glewInit failed, something is seriously wrong. */
      std::string msg = std::string( "Error initializing glew: " );
      throw std::runtime_error( msg );
   }

   return 0;
}

void MainClass::initCamera()
{

   m_camera.pos = float3( 24434, 4600, 4279 );
   m_camera.front = float3( 0, 0, 1 );
   m_camera.up = float3( 0, 1, 0 );
   m_camera.nearPlaneDistance = 20.f;
   m_camera.farPlaneDistance = 800000.f;

   m_camera.type = PerspectiveFrustum;
   m_camera.horizontalFov = 1;

   int width, height;
   glfwGetFramebufferSize( m_pWindow, &width, &height );

   m_camera.verticalFov = m_camera.horizontalFov * ( float ) height / ( float ) width;

   m_camera.projectiveSpace = FrustumSpaceGL;
   m_camera.handedness = FrustumRightHanded;
}

void MainClass::exitApplication()
{
   glfwTerminate();
}

bool MainClass::init()
{
   initGLFW( 1920, 1080 );
   initGLEW();

   m_pInput = boost::make_shared<Input>( m_pWindow );

   initCamera();

   glEnable( GL_DEPTH_TEST );
   glDepthFunc( GL_LESS );

   m_lastTick = Clock::Tick();


   return true;
}

double horizontalAngle = 0;
double verticalAngle = 0;

void MainClass::updateCamera( float deltaTime )
{
   int width, height;
   glfwGetFramebufferSize( m_pWindow, &width, &height );

   double xpos, ypos;
   glfwGetCursorPos( m_pWindow, &xpos, &ypos );

   double halfWidth = width / 2;
   double halfHeight = height / 2;

   // Reset mouse position for next frame
   glfwSetCursorPos( m_pWindow, halfWidth, halfHeight );

   double mouseSpeed = 0.0001;
   horizontalAngle += mouseSpeed * deltaTime * double( halfWidth - xpos );
   verticalAngle += mouseSpeed * deltaTime * double( halfHeight - ypos );

   Quat rotHorizontal = Quat( float3( 0, 1, 0 ), horizontalAngle );
   Quat rotVertical = Quat( float3( -1, 0, 0 ), verticalAngle );

   m_camera.up = rotHorizontal * ( rotVertical * float3( 0, 1, 0 ) );
   m_camera.front = rotHorizontal * ( rotVertical * float3( 0, 0, 1 ) );


   m_camera.up.Normalize();
   m_camera.front.Normalize();

   
}

void MainClass::mainLoop()
{
   init();


   m_pChunkManager = boost::make_shared<ChunkManager>( m_camera );

   m_tex = textureFromBmp( "rock.bmp" );
   m_tex1 = textureFromBmp( "grass.bmp" );
   m_tex2 = textureFromBmp( "normal.bmp" );
   m_tex3 = textureFromBmp( "grass_normal.bmp" );


   //tex.loadBMP("rock.bmp", 0);


   //GfxApi::Texture tex2;
   //tex2.loadBMP("grass.bmp", 1);

   //GfxApi::Texture tex3;
   //tex3.loadBMP("normal.bmp", 2);

   //GfxApi::Texture tex4;
   //tex4.loadBMP("grass_normal.bmp", 3);

   std::vector<float3> out_vertices;
   std::vector<float2> out_uvs;
   std::vector<float3> out_normals;

   std::vector<int> m_Entries;
   std::vector<int> m_Textures;

   //Assimp::Importer importer;
   //const aiScene* pScene = importer.ReadFile("castle.3ds", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_MakeLeftHanded);

   //m_Entries.resize(pScene->mNumMeshes);
   //m_Textures.resize(pScene->mNumMaterials);

   ////for (unsigned int i = 0; i < m_Entries.size(); i++) {
   //	const aiMesh* paiMesh = pScene->mMeshes[0];
   //	//InitMesh(i, paiMesh);
   ////}

   //aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

   ////loadOBJ("castle01.obj", out_vertices, out_uvs, out_normals);

   //GfxApi::VertexDeclaration decl;
   //decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position"));
   //decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::FLOAT, 3, "vertex_normal"));
   //boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer = boost::make_shared<GfxApi::VertexBuffer>(paiMesh->mNumVertices, decl);
   //boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer = boost::make_shared<GfxApi::IndexBuffer>(paiMesh->mNumFaces*3, decl, GfxApi::PrimitiveIndexType::Indices32Bit);
   //unsigned int * ibPtr = reinterpret_cast<unsigned int*>(pIndexBuffer->getCpuPtr());
   //float * vbPtr = reinterpret_cast<float*>(pVertexBuffer->getCpuPtr());
   //uint32_t offset = 0;

   //for (unsigned int i = 0; i < paiMesh->mNumVertices; i++) 
   //{
   //	
   //	const aiVector3D* pPos = &(paiMesh->mVertices[i]);

   //	const aiVector3D* pNormal = &(paiMesh->mNormals[i]);
   //
   //	const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &(paiMesh->mTextureCoords[0][i]) : &Zero3D;

   //	vbPtr[offset + 0] = pPos->x;
   //	vbPtr[offset + 1] = pPos->y;
   //	vbPtr[offset + 2] = pPos->z;
   //	vbPtr[offset + 3] = pNormal != 0 ? pNormal->x : 0;
   //	vbPtr[offset + 4] = pNormal != 0 ? pNormal->y : 0;
   //	vbPtr[offset + 5] = pNormal != 0 ? pNormal->z : 0;
   //	offset += 6;
   //
   //}

   //pVertexBuffer->allocateGpu();
   //pVertexBuffer->updateToGpu();

   //offset = 0;
   //for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
   //{
   //	const aiFace& Face = paiMesh->mFaces[i];
   //	ibPtr[offset] = Face.mIndices[0];
   //	ibPtr[offset + 1] = Face.mIndices[1];
   //	ibPtr[offset + 2] = Face.mIndices[2];
   //	offset += 3;
   //}

   //pIndexBuffer->allocateGpu();
   //pIndexBuffer->updateToGpu();

   //auto mesh = boost::make_shared<GfxApi::Mesh>(GfxApi::PrimitiveType::TriangleList);
   //mesh->m_vbs.push_back(pVertexBuffer);
   //mesh->m_ib.swap(pIndexBuffer);

   //mesh->m_sp = m_pChunkManager->m_shaders[0];

   //mesh->generateVAO();
   //mesh->linkShaders();

   //m_pMesh = mesh;

   //btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();

   while( !m_exiting )
   {

      onTick();
//      sleep( 1 );

   }

   exitApplication();
}

void MainClass::handleInput( float deltaTime )
{
   float moveSpeed = 0.1; //units per second
   if( m_pInput->keyPressed( GLFW_KEY_LEFT_SHIFT ) )
   {
      moveSpeed = 2.5;
   }
   if( m_pInput->keyPressed( GLFW_KEY_S ) )
   {
      m_camera.pos += deltaTime * moveSpeed * -m_camera.front;
   }
   else if( m_pInput->keyPressed( GLFW_KEY_W ) )
   {
      m_camera.pos += deltaTime * moveSpeed * m_camera.front;
   }

   if( m_pInput->keyPressed( GLFW_KEY_A ) )
   {
      m_camera.pos += deltaTime * moveSpeed * -m_camera.WorldRight();
   }
   else if( m_pInput->keyPressed( GLFW_KEY_D ) )
   {
      m_camera.pos += deltaTime * moveSpeed * m_camera.WorldRight();
   }

   if( m_pInput->keyPressed( GLFW_KEY_Y ) )
   {
      m_camera.pos += deltaTime * moveSpeed * -m_camera.up;
   }
   else if( m_pInput->keyPressed( GLFW_KEY_X ) )
   {
      m_camera.pos += deltaTime * moveSpeed * m_camera.up;
   }

   if( m_pInput->keyPressed( GLFW_KEY_O ) )
   {
      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
   }
   else if( m_pInput->keyPressed( GLFW_KEY_P ) )
   {
      glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
   }


   if( m_pInput->keyPressed( GLFW_KEY_F ) )
   {
      m_cameraFrozen = m_camera;
      m_bNoCamUpdate = true;
   }

   if( m_pInput->keyPressed( GLFW_KEY_G ) )
   {
      m_bNoCamUpdate = false;
   }

   if( m_pInput->keyPressed( GLFW_KEY_H ) )
   {
      m_bHideTerrain = true;
   }

   if( m_pInput->keyPressed( GLFW_KEY_J ) )
   {
      m_bHideTerrain = false;
   }

   if( m_pInput->keyPressed( GLFW_KEY_K ) )
   {
      printf( "%f %f %f \n", m_camera.pos.x, m_camera.pos.y, m_camera.pos.z );
      m_bShowTree = true;
   }

   if( m_pInput->keyPressed( GLFW_KEY_L ) )
   {
      m_bShowTree = false;
   }

   if( m_pInput->keyPressed( GLFW_KEY_B ) )
   {
      m_bHideSeams ^= true;
   }

   if( m_pInput->keyPressed( GLFW_KEY_N ) )
   {
      m_bHideTerrain ^= true;
   }

}

void MainClass::onTick()
{

   double deltaTime = Clock::MillisecondsSinceD( m_lastTick );
   m_lastTick = Clock::Tick();

   if( m_bNoCamUpdate )
   {
      m_pChunkManager->updateLoDTree2( m_cameraFrozen );
   }
   else
   {
      m_pChunkManager->updateLoDTree2( m_camera );
   }


   if( glfwWindowShouldClose( m_pWindow ) )
   {
      throw std::runtime_error( "window was closed" );
   }

   if( glfwGetWindowAttrib( m_pWindow, GLFW_FOCUSED ) )
   {
      // handle all input
      handleInput( deltaTime );

      // perform camera updates
      updateCamera( deltaTime );
   }


   // Make the window's context current 
   glfwMakeContextCurrent( m_pWindow );

   if( !m_bHideTerrain )
   {
      m_graphics.Clear( true, true, true, 0.5, 0.6, 0.8 );
   }
   else
   {
      m_graphics.Clear( true, true, true, 0, 0, 0 );
   }

   //printf("%f, %f, %f \n", m_camera.pos.x, m_camera.pos.y, m_camera.pos.z);

   for( auto& visible : m_pChunkManager->m_visibles )
   {
      if( !visible )
         continue;

      if( visible->m_pMesh )
      {
         if( !m_camera.Intersects( visible->m_chunk_bounds ) )
         {
            continue;
         }

            m_pChunkManager->m_shaders[0]->Use();
            m_pChunkManager->m_shaders[0]->SetUniform( m_camera.ViewProjMatrix(), "worldViewProj" );
            //m_pChunkManager->m_shaders[0]->SetUniform(0, "tex1");
            //m_pChunkManager->m_shaders[0]->SetUniform(1, "tex2");
            //m_pChunkManager->m_shaders[0]->SetUniform(2, "tex3");
            //m_pChunkManager->m_shaders[0]->SetUniform(3, "tex4");
            m_pChunkManager->m_shaders[0]->SetUniform( m_camera.pos.x, m_camera.pos.y, m_camera.pos.z, "cameraPos" );

         if( visible->m_pMesh->textures.size() < 1 )
         {
            auto texture_unit = boost::make_shared<GfxApi::TextureUnit>( 0 );
            auto sampler = boost::make_shared<GfxApi::TextureSampler>();
            sampler->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler->GenerateParams();
            std::string sampler_name = "tex1";

            auto mesh_texture = boost::make_shared<GfxApi::MeshTexture>( m_tex, texture_unit, sampler, sampler_name );
            visible->m_pMesh->textures.push_back( mesh_texture );

            auto texture_unit1 = boost::make_shared<GfxApi::TextureUnit>( 1 );
            auto sampler1 = boost::make_shared<GfxApi::TextureSampler>();
            sampler1->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler1->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler1->GenerateParams();
            std::string sampler_name1 = "tex2";

            auto mesh_texture1 = boost::make_shared<GfxApi::MeshTexture>( m_tex1, texture_unit1, sampler1, sampler_name1 );
            visible->m_pMesh->textures.push_back( mesh_texture1 );

            auto texture_unit2 = boost::make_shared<GfxApi::TextureUnit>( 2 );
            auto sampler2 = boost::make_shared<GfxApi::TextureSampler>();
            sampler2->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler2->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler2->GenerateParams();
            std::string sampler_name2 = "tex3";

            auto mesh_texture2 = boost::make_shared<GfxApi::MeshTexture>( m_tex2, texture_unit2, sampler2, sampler_name2 );
            visible->m_pMesh->textures.push_back( mesh_texture2 );

            auto texture_unit3 = boost::make_shared<GfxApi::TextureUnit>( 3 );
            auto sampler3 = boost::make_shared<GfxApi::TextureSampler>();
            sampler3->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler3->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler3->GenerateParams();
            std::string sampler_name3 = "tex4";

            auto mesh_texture3 = boost::make_shared<GfxApi::MeshTexture>( m_tex3, texture_unit3, sampler3, sampler_name3 );
            visible->m_pMesh->textures.push_back( mesh_texture3 );
         }


         double scale = ( 1.0f / ChunkManager::CHUNK_SIZE ) * ( visible->m_chunk_bounds.MaxX() - visible->m_chunk_bounds.MinX() );


         auto xForm = float4x4::FromTRS( float3( visible->m_chunk_bounds.MinX(), visible->m_chunk_bounds.MinY(), visible->m_chunk_bounds.MinZ() ),
                                         float4x4( Quat::identity, float3( 0, 0, 0 ) ),
                                         float3( scale, scale, scale ) );

         visible->m_pMesh->Bind();

         visible->m_pMesh->sp->SetUniform( xForm, "world" );
         visible->m_pMesh->sp->SetUniform( visible->m_scale, "res" );

         for( auto mesh_texture : visible->m_pMesh->textures )
         {
            auto& texture_unit = mesh_texture->texture_unit;
            texture_unit->Activate();
            mesh_texture->texture->Bind();


            if( !mesh_texture->sampler )
               texture_unit->UnBindSampler();
            else
               texture_unit->BindSampler( *mesh_texture->sampler );

            //mesh_texture->texture->GenMipmaps();

            visible->m_pMesh->sp->BindTexture( texture_unit->Index(), *texture_unit, *mesh_texture->texture, mesh_texture->sampler_name );
         }

         visible->m_pMesh->Draw();

         visible->m_pMesh->UnBind();
      }

   }


   for( auto& visible : m_pChunkManager->m_visiblesTemp )
   {
      if( !visible )
         continue;

      if( visible->m_pMesh )
      {
         if( !m_camera.Intersects( visible->m_chunk_bounds ) )
         {
            continue;
         }
         m_pChunkManager->m_shaders[0]->Use();
         m_pChunkManager->m_shaders[0]->SetUniform( m_camera.ViewProjMatrix(), "worldViewProj" );
         //m_pChunkManager->m_shaders[0]->SetUniform(0, "tex1");
         //m_pChunkManager->m_shaders[0]->SetUniform(1, "tex2");
         //m_pChunkManager->m_shaders[0]->SetUniform(2, "tex3");
         //m_pChunkManager->m_shaders[0]->SetUniform(3, "tex4");
         m_pChunkManager->m_shaders[0]->SetUniform( m_camera.pos.x, m_camera.pos.y, m_camera.pos.z, "cameraPos" );

         if( visible->m_pMesh->textures.size() < 1 )
         {
            auto texture_unit = boost::make_shared<GfxApi::TextureUnit>( 0 );
            auto sampler = boost::make_shared<GfxApi::TextureSampler>();
            sampler->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler->GenerateParams();
            std::string sampler_name = "tex1";

            auto mesh_texture = boost::make_shared<GfxApi::MeshTexture>( m_tex, texture_unit, sampler, sampler_name );
            visible->m_pMesh->textures.push_back( mesh_texture );

            auto texture_unit1 = boost::make_shared<GfxApi::TextureUnit>( 1 );
            auto sampler1 = boost::make_shared<GfxApi::TextureSampler>();
            sampler1->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler1->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler1->GenerateParams();
            std::string sampler_name1 = "tex2";

            auto mesh_texture1 = boost::make_shared<GfxApi::MeshTexture>( m_tex1, texture_unit1, sampler1, sampler_name1 );
            visible->m_pMesh->textures.push_back( mesh_texture1 );

            auto texture_unit2 = boost::make_shared<GfxApi::TextureUnit>( 2 );
            auto sampler2 = boost::make_shared<GfxApi::TextureSampler>();
            sampler2->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler2->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler2->GenerateParams();
            std::string sampler_name2 = "tex3";

            auto mesh_texture2 = boost::make_shared<GfxApi::MeshTexture>( m_tex2, texture_unit2, sampler2, sampler_name2 );
            visible->m_pMesh->textures.push_back( mesh_texture2 );

            auto texture_unit3 = boost::make_shared<GfxApi::TextureUnit>( 3 );
            auto sampler3 = boost::make_shared<GfxApi::TextureSampler>();
            sampler3->magFilter = GfxApi::TextureFilterMode::Linear;
            sampler3->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
            sampler3->GenerateParams();
            std::string sampler_name3 = "tex4";

            auto mesh_texture3 = boost::make_shared<GfxApi::MeshTexture>( m_tex3, texture_unit3, sampler3, sampler_name3 );
            visible->m_pMesh->textures.push_back( mesh_texture3 );
         }


         double scale = ( 1.0f / ChunkManager::CHUNK_SIZE ) * ( visible->m_chunk_bounds.MaxX() - visible->m_chunk_bounds.MinX() );


         auto xForm = float4x4::FromTRS( float3( visible->m_chunk_bounds.MinX(), visible->m_chunk_bounds.MinY(), visible->m_chunk_bounds.MinZ() ),
                                         float4x4( Quat::identity, float3( 0, 0, 0 ) ),
                                         float3( scale, scale, scale ) );

         visible->m_pMesh->Bind();

         visible->m_pMesh->sp->SetUniform( xForm, "world" );
         visible->m_pMesh->sp->SetUniform( visible->m_scale, "res" );

         for( auto mesh_texture : visible->m_pMesh->textures )
         {
            auto& texture_unit = mesh_texture->texture_unit;
            texture_unit->Activate();
            mesh_texture->texture->Bind();


            if( !mesh_texture->sampler )
               texture_unit->UnBindSampler();
            else
               texture_unit->BindSampler( *mesh_texture->sampler );

            //mesh_texture->texture->GenMipmaps();

            visible->m_pMesh->sp->BindTexture( texture_unit->Index(), *texture_unit, *mesh_texture->texture, mesh_texture->sampler_name );
         }

         visible->m_pMesh->Draw();

         visible->m_pMesh->UnBind();
      }

   }

   //if( m_pChunkManager->m_scene_current )
   //{
   //   m_pChunkManager->m_shaders[0]->Use();
   //   m_pChunkManager->m_shaders[0]->SetUniform( m_camera.ViewProjMatrix(), "worldViewProj" );
   //   //m_pChunkManager->m_shaders[0]->SetUniform(0, "tex1");
   //   //m_pChunkManager->m_shaders[0]->SetUniform(1, "tex2");
   //   //m_pChunkManager->m_shaders[0]->SetUniform(2, "tex3");
   //   //m_pChunkManager->m_shaders[0]->SetUniform(3, "tex4");
   //   m_pChunkManager->m_shaders[0]->SetUniform( m_camera.pos.x, m_camera.pos.y, m_camera.pos.z, "cameraPos" );

   //   //for (auto& chunk_list : *m_pChunkManager->m_scene_current)
   //   for( std::size_t i = ChunkManager::MAX_LOD_LEVEL + 3; i > 0; i-- )
   //   {
   //      auto& chunk_list = ( *m_pChunkManager->m_scene_current )[i];
   //      for( auto& chunk : chunk_list )
   //      {
   //         if( chunk->m_pMesh && !m_bHideTerrain )
   //         {
   //            //if( m_camera.Intersects( chunk->m_chunk_bounds ) )
   //            {
   //              // if( m_bNoCamUpdate )
   //               {
   //                //  if( !m_cameraFrozen.Intersects( chunk->m_chunk_bounds ) )
   //                  {
   //                     // continue;
   //                  }
   //               }

   //               if( chunk->m_pMesh->textures.size() < 1 )
   //               {
   //                  auto texture_unit = boost::make_shared<GfxApi::TextureUnit>( 0 );
   //                  auto sampler = boost::make_shared<GfxApi::TextureSampler>();
   //                  sampler->magFilter = GfxApi::TextureFilterMode::Linear;
   //                  sampler->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
   //                  sampler->GenerateParams();
   //                  std::string sampler_name = "tex1";

   //                  auto mesh_texture = boost::make_shared<GfxApi::MeshTexture>( m_tex, texture_unit, sampler, sampler_name );
   //                  chunk->m_pMesh->textures.push_back( mesh_texture );

   //                  auto texture_unit1 = boost::make_shared<GfxApi::TextureUnit>( 1 );
   //                  auto sampler1 = boost::make_shared<GfxApi::TextureSampler>();
   //                  sampler1->magFilter = GfxApi::TextureFilterMode::Linear;
   //                  sampler1->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
   //                  sampler1->GenerateParams();
   //                  std::string sampler_name1 = "tex2";

   //                  auto mesh_texture1 = boost::make_shared<GfxApi::MeshTexture>( m_tex1, texture_unit1, sampler1, sampler_name1 );
   //                  chunk->m_pMesh->textures.push_back( mesh_texture1 );

   //                  auto texture_unit2 = boost::make_shared<GfxApi::TextureUnit>( 2 );
   //                  auto sampler2 = boost::make_shared<GfxApi::TextureSampler>();
   //                  sampler2->magFilter = GfxApi::TextureFilterMode::Linear;
   //                  sampler2->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
   //                  sampler2->GenerateParams();
   //                  std::string sampler_name2 = "tex3";

   //                  auto mesh_texture2 = boost::make_shared<GfxApi::MeshTexture>( m_tex2, texture_unit2, sampler2, sampler_name2 );
   //                  chunk->m_pMesh->textures.push_back( mesh_texture2 );

   //                  auto texture_unit3 = boost::make_shared<GfxApi::TextureUnit>( 3 );
   //                  auto sampler3 = boost::make_shared<GfxApi::TextureSampler>();
   //                  sampler3->magFilter = GfxApi::TextureFilterMode::Linear;
   //                  sampler3->minFilter = GfxApi::TextureFilterMode::LinearMipmapLinear;
   //                  sampler3->GenerateParams();
   //                  std::string sampler_name3 = "tex4";

   //                  auto mesh_texture3 = boost::make_shared<GfxApi::MeshTexture>( m_tex3, texture_unit3, sampler3, sampler_name3 );
   //                  chunk->m_pMesh->textures.push_back( mesh_texture3 );
   //               }


   //               double scale = ( 1.0f / ChunkManager::CHUNK_SIZE ) * ( chunk->m_chunk_bounds.MaxX() - chunk->m_chunk_bounds.MinX() );


   //               auto xForm = float4x4::FromTRS( float3( chunk->m_chunk_bounds.MinX(), chunk->m_chunk_bounds.MinY(), chunk->m_chunk_bounds.MinZ() ),
   //                                               float4x4( Quat::identity, float3( 0, 0, 0 ) ),
   //                                               float3( scale, scale, scale ) );

   //               chunk->m_pMesh->Bind();

   //               chunk->m_pMesh->sp->SetUniform( xForm, "world" );
   //               chunk->m_pMesh->sp->SetUniform( chunk->m_scale, "res" );

   //               for( auto mesh_texture : chunk->m_pMesh->textures )
   //               {
   //                  auto& texture_unit = mesh_texture->texture_unit;
   //                  texture_unit->Activate();
   //                  mesh_texture->texture->Bind();


   //                  if( !mesh_texture->sampler )
   //                     texture_unit->UnBindSampler();
   //                  else
   //                     texture_unit->BindSampler( *mesh_texture->sampler );

   //                  //mesh_texture->texture->GenMipmaps();

   //                  chunk->m_pMesh->sp->BindTexture( texture_unit->Index(), *texture_unit, *mesh_texture->texture, mesh_texture->sampler_name );
   //               }

   //               chunk->m_pMesh->Draw();

   //               chunk->m_pMesh->UnBind();
   //            }
   //         }

   //      }

   //   }

   //}

   // Swap front and back buffers 
   glfwSwapBuffers( m_pWindow );


   // poll the input system
   m_pInput->poll();



}

Frustum& MainClass::getCamera()
{
   return m_camera;
}

Frustum* MainClass::getCameraPtr()
{
   return &m_camera;
}