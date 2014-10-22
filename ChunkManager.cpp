#include "ChunkManager.h"

#include "Chunk.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "cubelib/cube.hpp"

#include <minmax.h>

#include "GfxApi.h"

#include <set>




ChunkManager::ChunkManager(void)
{

    boost::shared_ptr<GfxApi::Shader> vs = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::VertexShader);
    vs->LoadFromFile(GfxApi::ShaderType::VertexShader, "shader/chunk.vs");

    boost::shared_ptr<GfxApi::Shader> ps = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::PixelShader);
    ps->LoadFromFile(GfxApi::ShaderType::PixelShader, "shader/chunk.ps");

    auto& sp = boost::make_shared<GfxApi::ShaderProgram>();
    sp->attach(*vs);
    sp->attach(*ps);

    boost::shared_ptr<GfxApi::Shader> vs1 = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::VertexShader);
    vs1->LoadFromFile(GfxApi::ShaderType::VertexShader, "shader/line.vs");

    boost::shared_ptr<GfxApi::Shader> ps1 = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::PixelShader);
    ps1->LoadFromFile(GfxApi::ShaderType::PixelShader, "shader/line.ps");

    auto& sp1 = boost::make_shared<GfxApi::ShaderProgram>();
    sp1->attach(*vs1);
    sp1->attach(*ps1);

    m_shaders.push_back(sp);
    m_shaders.push_back(sp1);

    std::thread chunkLoadThread(&ChunkManager::chunkLoaderThread, this);
	chunkLoadThread.detach();

    AABB rootBounds(vec(WORLD_BOUNDS_MIN_XZ, WORLD_BOUNDS_MIN_Y, WORLD_BOUNDS_MIN_XZ), 
                    vec(WORLD_BOUNDS_MAX_XZ, WORLD_BOUNDS_MAX_Y, WORLD_BOUNDS_MAX_XZ));

    boost::shared_ptr<Chunk> pChunk = boost::make_shared<Chunk>(rootBounds, 1, this);

    pChunk->generateBaseMap();
    pChunk->generateTerrain();
    pChunk->generateVertices();
    pChunk->generateMesh();

    m_pOctTree.reset(new ChunkTree(nullptr, nullptr, pChunk, 1, cube::corner_t::get(0, 0, 0)));

    pChunk->m_pTree = m_pOctTree.get();

    m_pOctTree->split();

    for(auto& corner : cube::corner_t::all())
    {
        initTree(m_pOctTree->getChild(corner));
        m_visibles.insert(m_pOctTree->getChild(corner).getValue());
    }

    updateVisibles(*m_pOctTree);

}

void ChunkManager::chunkLoaderThread() 
{
    //this has to be in ms or something, not framerate, but time!
    //int frameRate = 60;

    //60 fps is ~15 ms / frame
    std::chrono::milliseconds frametime( 15 );
    bool did_some_work = false;
    while(1) 
    {
    
        did_some_work = false;

        std::size_t max_chunks_per_frame = 1;

        for (std::size_t i = 0; i < max_chunks_per_frame; ++i)
        {
            boost::shared_ptr<Chunk> chunk = m_chunkNoiseGeneratorQueue.pop();
            if(!chunk)
            {
                break;
            }

            chunk->generateBaseMap();
            chunk->generateTerrain();
            chunk->generateVertices();
            

            *chunk->m_workInProgress = false;

            did_some_work = true;
        }

        if (!did_some_work)
        {
            //we can afford to poll 10 X a frame or even more, thread will remain 100% idle, because of sleep.
            std::this_thread::sleep_for( frametime / 10);
        }


    }

}



void ChunkManager::updateVisibles(ChunkTree& pTree)
{
    if(pTree.hasChildren())
    {
        for(auto& corner : cube::corner_t::all())
        {
            updateVisibles(pTree.getChild(corner));
        }
    }
    else
    {
        if(!(*pTree.getValue()->m_workInProgress))
        {
            if(!pTree.getValue()->m_pMesh)
            {

                pTree.getValue()->generateMesh();
            }
            m_visibles.insert(pTree.getValue());
        }
    }

}


