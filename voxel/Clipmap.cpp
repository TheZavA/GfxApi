#include "Clipmap.h"
#include <assert.h>
#include "ChunkManager.h"
#include "Chunk.h"
#include "../gfxapi/GfxApi.h"
#include "octree.h"

///////////////////////////////////////////////////////////////
const float QEF_ERROR = 1e-1f;
const int QEF_SWEEPS = 4;

Ring::Ring(std::size_t k, float3 basePos, float scale, ChunkManager* pChunkManager, std::size_t level) 
    : m_k(k)
    , m_n((1 << k))
    , m_m( (m_n + 1) >> 2)
    , m_centerPos(basePos)
    , m_scale(scale)
    , m_pChunks(nullptr)
    , m_pChunkManager(pChunkManager)
    , m_bGenerationInProgress(false)
    , m_pQueuedRing(nullptr)
    , m_level(level)
    , m_seams_generated(nullptr)
    , m_nodeIdxCurr(0)
    , m_nodeDrInf(0)
{
    auto chunks = boost::make_shared<TVolume3d<ChunkPtr>>(m_n, m_n, m_n);
    m_pChunks = chunks;

    m_len = m_n * m_scale * (float(ChunkManager::CHUNK_SIZE));

    m_basePos = (m_centerPos - float3(m_len/2, m_len/2, m_len/2));

    float3 bottom = (m_basePos + float3(m_len, m_len, m_len));
    m_ringBounds = AABB(m_basePos, bottom);

}

Ring::~Ring()
{

}

bool Ring::allChunksGenrated()
{
    for(std::size_t y = 0; y < m_n; y++)
    {
        for(std::size_t z = 0; z < m_n; z++)
        {
            for(std::size_t x = 0; x < m_n; x++)
            {
                auto& chunk = (*m_pChunks)(x, y, z);
                if(chunk->m_workInProgress)
                {
                    return false;
                }
                else if(!(chunk->m_workInProgress) && !chunk->m_pMesh)
                {
                    if(!chunk->m_pMesh)
                    {
                        chunk->createMesh();
                        continue;
                    }
                }
            }
        }
    }
    return true;
}

void Ring::initializeChunks()
{
    auto chunks = boost::make_shared<TVolume3d<ChunkPtr>>(m_n, m_n, m_n);

    for(std::size_t y = 0; y < m_n; y++)
    {
        for(std::size_t z = 0; z < m_n; z++)
        {
            for(std::size_t x = 0; x < m_n; x++)
            {

                auto& chunk = (*chunks)(x, y, z);
                float3 pos = m_basePos;

                auto pchunk = boost::make_shared<Chunk>(pos, x, y, z, m_scale, m_pChunkManager, m_ringBounds, shared_from_this());
                chunk = pchunk;

                chunk->m_workInProgress = boost::make_shared<bool>(true);
                m_pChunkManager->m_chunkGeneratorQueue.push(pchunk);
            }
        }
    }
    m_pChunks = chunks;

}

boost::shared_ptr<Ring> Ring::move(cube::face_t face)
{

    float3 offset = float3(face.opposite().direction().x(), 
                           face.opposite().direction().y(), 
                           face.opposite().direction().z()) * m_scale * ChunkManager::CHUNK_SIZE*2;

    auto newRing = boost::make_shared<Ring>(m_k, m_centerPos + offset, m_scale, m_pChunkManager, m_level);

    for(int y = 0; y < m_n; y++)
    {
        for(int z = 0; z < m_n; z++)
        {
            for(int x = 0; x < m_n; x++)
            {

                int x_diff = face.opposite().direction().x() * 2;
                int y_diff = face.opposite().direction().y() * 2;
                int z_diff = face.opposite().direction().z() * 2;

                int x0 = x + x_diff;
                int y0 = y + y_diff;
                int z0 = z + z_diff;

                if(x0 < 0 || x0 >= m_n || y0 < 0 || y0 >= m_n || z0 < 0 || z0 >= m_n)
                {

                    auto chunk = boost::make_shared<Chunk>(newRing->m_basePos, 
                                                           x, y, z,
                                                           m_scale,
                                                           m_pChunkManager,
                                                           newRing->m_ringBounds,
                                                           newRing);

                    (*newRing->m_pChunks)(x, y, z) = chunk;

                    chunk->m_workInProgress = boost::make_shared<bool>(true);
                    m_pChunkManager->m_chunkGeneratorQueue.push(chunk);
                }
                else
                {
                    
                    auto& ch1 = (*m_pChunks)(x0, y0, z0);
                    ch1->m_x = x;
                    ch1->m_y = y;
                    ch1->m_z = z;
                    ch1->m_basePos = newRing->m_basePos;

                    (*newRing->m_pChunks)(x, y, z) = ch1;
                    
                }

            }
        }
    }

    return newRing;
}

