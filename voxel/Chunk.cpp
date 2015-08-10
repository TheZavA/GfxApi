#include "Chunk.h"
#include <assert.h>
#include "ChunkManager.h"
#include "../ocl.h"
#include "../MainClass.h"
#include <GI/gi.h>
#include "../cubelib/cube.hpp"
#include "Clipmap.h"

extern MainClass* g_pMainClass;

const float QEF_ERROR = 1e-1f;
const int QEF_SWEEPS = 4;

Chunk::Chunk(float3& basePos, std::size_t gridX, std::size_t gridY, std::size_t gridZ, float scale, ChunkManager* pChunkManager, const AABB& ringBounds, boost::shared_ptr<Ring> p_ring)
    //: m_bounds(bounds)
    : m_basePos(basePos)
    , m_pMesh(nullptr)
    , m_x(gridX)
    , m_y(gridY)
    , m_z(gridZ)
    , m_pChunkManager(pChunkManager)
    , m_pIndices(nullptr)
    , m_pVertices(nullptr)
    , m_pOctreeNodes(nullptr)
    , m_bHasNodes(false)
    , m_nodeDrInf(0)
    , m_nodeIdxCurr(0)
    , m_ringBounds(ringBounds)
	, m_workInProgress(nullptr)
   // , m_p_parent_ring(p_ring)
{
    m_scale = scale;
}

Chunk::~Chunk()
{
    //std::cout << " Chunk destroyed\n";
}


void Chunk::draw()
{

}

void Chunk::generateDensities()
{
    float3 pos = m_basePos + float3(m_x, m_y, m_z) * m_scale * ChunkManager::CHUNK_SIZE;

    m_pChunkManager->m_ocl->density3d( ChunkManager::CHUNK_SIZE + 2, pos.x, pos.y, pos.z, m_scale);
}

void Chunk::buildTree(const int size, const float threshold)
{
    
    auto leafs = createLeafNodes();

    m_octree_root_index = createOctree(leafs, threshold, false);

    m_edgesCompact.reset();
    m_zeroCrossingsCl3d.reset();
}

int toint(uint8_t x, uint8_t y, uint8_t z)
{
    int index = 0;
    ((char*)(&index))[0] = x;
    ((char*)(&index))[1] = y;
    ((char*)(&index))[2] = z;
    return index;
}

uint32_t Chunk::createOctree(std::vector<uint32_t>& leaf_indices, const float threshold, bool simplify = true)
{

    int index = 0;

    while(leaf_indices.size() > 1)
    {

        std::vector<uint32_t> tmp_idx_vec;
        std::unordered_map<int, uint32_t> tmp_map;
        for(auto& nodeIdx : leaf_indices)
        {
            auto node = &(*m_pOctreeNodes)[nodeIdx];
            OctreeNode* new_node = nullptr;

            boost::array<int,3> pos;

            int level = node->size * 2;

            pos[0] = (node->minx / level) * level;
            pos[1] = (node->miny / level) * level;
            pos[2] = (node->minz / level) * level;

            auto& it = tmp_map.find(toint(pos[0], pos[1], pos[2]));
            if(it == tmp_map.end())
            {
                uint32_t index = m_nodeIdxCurr;
                if(m_nodeIdxCurr >= m_pOctreeNodes->size())
                {
                    m_pOctreeNodes->push_back(OctreeNode());
                }
                new_node = &(*m_pOctreeNodes)[m_nodeIdxCurr++];
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
                new_node = &(*m_pOctreeNodes)[it->second];
            }

            boost::array<int,3> diff;
            diff[0] = pos[0] - node->minx;
            diff[1] = pos[1] - node->miny;
            diff[2] = pos[2] - node->minz;

            float3 corner_pos = float3(diff[0]?1:0, diff[1]?1:0, diff[2]?1:0);
			

			uint8_t idx = ((uint8_t)corner_pos.x << 2) |
					      ((uint8_t)corner_pos.y << 1) |
						  ((uint8_t)corner_pos.z);

            new_node->childrenIdx[idx] = nodeIdx;

        }
        leaf_indices.swap(tmp_idx_vec);
    }


    if(leaf_indices.size() == 0)
    {
        return -1;
    }

    if(m_scale != LOD_MIN_RES)
        return simplifyOctree(leaf_indices[0], 1.0f, nullptr);

    return leaf_indices[0];
   
}


