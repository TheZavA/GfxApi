
#include "MainClass.h"

#include "GfxApi.h"

#include "voxel/octree.h"

#include <GLFW/glfw3.h>

#include <boost/make_shared.hpp>

#include "voxel/Chunk.h"

#include "voxel/ChunkManager.h"

MainClass::MainClass() :
    m_exiting(false),
    m_windowTitle("GfxApi"),
    m_bNoCamUpdate(false),
    m_bHideTerrain(false),
    m_bShowTree(false)
{

}


MainClass::~MainClass(void)
{

}

int MainClass::initGLFW(int screenWidth, int screenHeight)
{
    if (!glfwInit())
        throw std::runtime_error("Error initializing GLFW");

    m_pWindow = glfwCreateWindow(screenWidth, screenHeight, m_windowTitle.c_str(), NULL, NULL);

    if (!m_pWindow)
    {
        glfwTerminate();
        throw std::runtime_error("Could not create GLFW window");
    }
   // glfwSetWindowUserPointer(window, &context);
   // thats useful for key-callbacks

    /* Make the window's context current */
    glfwMakeContextCurrent(m_pWindow);

    glfwSetInputMode(m_pWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    return 0;
}


int MainClass::initGLEW()
{
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        std::string msg = std::string("Error initializing glew: ");
        throw std::runtime_error(msg);
    }

    return 0;
}

void MainClass::initCamera()
{
    m_camera.pos = float3(0, 4000, -5);
    m_camera.front = float3(0,0,1);
    m_camera.up = float3(0,1,0);
    m_camera.nearPlaneDistance = 1.f;
    m_camera.farPlaneDistance = 40000.f;

    m_camera.type = PerspectiveFrustum;
    m_camera.horizontalFov = 1;
    
    int width, height;
    glfwGetFramebufferSize(m_pWindow, &width, &height);

    m_camera.verticalFov = m_camera.horizontalFov * (float)height / (float)width;
    
    m_camera.projectiveSpace = FrustumSpaceGL;
    m_camera.handedness = FrustumRightHanded;
}

void MainClass::exitApplication()
{
    glfwTerminate();
}

bool MainClass::init()
{
    initGLFW(1280, 768);
    initGLEW();

    m_pInput = boost::make_shared<GfxApi::Input>(m_pWindow);

    initCamera();

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS); 

    m_lastTick = Clock::Tick();

    return true;
}

double horizontalAngle = 0;
double verticalAngle = 0;

void MainClass::updateCamera(float deltaTime)
{
    int width, height;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    
    double xpos, ypos;
    glfwGetCursorPos(m_pWindow, &xpos, &ypos);

    double halfWidth = width/2;
    double halfHeight = height/2;
    
    // Reset mouse position for next frame
    glfwSetCursorPos(m_pWindow, halfWidth, halfHeight);
    
    double mouseSpeed = 0.0001;
    horizontalAngle += mouseSpeed * deltaTime * double(halfWidth - xpos );
    verticalAngle += mouseSpeed * deltaTime * double(halfHeight - ypos );
  
    Quat rotHorizontal = Quat(float3(0,1,0), horizontalAngle); 
    Quat rotVertical = Quat(float3(-1,0,0), verticalAngle);

    m_camera.up = rotHorizontal * (rotVertical * float3(0,1,0));
    m_camera.front = rotHorizontal * (rotVertical * float3(0,0,1));


    m_camera.up.Normalize();
    m_camera.front.Normalize();

   
}

void MainClass::mainLoop()
{
    init();


    m_pChunkManager = boost::make_shared<ChunkManager>();

    GfxApi::Texture tex;

    tex.loadBMP("rock.bmp");

    while(!m_exiting)
    {
        
        onTick();
        Sleep(1);
        
    }

    exitApplication();
}

void MainClass::handleInput(float deltaTime)
{
    float moveSpeed = 0.005; //units per second
    if(m_pInput->keyPressed(GLFW_KEY_LEFT_SHIFT))
    {
        moveSpeed = 0.5;
    }
    if(m_pInput->keyPressed(GLFW_KEY_S))
    {
        m_camera.pos += deltaTime * moveSpeed * -m_camera.front;
    } 
    else if(m_pInput->keyPressed(GLFW_KEY_W))
    {
        m_camera.pos += deltaTime * moveSpeed * m_camera.front;
    }

    if(m_pInput->keyPressed(GLFW_KEY_A))
    {
        m_camera.pos += deltaTime * moveSpeed * -m_camera.WorldRight();
    } 
    else if(m_pInput->keyPressed(GLFW_KEY_D))
    {
        m_camera.pos += deltaTime * moveSpeed * m_camera.WorldRight();
    }

    if(m_pInput->keyPressed(GLFW_KEY_Y))
    {
        m_camera.pos += deltaTime * moveSpeed * -m_camera.up; 
    } 
    else if(m_pInput->keyPressed(GLFW_KEY_X))
    {
        m_camera.pos += deltaTime * moveSpeed * m_camera.up;
    }

    if(m_pInput->keyPressed(GLFW_KEY_O))
    {
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    }
    else if(m_pInput->keyPressed(GLFW_KEY_P))
    {
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    }
   

    if(m_pInput->keyPressed(GLFW_KEY_F))
    {
        m_cameraFrozen = m_camera;
        m_bNoCamUpdate = true;
    }

    if(m_pInput->keyPressed(GLFW_KEY_G))
    {
        m_bNoCamUpdate = false;
    }

    if(m_pInput->keyPressed(GLFW_KEY_H))
    {
        m_bHideTerrain = true;
    }

    if(m_pInput->keyPressed(GLFW_KEY_J))
    {
        m_bHideTerrain = false;
    }

    if(m_pInput->keyPressed(GLFW_KEY_K))
    {
        m_bShowTree = true;
    }

    if(m_pInput->keyPressed(GLFW_KEY_L))
    {
        m_bShowTree = false;
    }

    
}
    
