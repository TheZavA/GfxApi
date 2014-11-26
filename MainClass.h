#ifndef _MAINCLASS_H
#define _MAINCLASS_H

#include "GfxApi.h"

#include <string>
#include "mgl/MathGeoLib.h"
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>



void initText2D(const char * texturePath);
void printText2D(const char * text, int x, int y, int size);
void cleanupText2D();


struct GLFWwindow;

class ChunkManager;

namespace GfxApi
{
    class RenderNode;
    class Input;
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

private:

    GLFWwindow* m_pWindow;

    GfxApi::Graphics m_graphics;

    bool m_exiting;

    bool isExiting();

    std::string m_windowTitle;

    Frustum m_camera;

    Frustum m_cameraFrozen;

    bool m_bNoCamUpdate;

    bool m_bHideTerrain;

    bool m_bShowTree;

    boost::shared_ptr<GfxApi::Input> m_pInput;

    tick_t m_lastTick;

    std::vector<boost::shared_ptr<GfxApi::RenderNode>> m_meshList;

    boost::shared_ptr<ChunkManager> m_pChunkMgr;
};

#endif

