#include "ChunkManager.h"

#include "Chunk.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

//#include "../cubelib/cube.hpp"

#include <minmax.h>

#include "../GfxApi.h"

#include <set>

#include "../ocl.h"


#include "Clipmap.h"

ChunkManager::ChunkManager(Frustum& camera)
{
   
    m_ocl = boost::make_shared<ocl_t>(0,0);
    std::vector<std::string> sources;
    
    
    sources.push_back("scan_kernel.cl");
    sources.push_back("noisetest.cl");
    sources.push_back("chunk_kernel.cl");
    
    m_ocl->load_cl_source(sources);

    boost::shared_ptr<GfxApi::Shader> vs = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::VertexShader);
    vs->LoadFromFile(GfxApi::ShaderType::VertexShader, "shader/chunk.vs");

    boost::shared_ptr<GfxApi::Shader> ps = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::PixelShader);
    ps->LoadFromFile(GfxApi::ShaderType::PixelShader, "shader/chunk.ps");

    auto sp = boost::make_shared<GfxApi::ShaderProgram>();
    sp->attach(*vs);
    sp->attach(*ps);

    boost::shared_ptr<GfxApi::Shader> vs1 = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::VertexShader);
    vs1->LoadFromFile(GfxApi::ShaderType::VertexShader, "shader/line.vs");

    boost::shared_ptr<GfxApi::Shader> ps1 = boost::make_shared<GfxApi::Shader>(GfxApi::ShaderType::PixelShader);
    ps1->LoadFromFile(GfxApi::ShaderType::PixelShader, "shader/line.ps");

    auto sp1 = boost::make_shared<GfxApi::ShaderProgram>();
    sp1->attach(*vs1);
    sp1->attach(*ps1);

    m_shaders.push_back(sp);
    m_shaders.push_back(sp1);

    std::thread chunkLoadThread(&ChunkManager::chunkLoaderThread, this);
	chunkLoadThread.detach();

    m_pClipmap = boost::make_shared<Clipmap>(camera.pos, MAX_LOD_LEVEL, LOD_MIN_RES, 3, this);

}


void ChunkManager::resetClipmap(Frustum& camera)
{
    m_pClipmap.reset();
    
    m_pClipmap = boost::make_shared<Clipmap>(camera.pos, MAX_LOD_LEVEL, LOD_MIN_RES, 3, this);
}


void ChunkManager::chunkLoaderThread() 
{
    //this has to be in ms or something, not framerate, but time!
    //int frameRate = 60;
    typedef boost::chrono::high_resolution_clock Clock;
    //60 fps is ~15 ms / frame
    std::chrono::milliseconds frametime( 15 );
    bool did_some_work = false;
    while(1) 
    {
    
        did_some_work = false;

        std::size_t max_chunks_per_frame = 5;

        for (std::size_t i = 0; i < max_chunks_per_frame; ++i)
        {

            auto& chunk = m_chunkGeneratorQueue.pop();

            if(!chunk)
            {
                continue;
            }
           
            auto t1 = Clock::now();
         
            chunk->generateDensities();
 

            chunk->classifyEdges();
        
                
            if(chunk->m_bHasNodes && chunk->m_edgesCompact && chunk->m_edgesCompact->size() > 0)
            {
                chunk->generateZeroCross();
                chunk->generateFullEdges();
            }

            if(!chunk->m_bHasNodes)
            {
                chunk->m_edgesCompact.reset();
                chunk->m_workInProgress.reset();
                did_some_work = true;
                continue;
            }


            assert(chunk->m_zeroCrossCompact != 0);
            chunk->buildTree(ChunkManager::CHUNK_SIZE, 1.0f);
            
            if(chunk->m_pOctreeNodes)
            {
                (*(chunk->m_pOctreeNodes)).clear();
            }

            chunk->m_edgesCompact.reset();
            chunk->m_cornersCompact.reset();
            chunk->m_zeroCrossCompact.reset();

            chunk->m_workInProgress.reset();
            did_some_work = true;
            auto t2 = Clock::now();

           // std::cout << t2 - t1 << '\n';

        }


        std::size_t max_rings_per_frame = 1;

        for (std::size_t i = 0; i < max_rings_per_frame; ++i)
        {

            auto& p_ring = m_ring_generator_queue.pop();

            if(!p_ring)
            {
                continue;
            }

            if(!p_ring->allChunksGenrated())
            {
                //std::cout << "skipping a chunk" << std::endl;
                m_ring_generator_queue.push(p_ring);
                continue;
            }

            int index_count = 0;
            for(int y = 0; y < p_ring->m_n; y++)
            {
                for(int z = 0; z < p_ring->m_n; z++)
                {
                    for(int x = 0; x < p_ring->m_n; x++)
                    {
               
                         auto& chunk = (*p_ring->m_pChunks)(x, y, z);
                         for(auto entry : chunk->m_seam_nodes)
                         {
                            float3 pos = chunk->m_basePos + float3(chunk->m_x, chunk->m_y, chunk->m_z) * chunk->m_scale * (ChunkManager::CHUNK_SIZE) ;
                            float3 checkPos = pos + float3(1,1,1) * (chunk->m_scale * (ChunkManager::CHUNK_SIZE) / 2);
                            
                            if(p_ring->m_ring_bounds_inner.Contains(checkPos))
                            {
                                continue;
                            }

                            entry.minx = entry.minx + (chunk->m_x * ChunkManager::CHUNK_SIZE);
                            entry.miny = entry.miny + (chunk->m_y * ChunkManager::CHUNK_SIZE);
                            entry.minz = entry.minz + (chunk->m_z * ChunkManager::CHUNK_SIZE);

                            entry.drawInfo.position.x += (float)chunk->m_x * (float)ChunkManager::CHUNK_SIZE;
                            entry.drawInfo.position.y += (float)chunk->m_y * (float)ChunkManager::CHUNK_SIZE;
                            entry.drawInfo.position.z += (float)chunk->m_z * (float)ChunkManager::CHUNK_SIZE;

                            p_ring->m_seam_nodes.push_back(entry);
                            p_ring->m_seam_indices.push_back(index_count++);
                            
                         }
                         
                    }
                    
                }
            }
            p_ring->m_nodeIdxCurr = index_count;
            p_ring->m_seam_tree_root_index = p_ring->createOctree(p_ring->m_seam_indices, 1.0f, false);

            if(p_ring->m_seam_tree_root_index != -1 && p_ring->m_seam_nodes.size() > 1)
            {
                boost::shared_ptr<IndexBuffer> pIndices = boost::make_shared<IndexBuffer>();
                boost::shared_ptr<VertexBuffer> pVertices = boost::make_shared<VertexBuffer>();

                GenerateMeshFromOctree(p_ring->m_seam_tree_root_index, *pVertices, *pIndices, p_ring->m_seam_nodes);

                if(pVertices->size() <= 1 || pIndices->size() <= 1)
                {
                    p_ring->m_seam_nodes.clear();
                    p_ring->m_b_work_in_progress.reset();
                    p_ring->m_seams_generated = boost::make_shared<bool>(true);
                    continue;
                }
                p_ring->m_pVertices = pVertices;
                p_ring->m_pIndices = pIndices;
            }

			p_ring->m_seam_nodes.clear();
			p_ring->m_seam_indices.clear();
            p_ring->m_b_work_in_progress.reset();
        }

        if (!did_some_work)
        {
            //we can afford to poll 10 X a frame or even more, thread will remain 100% idle, because of sleep.
            std::this_thread::sleep_for( frametime / 10);
        }


    }

}


ChunkManager::~ChunkManager(void)
{
}


void ChunkManager::render(void)
{

}