boost::shared_ptr<Ring> Ring::copy_base()
{

    auto newRing = boost::make_shared<Ring>(m_k, m_centerPos, m_scale, m_pChunkManager, m_level);

    for(int y = 0; y < m_n; y++)
    {
        for(int z = 0; z < m_n; z++)
        {
            for(int x = 0; x < m_n; x++)
            {

                auto ch1 = (*m_pChunks)(x, y, z);
         
                (*newRing->m_pChunks)(x, y, z) = ch1;

            }
        }
    }

    return newRing;
}

void Ring::draw()
{


}

int Ring::toint(uint8_t x, uint8_t y, uint8_t z)
{
    int index = 0;
    ((char*)(&index))[0] = x;
    ((char*)(&index))[1] = y;
    ((char*)(&index))[2] = z;
    return index;
}

uint32_t Ring::createOctree(std::vector<uint32_t>& leaf_indices, const float threshold, bool simplify = true)
{

    int index = 0;

    while(leaf_indices.size() > 1)
    {

        std::vector<uint32_t> tmp_idx_vec;
        std::unordered_map<int, uint32_t> tmp_map;
        for(auto& nodeIdx : leaf_indices)
        {
            auto node = m_seam_nodes[nodeIdx];
            OctreeNode* new_node = nullptr;

            boost::array<int,3> pos;

            int level = node.size * 2;

            pos[0] = (node.minx / level) * level;
            pos[1] = (node.miny / level) * level;
            pos[2] = (node.minz / level) * level;

           // printf("%i, %i, %i\n", new_node->minx, new_node->miny, new_node->minz);

            auto& it = tmp_map.find(toint(pos[0], pos[1], pos[2]));
            if(it == tmp_map.end())
            {
                uint32_t index = m_nodeIdxCurr;
                if(m_nodeIdxCurr >= m_seam_nodes.size())
                {
                    m_seam_nodes.push_back(OctreeNode());
                }
                new_node = &m_seam_nodes[m_nodeIdxCurr++];
                new_node->type = Node_Internal;
                new_node->size = level;
                new_node->minx = pos[0];
                new_node->miny = pos[1];
                new_node->minz = pos[2];
                
                tmp_map[toint(pos[0], pos[1], pos[2])] = index;
                
                tmp_idx_vec.push_back(index);
            }
            else
            {
                new_node = &m_seam_nodes[it->second];
            }

            boost::array<int,3> diff;
            diff[0] = pos[0] - node.minx;
            diff[1] = pos[1] - node.miny;
            diff[2] = pos[2] - node.minz;
			
            float3 corner_pos = float3(diff[0] ? 1 : 0, diff[1] ? 1 : 0, diff[2] ? 1 : 0);

			uint8_t idx = ((uint8_t)corner_pos.x << 2) | ((uint8_t)corner_pos.y << 1) | ((uint8_t)corner_pos.z);

            new_node->childrenIdx[idx] = nodeIdx;

        }
        leaf_indices.swap(tmp_idx_vec);
    }


    if(leaf_indices.size() == 0)
    {
        return -1;
    }

    //return simplifyOctree(leaf_indices[0], 1.0f, nullptr);

    return leaf_indices[0];
   
}