void ChunkManager::initTree(ChunkTree&  pChild)
{
    boost::shared_ptr<Chunk>& pChunk = pChild.getValue();
    

    AABB bounds = pChild.getParent()->getValue()->m_bounds;
    vec center = bounds.CenterPoint();
    vec c0 = bounds.CornerPoint(pChild.getCorner().index());

    AABB b0(vec(min(c0.x, center.x),
                min(c0.y, center.y),
                min(c0.z, center.z)), 
            vec(max(c0.x, center.x),
                max(c0.y, center.y),
                max(c0.z, center.z)));

    pChunk = boost::make_shared<Chunk>(b0, 1.0f/pChild.getLevel(), this);

    pChunk->m_pTree = &pChild;

    *pChunk->m_workInProgress = true;

    m_chunkNoiseGeneratorQueue.push(pChild.getValueCopy());

}

void ChunkManager::initTree1(ChunkTree&  pChild)
{
    boost::shared_ptr<Chunk>& pChunk = pChild.getValue();

    AABB bounds = pChild.getParent()->getValue()->m_bounds;
    vec center = bounds.CenterPoint();
    vec c0 = bounds.CornerPoint(pChild.getCorner().index());

    AABB b0(vec(min(c0.x, center.x),
                min(c0.y, center.y),
                min(c0.z, center.z)), 
            vec(max(c0.x, center.x),
                max(c0.y, center.y),
                max(c0.z, center.z)));

    pChunk = boost::make_shared<Chunk>(b0, 1.0f/pChild.getLevel(), this);

    pChunk->m_pTree = &pChild;

    pChunk->generateTerrain();
    pChunk->generateVertices();
    pChunk->generateMesh();

}


bool ChunkManager::isAcceptablePixelError(float3& cameraPos, ChunkTree& tree)
{
    const AABB& box = tree.getValue()->m_bounds;

    const AABB& rootBounds = tree.getRoot()->getValue()->m_bounds;
  
    //World length of highest LOD node
    //float x_0 = 128.0;
    float x_0 = 64;

    /////All nodes within this distance will surely be rendered
    float f_0 = x_0 * 0.8;
  
    /////Total nodes
    float t = math::Pow(2 * (f_0 / x_0), 3);

    /////Lowest octree level
    float n_max = math::Log2(rootBounds.Size().Length() / x_0);
  
    /////Node distance from camera
    float d = max(f_0, box.CenterPoint().Distance(cameraPos));
  
    /////Minimum optimal node level
    float n_opt = n_max - math::Log2(d / f_0);
  
    float size_opt = rootBounds.Size().Length() / math::Pow(2, n_opt);
  
    return size_opt > box.Size().Length();
}

bool ChunkManager::allChildsGenerated(ChunkTree& pChild)
{

    for(auto& corner : cube::corner_t::all())
    {
        if(pChild.getChild(corner).getValue() && !pChild.getChild(corner).getValue()->m_blockVolumeFloat)
        {
            return false;
        }

    }
    return true;
}

// 0 -- 0
// 1 -- 2
// 2 -- 6
// 3 -- 4
// 4 -- 7
// 5 -- 3
// 6 -- 1
// 7 -- 5

int OgreToGLCornerIndex(int index)
{
    int test[] = {0,2,6,4,7,3,1,5};
        
    return test[index];
}