void Chunk::createVertices()
{
    boost::shared_ptr<IndexBuffer> pIndices = boost::make_shared<IndexBuffer>();
    boost::shared_ptr<VertexBuffer> pVertices = boost::make_shared<VertexBuffer>();    

    GenerateMeshFromOctree(m_octree_root_index, *pVertices, *pIndices, *m_pOctreeNodes);


    if(pVertices->size() <= 1 || pIndices->size() <= 1)
    {
        //m_pOctreeNodes.reset();
        return;
    }
    m_pVertices = pVertices;
    m_pIndices = pIndices;
}

void Chunk::generateFullEdges()
{
    auto zeroCl3d = boost::make_shared<TVolume3d<zeroCrossings_cl_t>>(ChunkManager::CHUNK_SIZE + 2,
                                                                      ChunkManager::CHUNK_SIZE + 2, 
                                                                      ChunkManager::CHUNK_SIZE + 2);
#ifdef _DEBUG
    memset(zeroCl3d->getDataPtr(), 0, zeroCl3d->m_xyzSize * sizeof(zeroCrossings_cl_t));
#endif

    auto cornersCompact = boost::make_shared<std::vector<cl_int4_t>>();
    
    std::vector<zeroCrossings_cl_t*> zeroCrossCompactPtr;

    m_bHasNodes = false;
    for(auto &edge : *m_edgesCompact)
    {
        // duplicate each edge to its neighbours
        for(int y = 0; y < 4; y++)
        {

            int edge_index = edgeToIndex[edge.edge];
            int x1 = edge.grid_pos[0] + edgeDuplicateMask[edge_index][y][0];
            int y1 = edge.grid_pos[1] + edgeDuplicateMask[edge_index][y][1];
            int z1 = edge.grid_pos[2] + edgeDuplicateMask[edge_index][y][2];

            if( x1 < 0 || y1 < 0 || z1 < 0 ||
                x1 > ChunkManager::CHUNK_SIZE ||
                y1 > ChunkManager::CHUNK_SIZE ||
                z1 > ChunkManager::CHUNK_SIZE )
            {
                continue;
            }
            m_bHasNodes = true;

            auto& zero1 = (*zeroCl3d)(x1, y1, z1);

            if(zero1.edgeCount == 0)
            {
                zeroCrossCompactPtr.push_back(&zero1);
                cl_int4_t pos;
                pos.x = x1;
                pos.y = y1;
                pos.z = z1;
                pos.w = 0;
                cornersCompact->push_back(pos);
            }

            zero1.positions[zero1.edgeCount][0] = edge.zero_crossing.x;
            zero1.positions[zero1.edgeCount][1] = edge.zero_crossing.y;
            zero1.positions[zero1.edgeCount][2] = edge.zero_crossing.z;

            zero1.normals[zero1.edgeCount][0] = edge.normal.x;
            zero1.normals[zero1.edgeCount][1] = edge.normal.y;
            zero1.normals[zero1.edgeCount][2] = edge.normal.z;

            zero1.corners = edge.grid_pos[3];

            zero1.xPos = x1;
            zero1.yPos = y1;
            zero1.zPos = z1;

            zero1.edgeCount++;
        }

    }

    if(zeroCrossCompactPtr.size() > 0)
    {
        m_pChunkManager->m_ocl->getCompactCorners(&(*cornersCompact)[0], (*cornersCompact).size());

        auto zeroCrossCompact = boost::make_shared<std::vector<zeroCrossings_cl_t>>();
        for(auto crossingPtr : zeroCrossCompactPtr)
        {
            zeroCrossCompact->push_back(*crossingPtr);
        }

         m_zeroCrossCompact = zeroCrossCompact;
    }
    m_cornersCompact = cornersCompact;

}

void Chunk::createMesh()
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
    m_zeroCrossCompact.reset();
    m_pOctreeNodes.reset();

}