uint32_t Ring::simplifyOctree(uint32_t node_index, float threshold, OctreeNode* root)
{
	if (node_index == -1)
	{
		return -1;
	}

	if ((m_seam_nodes)[node_index].type != Node_Internal)
	{
		// can't simplify!
		return node_index;
	}

	QefSolver qef;
	int signs[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
	int midsign = -1;
	int edgeCount = 0;
	bool isCollapsible = true;

	for (int i = 0; i < 8; i++)
	{
		(m_seam_nodes)[node_index].childrenIdx[i] = simplifyOctree((m_seam_nodes)[node_index].childrenIdx[i], threshold, root);
		if ((m_seam_nodes)[node_index].childrenIdx[i] != -1)
		{
			OctreeNode* child = &(m_seam_nodes)[(m_seam_nodes)[node_index].childrenIdx[i]];
			if (child->type == Node_Internal)
			{
				isCollapsible = false;
			}
			else
			{
				if (child->drawInfo.position.x >= 1 && child->drawInfo.position.x < (m_n*ChunkManager::CHUNK_SIZE - 1) &&
					child->drawInfo.position.y >= 1 && child->drawInfo.position.y < (m_n*ChunkManager::CHUNK_SIZE - 1) &&
					child->drawInfo.position.z >= 1 && child->drawInfo.position.z < (m_n*ChunkManager::CHUNK_SIZE - 1))
				{

					qef.add(child->drawInfo.qef);

					midsign = (child->drawInfo.corners >> (7 - i)) & 1;
					signs[i] = (child->drawInfo.corners >> i) & 1;

					edgeCount++;
				}
				else
				{
					isCollapsible = false;
				}
			}
		}
	}

	if (!isCollapsible)
	{
		// at least one child is an internal node, can't collapse
		return node_index;
	}

	// at this point the masspoint will actually be a sum, so divide to make it the average
	Vec3 qefPosition;
	try
	{

		float ret = qef.solve(qefPosition, QEF_ERROR, QEF_SWEEPS, QEF_ERROR);
		if (ret == 0)
			return node_index;
	}
	catch (int e)
	{
		return node_index;
	}

	float error = qef.getError();

	float3 position(qefPosition.x, qefPosition.y, qefPosition.z);

	if (error > threshold)
	{
		// this collapse breaches the threshold
		return node_index;
	}

	if (position.x < (m_seam_nodes)[node_index].minx || position.x > float((m_seam_nodes)[node_index].minx) + 1.0f ||
		position.y < (m_seam_nodes)[node_index].miny || position.y > float((m_seam_nodes)[node_index].miny) + 1.0f ||
		position.z < (m_seam_nodes)[node_index].minz || position.z > float((m_seam_nodes)[node_index].minz) + 1.0f)
	{
		const auto& mp = qef.getMassPoint();
		position = float3(mp.x, mp.y, mp.z);

	}

	// change the node from an internal node to a 'psuedo leaf' node
	OctreeDrawInfo drawInfo;;

	for (int i = 0; i < 8; i++)
	{
		if (signs[i] == -1)
		{
			// Undetermined, use centre sign instead
			drawInfo.corners |= (midsign << i);
		}
		else
		{
			drawInfo.corners |= (signs[i] << i);
		}
	}

	drawInfo.averageNormal = float3(0.f);
	for (int i = 0; i < 8; i++)
	{
		if ((m_seam_nodes)[node_index].childrenIdx[i] != -1)
		{
			OctreeNode* child = &(m_seam_nodes)[(m_seam_nodes)[node_index].childrenIdx[i]];
			if (child->type == Node_Psuedo ||
				child->type == Node_Leaf)
			{
				drawInfo.averageNormal += child->drawInfo.averageNormal;
			}
		}
	}

	drawInfo.averageNormal = drawInfo.averageNormal.Normalized();
	drawInfo.position = position;
	drawInfo.qef = qef.getData();

	for (int i = 0; i < 8; i++)
	{
		//if((*m_pOctreeNodes)[node_index].childrenIdx != -1)
		//DestroyOctree(node->children[i]);
		(m_seam_nodes)[node_index].childrenIdx[i] = -1;
	}

	(m_seam_nodes)[node_index].type = Node_Psuedo;
	(m_seam_nodes)[node_index].drawInfo = drawInfo;

	return node_index;
}

void Ring::collectSeamNodes()
{

}


void Ring::createSeamMesh()
{

    if(!m_pVertices || !m_pIndices)
    {
        return;
    }

    
    GfxApi::VertexDeclaration decl;
    decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position"));
    decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::FLOAT, 3, "vertex_normal"));
    boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer = boost::make_shared<GfxApi::VertexBuffer>(m_pVertices->size(), decl);
    boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer = boost::make_shared<GfxApi::IndexBuffer>(m_pIndices->size(), decl, GfxApi::PrimitiveIndexType::Indices32Bit);
    float * vbPtr = reinterpret_cast<float*>(pVertexBuffer->getCpuPtr());
    unsigned int * ibPtr = reinterpret_cast<unsigned int*>(pIndexBuffer->getCpuPtr());
    uint32_t offset = 0;

    for( auto& vertexInf : *m_pVertices)
    {
        vbPtr[offset + 0] = vertexInf.m_xyz.x;
        vbPtr[offset + 1] = vertexInf.m_xyz.y;
        vbPtr[offset + 2] = vertexInf.m_xyz.z;
        vbPtr[offset + 3] = vertexInf.m_normal.x;
        vbPtr[offset + 4] = vertexInf.m_normal.y;
        vbPtr[offset + 5] = vertexInf.m_normal.z;
        offset += 6;
    }

    pVertexBuffer->allocateGpu();
    pVertexBuffer->updateToGpu();
    offset = 0;
    for( auto& idx : *m_pIndices)
    {
        ibPtr[offset++] = idx;
    }

    pIndexBuffer->allocateGpu();
    pIndexBuffer->updateToGpu();
    auto mesh = boost::make_shared<GfxApi::Mesh>(GfxApi::PrimitiveType::TriangleList);
    mesh->m_vbs.push_back(pVertexBuffer);
    mesh->m_ib.swap(pIndexBuffer);

    mesh->m_sp = m_pChunkManager->m_shaders[0];

    mesh->generateVAO();
    mesh->linkShaders();

    m_pMesh = mesh;

    m_pVertices.reset();
    m_pIndices.reset();
    //m_pOctreeDrawInfo.reset();


}

