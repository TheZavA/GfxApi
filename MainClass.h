#ifndef _MAINCLASS_H
#define _MAINCLASS_H



#include <string>
#include "mgl/MathGeoLib.h"
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include "GfxApi.h"


struct GLFWwindow;

class ChunkManager;
class Input;

namespace GfxApi
{
    struct RenderNode;

}



class MainClass
{
public:
    MainClass();
    ~MainClass(void);
    int initGLFW(int screenWidth, int screenHeight);
    int initGLEW();

    virtual bool init();

    virtual void mainLoop();
    
    virtual void onTick();

    void exitApplication();

    void initCamera();

    void updateCamera(float deltaTime);

    void handleInput(float deltaTime);

    Frustum& getCamera();
    Frustum* getCameraPtr();

private:

    GLFWwindow* m_pWindow;

    GfxApi::Graphics m_graphics;

    bool m_exiting;

    bool isExiting();

    std::string m_windowTitle;

    Frustum m_camera;

    Frustum m_cameraFrozen;

    bool m_bNoCamUpdate;


    bool m_bShowTree;

    bool m_bHideSeams;

    bool m_bHideTerrain;

    boost::shared_ptr<Input> m_pInput;

    tick_t m_lastTick;

    std::vector<boost::shared_ptr<GfxApi::RenderNode>> m_meshList;

    boost::shared_ptr<ChunkManager> m_pChunkManager;

	boost::shared_ptr<GfxApi::IndexBuffer> m_pIndices;
	boost::shared_ptr<GfxApi::VertexBuffer> m_pVertices;
	boost::shared_ptr<GfxApi::Mesh> m_pMesh;

	//boost::shared_ptr<GfxApi::Texture> m_tex;
	//boost::shared_ptr<GfxApi::Texture> m_tex1;
	//boost::shared_ptr<GfxApi::Texture> m_tex2;
	//boost::shared_ptr<GfxApi::Texture> m_tex3;

};

#endif