boost::shared_ptr<GfxApi::Mesh> Chunk::getMeshPtr()
{
    return m_pMesh;
}

void Chunk::classifyEdges()
{

    float3 pos = m_basePos + float3(m_x, m_y, m_z) * m_scale * ChunkManager::CHUNK_SIZE;
    
    std::vector<cl_edge_info_t> compactedEdges;

    m_pChunkManager->m_ocl->classifyEdges(nullptr, 
                                          ChunkManager::CHUNK_SIZE + 2, 
                                          pos.x,
                                          pos.y,
                                          pos.z,
                                          m_scale,
                                          compactedEdges);

    auto edges_compact = boost::make_shared<std::vector<edge_t>>();


    for(auto& edge : compactedEdges)
    {
        m_bHasNodes = true;
        edge_t edge1;
        edge1.grid_pos[0] = edge.grid_pos[0];
        edge1.grid_pos[1] = edge.grid_pos[1];
        edge1.grid_pos[2] = edge.grid_pos[2];

        uint8_t& edge_mask = edge.edge_info;
        if(edge_mask & 1) 
        {
            edge1.edge = 0;
            edges_compact->push_back(edge1);
        }
        if(edge_mask & 2) 
        {
            edge1.edge = 4;
            edges_compact->push_back(edge1);
        }
        if(edge_mask & 4) 
        {
            edge1.edge = 8;
            edges_compact->push_back(edge1);
        }
    }

    if(edges_compact->size() > 0)
    {
        m_edgesCompact = edges_compact;
    }

}

void Chunk::generateZeroCross()
{

    float3 pos = m_basePos + float3(m_x, m_y, m_z) * m_scale * ChunkManager::CHUNK_SIZE;

    m_pChunkManager->m_ocl->generateZeroCrossing(*m_edgesCompact, pos.x, pos.y, pos.z, m_scale);

}


std::vector<uint32_t> Chunk::createLeafNodes()
{
    std::vector<uint32_t> leaf_indices;

    m_pOctreeNodes = boost::make_shared<std::vector<OctreeNode>>(m_edgesCompact->size() *2);

    int off = 0;
    for(auto& zero : *m_zeroCrossCompact)
    {
        auto& corners = (*m_cornersCompact)[off++];


       /* if( zero.xPos > (ChunkManager::CHUNK_SIZE)|| zero.yPos > (ChunkManager::CHUNK_SIZE) || zero.zPos > (ChunkManager::CHUNK_SIZE) ||
            zero.xPos < 0 || zero.yPos < 0 || zero.zPos < 0)
        {
            continue;
        }*/
        
        zeroCrossings_cl_t& zeroCl = zero;

        OctreeDrawInfo drawInfo;
        drawInfo.averageNormal = float3(0,0,0);
	 
        QefSolver qef;
        for (int i = 0; i < zeroCl.edgeCount; i++)
	    {
            qef.add(zeroCl.positions[i][0], zeroCl.positions[i][1], zeroCl.positions[i][2], 
                    zeroCl.normals[i][0],   zeroCl.normals[i][1],   zeroCl.normals[i][2]);
        }

        Vec3 qefPosition;
	    qef.solve(qefPosition, QEF_ERROR, 4, QEF_ERROR);

        drawInfo.position.x = qefPosition.x;
        drawInfo.position.y = qefPosition.y;
        drawInfo.position.z = qefPosition.z;
        drawInfo.qef = qef.getData();

	    if (drawInfo.position.x < zeroCl.xPos || drawInfo.position.x > float(zeroCl.xPos + 1.0f) ||
		    drawInfo.position.y < zeroCl.yPos || drawInfo.position.y > float(zeroCl.yPos + 1.0f) ||
		    drawInfo.position.z < zeroCl.zPos || drawInfo.position.z > float(zeroCl.zPos + 1.0f) )
	    {
            drawInfo.position.x = qef.getMassPoint().x;
            drawInfo.position.y = qef.getMassPoint().y;
            drawInfo.position.z = qef.getMassPoint().z;
	    }
    
	    for (int i = 0; i < zeroCl.edgeCount; i++)
	    {
		    drawInfo.averageNormal += float3(zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2]);
	    }

        drawInfo.averageNormal = drawInfo.averageNormal.Normalized();
        drawInfo.corners = corners.w;

        OctreeNode *leaf;
        uint32_t count = m_nodeIdxCurr;
        if(m_nodeIdxCurr >= m_pOctreeNodes->size())
        {
            m_pOctreeNodes->push_back(OctreeNode());
        }
        leaf = &(*m_pOctreeNodes)[m_nodeIdxCurr++];
        leaf->size       = 1;
        leaf->minx       = zeroCl.xPos;
        leaf->miny       = zeroCl.yPos;
        leaf->minz       = zeroCl.zPos;
	    leaf->type       = Node_Leaf;

        leaf->drawInfo.averageNormal = drawInfo.averageNormal;
        leaf->drawInfo.corners = drawInfo.corners;
        leaf->drawInfo.index = drawInfo.index;
        leaf->drawInfo.position = drawInfo.position;
        leaf->drawInfo.qef = drawInfo.qef;


        //// its on a border, needs to be added to the chunk border list for seams
        if((zero.xPos <= ChunkManager::CHUNK_SIZE -1  
         && zero.yPos <= ChunkManager::CHUNK_SIZE -1
         && zero.zPos <= ChunkManager::CHUNK_SIZE -1
         && zero.xPos >= 0.0f
         && zero.yPos >= 0.0f 
         && zero.zPos >= 0.0f))
        {
            m_seam_nodes.push_back(*leaf);
        }

        if( zero.xPos <= ChunkManager::CHUNK_SIZE +1  
         && zero.yPos <= ChunkManager::CHUNK_SIZE +1 
         && zero.zPos <= ChunkManager::CHUNK_SIZE +1 
         && zero.xPos >= 0 
         && zero.yPos >= 0 
         && zero.zPos >= 0)
        {
            leaf_indices.push_back(count);
        }
      
        
    }

    return leaf_indices;
}

