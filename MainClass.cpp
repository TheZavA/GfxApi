#include "MainClass.h"

#include "GfxApi.h"

#include <GLFW/glfw3.h>

#include <boost/make_shared.hpp>

#include "ChunkManager.h"
#include "Chunk.h"


MainClass::MainClass() :
    m_exiting(false),
    m_windowTitle("GfxApi"),
    m_bNoCamUpdate(false),
    m_bHideTerrain(false)
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
    m_camera.pos = float3(0, 0, -5);
    m_camera.front = float3(0,0,1);
    m_camera.up = float3(0,1,0);
    m_camera.nearPlaneDistance = 1.f;
    m_camera.farPlaneDistance = 10000.f;

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

   // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    m_pChunkMgr = boost::make_shared<ChunkManager>();
    
    GfxApi::VertexDeclaration decl1;
    decl1.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position"));

    GfxApi::VertexDeclaration decl2;
    decl2.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::COLOR, GfxApi::VertexDataType::FLOAT, 3, "vertex_color"));

    GfxApi::VertexDeclaration decl3;
    decl3.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::TCOORD, GfxApi::VertexDataType::FLOAT, 2, "vertex_uv"));

    auto vb1 = boost::make_shared<GfxApi::VertexBuffer>(3, decl1);
    auto vb2 = boost::make_shared<GfxApi::VertexBuffer>(3, decl2);
    auto vb3 = boost::make_shared<GfxApi::VertexBuffer>(3, decl3);

    auto ib = boost::make_shared<GfxApi::IndexBuffer>(3, decl1, GfxApi::PrimitiveIndexType::Indices32Bit);

    auto mesh = boost::make_shared<GfxApi::Mesh>(GfxApi::PrimitiveType::TriangleList);

    float * vbPtr = reinterpret_cast<float*>(vb1->getCpuPtr());
    float * vbPtr2 = reinterpret_cast<float*>(vb2->getCpuPtr());
    float * vbPtr3 = reinterpret_cast<float*>(vb3->getCpuPtr());

    vbPtr[0] = 0;
    vbPtr[1] = 1;
    vbPtr[2] = 0;

    vbPtr2[0] = 1;
    vbPtr2[1] = 0;
    vbPtr2[2] = 0;

    vbPtr3[0] = 0;
    vbPtr3[1] = 0;

    vbPtr[3] = 1;
    vbPtr[4] = -1;
    vbPtr[5] = 0;

    vbPtr2[3] = 0;
    vbPtr2[4] = 1;
    vbPtr2[5] = 0;

    vbPtr3[2] = 1;
    vbPtr3[3] = 0;

    vbPtr[6] = -1;
    vbPtr[7] = -1;
    vbPtr[8] = 0;

    vbPtr2[6] = 0;
    vbPtr2[7] = 0;
    vbPtr2[8] = 1;

    vbPtr3[4] = 1;
    vbPtr3[5] = 1;

    vb1->allocateGpu();
    vb1->updateToGpu();

    vb2->allocateGpu();
    vb2->updateToGpu();

    vb3->allocateGpu();
    vb3->updateToGpu();

    uint32_t * ibPtr = reinterpret_cast<uint32_t*>(ib->getCpuPtr());

    ibPtr[0] = 0;
    ibPtr[1] = 1;
    ibPtr[2] = 2;

    ib->allocateGpu();
    ib->updateToGpu();

    mesh->m_vbs.push_back(vb1);
    mesh->m_vbs.push_back(vb2);
    mesh->m_vbs.push_back(vb3);
    mesh->m_ib.swap(ib);

    boost::shared_ptr<GfxApi::Shader> vs = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::VertexShader);
    vs->LoadFromFile(GfxApi::ShaderType::VertexShader, "shader/vertex_shader.txt");

    boost::shared_ptr<GfxApi::Shader> ps = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::PixelShader);
    ps->LoadFromFile(GfxApi::ShaderType::PixelShader, "shader/pixel_shader.txt");

    mesh->m_sp = boost::make_shared<GfxApi::ShaderProgram>();
    mesh->m_sp->attach(*vs);
    mesh->m_sp->attach(*ps);
    mesh->generateVAO();
    mesh->linkShaders();

    auto node = boost::make_shared<GfxApi::RenderNode>(mesh, float3(0, 0, 0), float3(1,1,1), float3(0,0,0));
    auto node1 = boost::make_shared<GfxApi::RenderNode>(mesh, float3(-2, 0, 0), float3(1,2,1), float3(0,0,0));
    auto node2 = boost::make_shared<GfxApi::RenderNode>(mesh, float3(-1, 0, 0), float3(1,1,1), float3(0,0,0));

    m_meshList.push_back(node);
    m_meshList.push_back(node1);
    m_meshList.push_back(node2);
     
    while(!m_exiting)
    {
        
        onTick();
        Sleep(1);
        
    }

    exitApplication();
}

void MainClass::handleInput(float deltaTime)
{
    float moveSpeed = 0.01; //units per second
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


}
    
void MainClass::onTick()
{

    double deltaTime = Clock::MillisecondsSinceD(m_lastTick);
    m_lastTick = Clock::Tick();

    if(m_bNoCamUpdate)
    {
        m_pChunkMgr->updateLoDTree(m_cameraFrozen);
    }
    else
    {
        m_pChunkMgr->updateLoDTree(m_camera);
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


    //// render all meshes
    //for(auto& node : m_meshList)
    //{
    // 
    //    node->m_pMesh->applyVAO();
    //    if(m_pChunkMgr->m_pLastShader != node->m_pMesh->m_sp)
    //    {
    //        node->m_pMesh->m_sp->use();
    //        m_pChunkMgr->m_pLastShader = node->m_pMesh->m_sp;
    //    }

    //    int worldLocation = node->m_pMesh->m_sp->getUniformLocation("world");
    //    assert(worldLocation != -1);
    //    int worldViewProjLocation = node->m_pMesh->m_sp->getUniformLocation("worldViewProj");
    //    assert(worldViewProjLocation != -1);
    //        
    //    float4x4 world = node->m_xForm;
    //    node->m_pMesh->m_sp->setFloat4x4(worldLocation, world);
    //    node->m_pMesh->m_sp->setFloat4x4(worldViewProjLocation, m_camera.ViewProjMatrix() );
    //        
    //    node->m_pMesh->draw();

    //}

    m_pChunkMgr->renderBounds(m_camera);
 

    //for(auto& chunk : m_pChunkMgr->m_visibles)
    //{
    //    if(!chunk->m_pMesh)
    //    {
    //        chunk->generateMesh();
    //    }
    //}

    for(auto& chunk : m_pChunkMgr->m_visibles)
    {

        if(chunk->m_pMesh && !m_bHideTerrain)
        {
            if(m_camera.Intersects(chunk->m_bounds))
            {
                double scale = (1.0f/ChunkManager::CHUNK_SIZE) * (chunk->m_bounds.MaxX() - chunk->m_bounds.MinX());
               
                auto node = boost::make_shared<GfxApi::RenderNode>(chunk->m_pMesh, float3(chunk->m_bounds.MinX() , 
                                                                                          chunk->m_bounds.MinY() ,
                                                                                          chunk->m_bounds.MinZ() ),
                                                                                          float3(scale, scale, scale), 
                                                                                          float3(0, 0, 0));
                node->m_pMesh->applyVAO();

                if(m_pChunkMgr->m_pLastShader != node->m_pMesh->m_sp)
                {
                    node->m_pMesh->m_sp->use();
                    m_pChunkMgr->m_pLastShader = node->m_pMesh->m_sp;
                }

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