/////////////////////////////////////////////////////////////////


Clipmap::Clipmap(float3 center_pos, std::size_t max_level, float min_res, std::size_t k, ChunkManager* pChunkManager)
    : m_base_pos(center_pos)
    , m_max_level(max_level)
    , m_min_res(min_res)
    , m_k(k)
{

    float current_res = min_res;

    for(std::size_t i = 0; i < max_level; i++)
    {
        auto pNextRing = boost::make_shared<Ring>(k, center_pos, current_res, pChunkManager, i);
        pNextRing->initializeChunks();
        
        m_ring_list.push_back(pNextRing);
        current_res *= 2;
    }

    for(auto ring : m_ring_list)
    {

        boost::shared_ptr<Ring> pPreviousRing = nullptr;

        // for all rings in the list ...
        if(ring->m_level > 0)
        {
            pPreviousRing = m_ring_list.at(ring->m_level - 1);
        }
        else
        {
            pPreviousRing = nullptr;
        }

        // set generation in progress for this(old) ring
        ring->m_bGenerationInProgress = true;

        // set the new ring to queue
        ring->m_pQueuedRing = ring;

        ring->m_b_work_in_progress = boost::make_shared<bool>(true);

        if(pPreviousRing)
        {
            // inner bounds at generation time
            ring->m_ring_bounds_inner = pPreviousRing->m_ringBounds;
        }
        else
        {
            ring->m_ring_bounds_inner = AABB(float3(0,0,0), float3(0,0,0));
        }


        // push the ring onto the generation queue
        ring->m_pChunkManager->m_ring_generator_queue.push(ring);
    }

}

Clipmap::~Clipmap()
{

}