void MainClass::onTick()
{

    double deltaTime = Clock::MillisecondsSinceD(m_lastTick);
    m_lastTick = Clock::Tick();


    if(m_bNoCamUpdate)
    {
        m_pChunkManager->updateLoDTree(m_cameraFrozen);
    }
    else
    {
        m_pChunkManager->updateLoDTree(m_camera);
    }

    if (glfwWindowShouldClose(m_pWindow))
    {
        throw std::runtime_error("window was closed");
    }

    if (glfwGetWindowAttrib(m_pWindow, GLFW_FOCUSED))
    {
        // handle all input
        handleInput(deltaTime);

        // perform camera updates
        updateCamera(deltaTime);
    }

    
    // Make the window's context current 
    glfwMakeContextCurrent(m_pWindow);

    if(!m_bHideTerrain)
    {
        m_graphics.clear(true, true, true, 0, 0.5, 0.9);
    }
    else
    {
        m_graphics.clear(true, true, true, 0, 0, 0);
    }

    if(m_bShowTree)
    {
        m_pChunkManager->renderBounds(m_camera);
    }

    for(auto& node : m_meshList)
    {
        node->m_pMesh->applyVAO();
        node->m_pMesh->m_sp->use();

        int worldLocation = node->m_pMesh->m_sp->getUniformLocation("world");
        assert(worldLocation != -1);
        int worldViewProjLocation = node->m_pMesh->m_sp->getUniformLocation("worldViewProj");
        assert(worldViewProjLocation != -1);
            
        float4x4 world = node->m_xForm;
        node->m_pMesh->m_sp->setFloat4x4(worldLocation, world);
        node->m_pMesh->m_sp->setFloat4x4(worldViewProjLocation, m_camera.ViewProjMatrix() );
            
        node->m_pMesh->draw();
    }



    for(auto& chunk : m_pChunkManager->m_visibles)
    {

        if(chunk->m_pMesh && !m_bHideTerrain)
        {
            bool visibleChunk = false;

            //if(m_bNoCamUpdate)
            //{
            //    visibleChunk = m_cameraFrozen.Intersects(chunk->m_bounds);
            //}
            //else
            //{
            //   visibleChunk = m_camera.Intersects(chunk->m_bounds);
            //}

           // if(visibleChunk)
           // if(chunk->m_pTree->getLevel() > 2)
            {
                double scale = (1.0f/ChunkManager::CHUNK_SIZE) * (chunk->m_bounds.MaxX() - chunk->m_bounds.MinX());
               
                auto node = boost::make_shared<GfxApi::RenderNode>(chunk->m_pMesh, float3(chunk->m_bounds.MinX() , 
                                                                                          chunk->m_bounds.MinY() ,
                                                                                          chunk->m_bounds.MinZ() ),
                                                                                          float3(scale, scale, scale), 
                                                                                          float3(0, 0, 0));
                node->m_pMesh->applyVAO();

            /*    if(m_pChunkManager->m_pLastShader != node->m_pMesh->m_sp)
                {*/
                    node->m_pMesh->m_sp->use();
                /*    m_pChunkManager->m_pLastShader = node->m_pMesh->m_sp;
                }*/

                int worldLocation = node->m_pMesh->m_sp->getUniformLocation("world");
                assert(worldLocation != -1);
                int worldViewProjLocation = node->m_pMesh->m_sp->getUniformLocation("worldViewProj");
                assert(worldViewProjLocation != -1);
            
                float4x4 world = node->m_xForm;
                node->m_pMesh->m_sp->setFloat4x4(worldLocation, world);
                node->m_pMesh->m_sp->setFloat4x4(worldViewProjLocation, m_camera.ViewProjMatrix() );
            
                node->m_pMesh->draw();
            }
        }
        
    }

    GfxApi::Mesh::unbindVAO();


    // Swap front and back buffers 
    glfwSwapBuffers(m_pWindow);

    
    // poll the input system
    m_pInput->poll();
 

    
}
