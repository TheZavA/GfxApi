#include "Chunk.h"
#include <assert.h>
#include "ChunkManager.h"
#include "../ocl.h"

Chunk::Chunk(AABB& bounds, ChunkManager* pChunkManager)
    : m_bounds(bounds)
    , m_pMesh(nullptr)
    , m_treeRoot(nullptr)
    , m_pChunkManager(pChunkManager)
    , m_pIndices(nullptr)
    , m_pVertices(nullptr)
    , m_bHasNodes(false)
    , m_leafCount(0)
{
    m_scale = ((m_bounds.MaxX() - m_bounds.MinX())/(ChunkManager::CHUNK_SIZE));
    m_workInProgress = boost::make_shared<bool>(false);
    m_bLoadingDone = boost::make_shared<bool>(false);
}

Chunk::~Chunk()
{
}

void Chunk::generateDensities()
{
    auto densities3d = boost::make_shared<TVolume3d<float>>(ChunkManager::CHUNK_SIZE+2, ChunkManager::CHUNK_SIZE+2, ChunkManager::CHUNK_SIZE+2);


    m_pChunkManager->m_ocl->density3d(densities3d->getDataPtr(), ChunkManager::CHUNK_SIZE+2, m_bounds.MinX(), m_bounds.MinY(), m_bounds.MinZ(), m_scale);

    m_densities3d = densities3d;
}

void Chunk::buildTree(const int size, const float threshold)
{
    m_treeRoot = createOctree(m_bounds.minPoint, ChunkManager::CHUNK_SIZE*2, threshold);
}

OctreeNode* Chunk::createOctree(const float3& min, const int size, const float threshold)
{
	OctreeNode* root = new OctreeNode;
	root->min = min;
	root->size = size;
	root->type = Node_Internal;

    if(m_cornersCompact->size() > 0)
    {
	    constructOctreeNodes(root);
    
        root = simplifyOctree(root, threshold, root);
    }

    m_corners3d.reset();
    m_cornersCompact.reset();
    m_zeroCrossingsCl3d.reset();
	return root;
}

void Chunk::createVertices()
{
    boost::shared_ptr<IndexBuffer> pIndices = boost::make_shared<IndexBuffer>();
    boost::shared_ptr<VertexBuffer> pVertices = boost::make_shared<VertexBuffer>();

    
    assert(m_treeRoot);

    GenerateMeshFromOctree(m_treeRoot, *pVertices, *pIndices);

    if(pVertices->size() <= 1)
    {
        return;
    }

    m_pVertices = pVertices;
    m_pIndices = pIndices;
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

}

boost::shared_ptr<GfxApi::Mesh> Chunk::getMeshPtr()
{
    return m_pMesh;
}

void Chunk::generateCorners()
{
    auto corners3d = boost::make_shared<TVolume3d<corner_cl_t>>(ChunkManager::CHUNK_SIZE + 1, 
                                                                ChunkManager::CHUNK_SIZE + 1, 
                                                                ChunkManager::CHUNK_SIZE + 1);

    auto cornersCompact = boost::make_shared<std::vector<corner_cl_t>>();

    m_pChunkManager->m_ocl->generateCorners(m_densities3d->getDataPtr(), 
                                            corners3d->getDataPtr(), 
                                            ChunkManager::CHUNK_SIZE + 1, 
                                            m_bounds.MinX(), 
                                            m_bounds.MinY(), 
                                            m_bounds.MinZ(), 
                                            m_scale);

    std::size_t index = 0;

    for(int y = 0; y < ChunkManager::CHUNK_SIZE + 1; y++)
    {
        for(int z = 0; z < ChunkManager::CHUNK_SIZE + 1; z++)
        {
            for(int x = 0; x < ChunkManager::CHUNK_SIZE + 1; x++, index++)
            {
                corner_cl_t& corners = (*corners3d)[index];
                if(corners.corners != 0 && corners.corners != 255)
                {
                    cornersCompact->push_back(corners);
                }
	            
            }
        }
    }

    if(cornersCompact->size() > 0)
    {
        m_bHasNodes = true;
    }

    m_corners3d = corners3d;
    m_cornersCompact = cornersCompact;
}
void Chunk::generateZeroCrossings()
{
    auto zeroCl3d = boost::make_shared<TVolume3d<zeroCrossings_cl_t>>(ChunkManager::CHUNK_SIZE + 1,
                                                                      ChunkManager::CHUNK_SIZE + 1, 
                                                                      ChunkManager::CHUNK_SIZE + 1);

    if(m_cornersCompact->size() > 0)
    {
        m_pChunkManager->m_ocl->generateZeroCrossings(*m_cornersCompact, 
                                                      zeroCl3d->getDataPtr(),
                                                      ChunkManager::CHUNK_SIZE + 1,
                                                      m_bounds.MinX(), 
                                                      m_bounds.MinY(),
                                                      m_bounds.MinZ(),
                                                      m_scale);
   
        m_zeroCrossingsCl3d = zeroCl3d;
    }

}