void Clipmap::renderBounds(Frustum& camera)
{

    ///Draw the octree frame
    std::vector<float> vertexData;
    for(int i = 0; i < m_ring_list.size(); i++)
    {
        auto p_ring = m_ring_list.at(i);

        const AABB& box = p_ring->m_ringBounds;

        float3 colour;
        //if(box.Contains(camera.pos))
        {
         //   colour = float3(1.0 , 1.0, 1.0);
        }
       // else
        {
            colour = float3((float)(1.0f/p_ring->m_scale), (float)(1.0f/p_ring->m_scale), 0.5);
        }

        for(const cube::edge_t& edge : cube::edge_t::all())
        {
            for(const cube::corner_t& corner : edge.corners())
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

    mesh->m_sp = m_ring_list.at(0)->m_pChunkManager->m_shaders[0];

    mesh->generateVAO();
    mesh->linkShaders();

               
    auto node = boost::make_shared<GfxApi::RenderNode>(mesh, float3(0, 0, 0), float3(1, 1, 1), float3(0, 0, 0));

    node->m_pMesh->applyVAO();
  //  if(m_pLastShader != node->m_pMesh->m_sp)
    {
        node->m_pMesh->m_sp->use();
    //    m_pLastShader = node->m_pMesh->m_sp;
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

void Clipmap::updateRings(Frustum& camera)
{
    // get the center ring
    auto p_first_ring = m_ring_list.at(0);
    boost::shared_ptr<Ring> pNextRing = nullptr;
    boost::shared_ptr<Ring> pPreviousRing = nullptr;

    // for all rings in the list ...
    for(auto& p_curr_ring : m_ring_list)
    {
        if(p_curr_ring->m_level < (m_max_level - 1))
        {
            pNextRing = m_ring_list.at(p_curr_ring->m_level + 1);
        }
        else
        {
            pNextRing = nullptr;
        }

        if(p_curr_ring->m_level > 0)
        {
            pPreviousRing = m_ring_list.at(p_curr_ring->m_level - 1);
        }
        else
        {
            pPreviousRing = nullptr;
        }

        // reference to the current ring
        //auto& p_curr_ring = m_ring_list.at(i);

        // if there is a ring in queue for generation
        if(p_curr_ring->m_bGenerationInProgress)
        {
            // check if all chunks a done generating
            if(p_curr_ring->m_pQueuedRing->allChunksGenrated() && !p_curr_ring->m_pQueuedRing->m_b_work_in_progress)
            {

                if(pNextRing && pNextRing->m_bGenerationInProgress)
                {
                    continue;
                }
                
                p_curr_ring->m_bGenerationInProgress = false;
                // replace the old ring with the new one
                p_curr_ring = p_curr_ring->m_pQueuedRing;
                //continue;

                //if(pNextRing  && !pNextRing->m_b_work_in_progress/*&& !pPreviousRing->m_bGenerationInProgress*/)
                //{
                //    if(pNextRing->m_seams_generated)
                //    {
                //        pNextRing->m_seam_nodes.clear();
                //        pNextRing->m_seam_indices.clear();
                //        pNextRing->m_seams_generated.reset();
                //        pNextRing->m_pMesh.reset();
                //        pNextRing->m_bGenerationInProgress = true;
                //        pNextRing->m_b_work_in_progress = boost::make_shared<bool>(true);
                //        pNextRing->m_ring_bounds_inner = p_curr_ring->m_ringBounds;
                //        pNextRing->m_pQueuedRing = pNextRing;
                //        pNextRing->m_pChunkManager->m_ring_generator_queue.push(pNextRing);
                //    }
                //}
            }
            else
            {
                // go to the next ring
                continue;
            }
        }

        // distance of the ring center to the border ( half length of ring )
        float len = ((p_curr_ring->m_n )* p_curr_ring->m_scale * (float(ChunkManager::CHUNK_SIZE))) / 2;

        // ring center position
        float3 ringCenter = (p_curr_ring->m_basePos + float3(len, len, len));

        float min_distance = 0;
        cube::face_t chosen_face = cube::face_t::all().at(0);

        // check distance to all ring borders
        for(auto face : cube::face_t::all())
        {
            // calulate the center of the face at the border
            float3 faceCenter = p_curr_ring->m_centerPos + 
                                float3(face.direction().x(), face.direction().y(), face.direction().z()) * 
                                (len - ChunkManager::CHUNK_SIZE * p_curr_ring->m_scale);

            // distance vector of camera to face center
            float3 q = (faceCenter - camera.pos);

            // take the length of the vector (well 90% of it)
            float d = q.Length() * 0.9;


            // if length is smaller than d, we need to this ring
            if( len < (d) )
            {
                if(d > min_distance || min_distance == 0)
                {
                    min_distance = d;
                    chosen_face = face;
                }
                
            }
        }

        if(min_distance != 0)
        {


            float3 offset = float3(chosen_face.opposite().direction().x(), 
                            chosen_face.opposite().direction().y(), 
                            chosen_face.opposite().direction().z()) 
                            * p_curr_ring->m_scale 
                            * ChunkManager::CHUNK_SIZE*2;

            float len = (p_curr_ring->m_n -2) * p_curr_ring->m_scale * (float(ChunkManager::CHUNK_SIZE));
            float3 basePos = (p_curr_ring->m_centerPos + offset - float3(len/2, len/2, len/2));
            float3 bottom = (basePos + float3(len, len, len));
            AABB ringBounds = AABB(basePos, bottom);

            if(pPreviousRing && !ringBounds.Contains(pPreviousRing->m_ringBounds))
            {
                continue;
            }

            auto newring = p_curr_ring->move(chosen_face);

            // set generation in progress for this(old) ring
            p_curr_ring->m_bGenerationInProgress = true;

            // set the new ring to queue
            p_curr_ring->m_pQueuedRing = newring;

            newring->m_b_work_in_progress = boost::make_shared<bool>(true);

            if(pPreviousRing)
            {
                // inner bounds at generation time
                newring->m_ring_bounds_inner = pPreviousRing->m_ringBounds;
            }
            else
            {
                newring->m_ring_bounds_inner = AABB(float3(0,0,0), float3(0,0,0));
            }

            // push the ring onto the generation queue
            newring->m_pChunkManager->m_ring_generator_queue.push(newring);

			//if(pNextRing)
			//{
			//	pNextRing->m_pQueuedRing = pNextRing->copy_base();
			//	pNextRing->m_bGenerationInProgress = true;
			//	pNextRing->m_pQueuedRing->m_ring_bounds_inner = newring->m_ringBounds;
			//	pNextRing->m_pQueuedRing->m_b_work_in_progress = boost::make_shared<bool>(true);
			//	pNextRing->m_pChunkManager->m_ring_generator_queue.push(pNextRing->m_pQueuedRing);
			//}
			
        }
    }

    return;
}

void Clipmap::draw(Frustum& camera, bool hideSeams, bool hideTerrain)
{

    auto p_curr_ring = m_rings;
    

    for(int i = 0; i < m_ring_list.size(); i++)
    {
        boost::shared_ptr<Ring> p_prev_ring = nullptr;
        auto& p_curr_ring = m_ring_list.at(i);

        if(!p_curr_ring->allChunksGenrated())
        {
            continue;
        }

        //bool found = true;
        int index_count = 0;
        if(!p_curr_ring->m_b_work_in_progress && !p_curr_ring->m_seams_generated)
        {
            p_curr_ring->createSeamMesh();

            p_curr_ring->m_seams_generated = boost::make_shared<bool>(true);
        }

        if(p_curr_ring->m_pMesh && !hideSeams)
        {
            auto node = boost::make_shared<GfxApi::RenderNode>(p_curr_ring->m_pMesh, p_curr_ring->m_basePos,
                                                                float3(p_curr_ring->m_scale), 
                                                                float3(0.0f));
            node->m_pMesh->applyVAO();

            node->m_pMesh->m_sp->use();

            node->m_pMesh->m_sp->setUniform(node->m_xForm, "world");
            node->m_pMesh->m_sp->setUniform(camera.ViewProjMatrix(), "worldViewProj");
            node->m_pMesh->m_sp->setUniform(p_curr_ring->m_scale, "res");
            node->m_pMesh->m_sp->setUniform(0, "tex1");
            node->m_pMesh->m_sp->setUniform(1, "tex2");
            node->m_pMesh->m_sp->setUniform(camera.pos.x, camera.pos.y, camera.pos.z, "cameraPos");  
            node->m_pMesh->draw();
                    
            GfxApi::Mesh::unbindVAO();
        }
    }
}