void ChunkManager::renderBounds(const Frustum& camera)
{

    ///Draw the octree frame
    std::vector<float> vertexData;
    for(auto& visible : m_visibles)
    {
        const AABB& box = visible->m_bounds;

        float3 colour;
        if(box.Contains(camera.pos))
        {
            colour = float3(1.0 , 0, 0);
        }
        else
        {
            colour = float3(0.2, (float)(visible->m_pTree->getLevel()) / (float)MAX_LOD_LEVEL, 0.7);
        }

        
        //ColourValue colour = ColourValue::Green;



        BOOST_FOREACH(const cube::edge_t& edge, cube::edge_t::all())
        {
            BOOST_FOREACH(const cube::corner_t& corner, edge.corners())
            {
                float3 corner_position = box.CornerPoint(corner.index());
                float3 relative_corner = corner_position - box.CenterPoint();
                relative_corner *= .999;
                float3 in_corner = box.CenterPoint() + relative_corner;

                vertexData.push_back(in_corner.x);
                vertexData.push_back(in_corner.y);
                vertexData.push_back(in_corner.z);
                vertexData.push_back(colour.x);
                vertexData.push_back(colour.y);
                vertexData.push_back(colour.z);

            }
        }


    }

        GfxApi::VertexDeclaration decl;
        decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position"));
        decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::COLOR, GfxApi::VertexDataType::FLOAT, 3, "vertex_color"));

        boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer = boost::make_shared<GfxApi::VertexBuffer>(vertexData.size()/6, decl);

        float * vbPtr = reinterpret_cast<float*>(pVertexBuffer->getCpuPtr());
        

        uint32_t offset = 0;
        for( auto& vertexInf : vertexData)
        {

            vbPtr[offset] = vertexInf;

            offset ++;
        }

        pVertexBuffer->allocateGpu();
        pVertexBuffer->updateToGpu();

        auto mesh = boost::make_shared<GfxApi::Mesh>(GfxApi::PrimitiveType::LineList);

        mesh->m_vbs.push_back(pVertexBuffer);

        mesh->m_sp = m_shaders[1];

        mesh->generateVAO();
        mesh->linkShaders();

               
        auto node = boost::make_shared<GfxApi::RenderNode>(mesh, float3(0 , 
                                                                        0 ,
                                                                        0 ),
                                                                                    float3(1, 1, 1), 
                                                                                    float3(0, 0, 0));
        node->m_pMesh->applyVAO();
        if(m_pLastShader != node->m_pMesh->m_sp)
        {
            node->m_pMesh->m_sp->use();
            m_pLastShader = node->m_pMesh->m_sp;
        }

        int worldLocation = node->m_pMesh->m_sp->getUniformLocation("world");
        assert(worldLocation != -1);
        int worldViewProjLocation = node->m_pMesh->m_sp->getUniformLocation("worldViewProj");
        assert(worldViewProjLocation != -1);
            
        float4x4 world = node->m_xForm;
        node->m_pMesh->m_sp->setFloat4x4(worldLocation, world);
        node->m_pMesh->m_sp->setFloat4x4(worldViewProjLocation, camera.ViewProjMatrix() );
            
        node->m_pMesh->draw();


        GfxApi::Mesh::unbindVAO();

}

void ChunkManager::updateLoDTree(Frustum& camera)
{

    VisibleList::iterator w = m_visibles.begin();
    

    while ( w != m_visibles.end() )
    {
        ChunkTree* visible = (*w)->m_pTree;


 
        BOOST_ASSERT(visible);
        BOOST_ASSERT(!visible->isRoot());
    
        ChunkTree* parent = visible->getParent();
        BOOST_ASSERT(parent);
    
        bool parent_acceptable_error = !parent->isRoot() && isAcceptablePixelError(camera.pos, *parent);
        bool acceptable_error = isAcceptablePixelError(camera.pos, *visible);
        
        BOOST_ASSERT(lif(parent_acceptable_error, acceptable_error));
        BOOST_ASSERT(lif(!acceptable_error, !parent_acceptable_error));
    
        if (parent_acceptable_error)
        {
            
            //Add the parent to visibles
            m_visibles.insert(parent->getValue());
            
            {
                
                std::set< boost::shared_ptr<Chunk> >::iterator e = w;
                ++w;
                m_visibles.erase(e);
                continue;
            }

        } 
        else if ( acceptable_error) 
        {
            //Let things stay the same
            if(!visible->getValue()->m_pMesh)
            {
                visible->getValue()->generateMesh();
            }
        } 
        else 
        {
            if (visible->getLevel() < MAX_LOD_LEVEL)
            {
            
                //If visible doesn't have children
                if (!visible->hasChildren())
                {
                    //Create children for visible
                    visible->split();
                    for(auto& corner : cube::corner_t::all())
                    {
                        initTree(visible->getChild(corner));
                    }
                }
           
                if(visible->hasChildren() && allChildsGenerated(*visible))
                {
                    //Foreach child of visible
                    for(auto& corner : cube::corner_t::all())
                    {
                        m_visibles.insert(visible->getChild(corner).getValue());
                    }

                    ///Remove visible from visibles
                    {
                        std::set< boost::shared_ptr<Chunk> >::iterator e = w;
                        ++w;
                        m_visibles.erase(e);
                        continue;
                    }
 
                }

                
            
            } 
            else 
            {
        
            }
        }

        ++w;
    }

    printf("%i\n", m_visibles.size());
  
}


ChunkManager::~ChunkManager(void)
{
}


void ChunkManager::render(void)
{

}