OctreeNode* Chunk::constructLeaf(OctreeNode* leaf)
{
    if (!leaf || leaf->size != 1 || m_cornersCompact->size() == 0)
	{
		return nullptr;
	}

    const float3 cornerPos = leaf->min - m_bounds.minPoint;
    corner_cl_t corners = (*m_corners3d)(cornerPos.x, cornerPos.y, cornerPos.z);


	if (corners.corners == 0 || corners.corners == 255)
	{
		// voxel is full inside or outside the volume
		delete leaf;
		return nullptr;
	}
   
    m_leafCount++;

    zeroCrossings_cl_t& zeroCl = (*m_zeroCrossingsCl3d)(cornerPos.x, cornerPos.y, cornerPos.z);

	// otherwise the voxel contains the surface, so find the edge intersections

	OctreeDrawInfo* drawInfo = new OctreeDrawInfo;
    memset(drawInfo, 0, sizeof(OctreeDrawInfo));

	drawInfo->qef.initialise(zeroCl.edgeCount, zeroCl.normals, zeroCl.positions);
	drawInfo->position = drawInfo->qef.solve();

	const float3 min = float3(leaf->min);
	const float3 max = float3(leaf->min + float3(leaf->size));
	if (drawInfo->position.x < min.x || drawInfo->position.x > max.x ||
		drawInfo->position.y < min.y || drawInfo->position.y > max.y ||
		drawInfo->position.z < min.z || drawInfo->position.z > max.z)
	{
		drawInfo->position = drawInfo->qef.masspoint;
	}

	for (int i = 0; i < zeroCl.edgeCount; i++)
	{
		drawInfo->averageNormal += float3(zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2]);
	}
    drawInfo->averageNormal = drawInfo->averageNormal.Normalized();

	drawInfo->corners = corners.corners;

	leaf->type = Node_Leaf;
	leaf->drawInfo = drawInfo;

	return leaf;
}

OctreeNode* Chunk::constructOctreeNodes(OctreeNode* node)
{

	if (!node)
	{
		return nullptr;
	}

	if (node->size == 1)
	{
		return constructLeaf(node);
	}
	
	const int childSize = node->size / 2;
	bool hasChildren = false;

	for (int i = 0; i < 8; i++)
	{
        float3 min = node->min + (CHILD_MIN_OFFSETS[i] * childSize);

        const float3 cornerPos = min - m_bounds.minPoint;

        if(cornerPos.x < 33 && cornerPos.y < 33 && cornerPos.z < 33)
        {
            OctreeNode* child = new OctreeNode;
		    child->size = childSize;
		    child->min = min;
		    child->type = Node_Internal;

		    node->children[i] = constructOctreeNodes(child);
        }
        else
        {
            node->children[i] = nullptr;
        }

		hasChildren |= (node->children[i] != nullptr);
	}

    if (!hasChildren)
	{
		delete node;
		return nullptr;
	}

	return node;
}

OctreeNode* Chunk::simplifyOctree(OctreeNode* node, float threshold, OctreeNode* root)
{
	if (!node)
	{
		return NULL;
	}

	if (node->type != Node_Internal)
	{
		// can't simplify!
		return node;
	}

	QEF qef;
	int signs[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
	int midsign = -1;
	int edgeCount = 0;
	bool isCollapsible = true;

	for (int i = 0; i < 8; i++)
	{
		node->children[i] = simplifyOctree(node->children[i], threshold, root);
		if (node->children[i])
		{
			OctreeNode* child = node->children[i];
			if (child->type == Node_Internal)
			{
				isCollapsible = false;
			}
			else
			{
                if (child->drawInfo->position.x > 1 && child->drawInfo->position.x < ChunkManager::CHUNK_SIZE  &&
		            child->drawInfo->position.y > 1 && child->drawInfo->position.y < ChunkManager::CHUNK_SIZE  &&
		            child->drawInfo->position.z > 1 && child->drawInfo->position.z < ChunkManager::CHUNK_SIZE )
                {

				    qef.add(child->drawInfo->qef);

				    midsign = (child->drawInfo->corners >> (7 - i)) & 1; 
				    signs[i] = (child->drawInfo->corners >> i) & 1; 

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
		return node;
	}

	// at this point the masspoint will actually be a sum, so divide to make it the average
	qef.masspoint /= (float)edgeCount;
	float3 position = qef.solve();

	if (qef.error > threshold)
	{
		// this collapse breaches the threshold
		return node;
	}

	if (position[0] < node->min.x || position[0] > (node->min.x + node->size) ||
		position[1] < node->min.y || position[1] > (node->min.y + node->size) ||
		position[2] < node->min.z || position[2] > (node->min.z + node->size))
	{
		position = qef.masspoint;
	}

	// change the node from an internal node to a 'psuedo leaf' node
	OctreeDrawInfo* drawInfo = new OctreeDrawInfo;

	for (int i = 0; i < 8; i++)
	{
		if (signs[i] == -1)
		{
			// Undetermined, use centre sign instead
			drawInfo->corners |= (midsign << i);
		}
		else 
		{
			drawInfo->corners |= (signs[i] << i);
		}
	}

	drawInfo->averageNormal = float3(0.f);
	for (int i = 0; i < 8; i++)
	{
		if (node->children[i])
		{
			OctreeNode* child = node->children[i];
			if (child->type == Node_Psuedo || 
				child->type == Node_Leaf)
			{
				drawInfo->averageNormal += child->drawInfo->averageNormal;
			}
		}
	}

    drawInfo->averageNormal = drawInfo->averageNormal.Normalized();
	drawInfo->position = position;
	drawInfo->qef = qef;

	for (int i = 0; i < 8; i++)
	{
		DestroyOctree(node->children[i]);
		node->children[i] = nullptr;
	}

	node->type = Node_Psuedo;
	node->drawInfo = drawInfo;

	return node;
}