uint32_t Chunk::simplifyOctree(uint32_t node_index, float threshold, OctreeNode* root)
{
	if (node_index == -1)
	{
		return -1;
	}

    if ((*m_pOctreeNodes)[node_index].type != Node_Internal)
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
		(*m_pOctreeNodes)[node_index].childrenIdx[i] = simplifyOctree((*m_pOctreeNodes)[node_index].childrenIdx[i], threshold, root);
		if ((*m_pOctreeNodes)[node_index].childrenIdx[i] != -1)
		{
			OctreeNode* child = &(*m_pOctreeNodes)[(*m_pOctreeNodes)[node_index].childrenIdx[i]];
			if (child->type == Node_Internal)
			{
				isCollapsible = false;
			}
			else
			{
                if (child->drawInfo.position.x >= 1 && child->drawInfo.position.x < ChunkManager::CHUNK_SIZE -1 &&
		            child->drawInfo.position.y >= 1 && child->drawInfo.position.y < ChunkManager::CHUNK_SIZE -1 &&
		            child->drawInfo.position.z >= 1 && child->drawInfo.position.z < ChunkManager::CHUNK_SIZE -1)
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
        if(ret == 0)
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

	if (position.x < (*m_pOctreeNodes)[node_index].minx || position.x > float((*m_pOctreeNodes)[node_index].minx ) + 1.0f ||
		position.y < (*m_pOctreeNodes)[node_index].miny || position.y > float((*m_pOctreeNodes)[node_index].miny ) + 1.0f ||
		position.z < (*m_pOctreeNodes)[node_index].minz || position.z > float((*m_pOctreeNodes)[node_index].minz ) + 1.0f)
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
		if ((*m_pOctreeNodes)[node_index].childrenIdx[i] != -1)
		{
			OctreeNode* child = &(*m_pOctreeNodes)[(*m_pOctreeNodes)[node_index].childrenIdx[i]];
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
		(*m_pOctreeNodes)[node_index].childrenIdx[i] = -1;
	}

	(*m_pOctreeNodes)[node_index].type = Node_Psuedo;
	(*m_pOctreeNodes)[node_index].drawInfo = drawInfo;

	return node_index;
}