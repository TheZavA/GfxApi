#include "Chunk.h"
#include "ChunkManager.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>

#include "voxel/McTable.h"

#include "GfxApi.h"

#include "noisepp/core/Noise.h"
#include "noisepp/utils/NoiseUtils.h"
#include "xmlnoise/xml_noise3d.hpp"
#include "xmlnoise/xml_noise3d_handlers.hpp"
#include "xmlnoise/xml_noise2d.hpp"
#include "xmlnoise/xml_noise2d_handlers.hpp"

#include <tuple>

#include "cubelib\cube.hpp"


float3 LinearInterp(Vector3Int p1, float p1Val, Vector3Int p2, float p2Val,  float value)
{

    float3 p1Float(std::get<0>(p1), std::get<1>(p1), std::get<2>(p1));
    float3 p2Float(std::get<0>(p2), std::get<1>(p2), std::get<2>(p2));

    float t = 1 * (p2Val - value) / (p2Val - p1Val);
	
    float sum1 = (p2Val - p1Val);

    float fSum = (value - p1Val) ;

    float3 sum((p2Float.x - p1Float.x) * fSum, (p2Float.y - p1Float.y) * fSum, (p2Float.z - p1Float.z) * fSum);

    sum /= sum1;


    return float3(p1Float.x + sum.x, p1Float.y + sum.y, p1Float.z + sum.z);

}



Chunk::Chunk(AABB bounds, double scale, ChunkManager* pChunkManager)
    : m_scale(scale)
    , m_bounds(bounds)
    , m_pChunkManager(pChunkManager)
    , m_blockVolumeFloat(nullptr)
    , m_pMesh(nullptr)
{
    m_workInProgress = boost::make_shared<bool>(false);

}


Chunk::~Chunk(void)
{
 
}

void Chunk::generateBaseMap(void)
{
    auto& tmpBaseMap = boost::make_shared<TVolume2d<float>>(ChunkManager::CHUNK_SIZE + 2, ChunkManager::CHUNK_SIZE + 2);

    auto& tmpSlopeMap = boost::make_shared<TVolume2d<float>>(ChunkManager::CHUNK_SIZE + 2, ChunkManager::CHUNK_SIZE + 2);

   

    boost::scoped_ptr<noisepp::Pipeline2D> pipeline( noisepp::utils::System::createOptimalPipeline2D());
    

    xml_noise2d_t xml_noise2d(*pipeline);
    xml_noise2d_t xml_biome2d(*pipeline);

    register_all_2dhandlers(xml_noise2d.handlers);
    register_all_2dhandlers(xml_biome2d.handlers);

    xml_biome2d.load("zoneDef.xml");
    xml_noise2d.load("baseMap.xml");



    noisepp::PerlinModule baseContinentDef_pe2;
    baseContinentDef_pe2.setSeed (1 + 4);
    baseContinentDef_pe2.setFrequency (0.001);
    baseContinentDef_pe2.setLacunarity (2.12312);
    baseContinentDef_pe2.setOctaveCount (8);
    baseContinentDef_pe2.setQuality (1);
    baseContinentDef_pe2.setScale(0.5);


    noisepp::ElementID rootid = baseContinentDef_pe2.addToPipeline(pipeline.get());
    noisepp::PipelineElement2D* rootelement = pipeline->getElement(rootid);

    noisepp::Cache *cache = pipeline->createCache();

    size_t index = 0;

    float worldX = m_bounds.MinX();
    float worldZ = m_bounds.MinZ();

    double res = ((m_bounds.MaxX() - m_bounds.MinX())/ChunkManager::CHUNK_SIZE);

    for(int z = 0; z < ChunkManager::CHUNK_SIZE + 2; z++)
    {
        for(int x = 0; x < ChunkManager::CHUNK_SIZE + 2; x ++, index++)
        {
            float noise =  (rootelement->getValue((worldX + (double)x * res), (worldZ + (double)z * res), cache))  ;

            float& value = (*tmpBaseMap)(x, z);
            float& slope = (*tmpSlopeMap)(x, z);
            value = noise;

            float heightz0 = (1 + rootelement->getValue((worldX + x * res), (worldZ + z * res- res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightz1 = (1 + rootelement->getValue((worldX + x * res), (worldZ + z * res + res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightx0 = (1 + rootelement->getValue((worldX + x * res - res ), (worldZ + z* res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightx1 = (1 + rootelement->getValue((worldX + x * res + res ), (worldZ + z* res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);

            float3 tt(0, 0, 0); 	
            float3 dv1(0, heightz0 - heightz1, 2);
            float3 dv2(2, heightx0 - heightx1, 0);
            tt = dv1.Cross(dv2);
            tt.Normalize();
            slope = tt.y;

        }    
    }
   

    pipeline->freeCache(cache);

    m_slopeMap = tmpSlopeMap;
    m_baseMap = tmpBaseMap;
    

}


void Chunk::generateTerrain(void)
{
    boost::shared_ptr<TVolume3d<float>> tmpVolumeFloat = boost::make_shared<TVolume3d<float>>(
                                                                                ChunkManager::CHUNK_SIZE + 2,
                                                                                ChunkManager::CHUNK_SIZE + 2,
                                                                                ChunkManager::CHUNK_SIZE + 2);
	std::size_t index = 0;


    boost::scoped_ptr<noisepp::Pipeline3D> pipeline( noisepp::utils::System::createOptimalPipeline3D());
    xml_noise3d_t xml_noise3d(*pipeline);
    register_all_3dhandlers(xml_noise3d.handlers);
    xml_noise3d.load("something.xml");
    assert(xml_noise3d.root);
    noisepp::ElementID rootid = xml_noise3d.root->addToPipeline(pipeline.get());
    noisepp::PipelineElement3D* rootelement = pipeline->getElement(rootid);
    noisepp::Cache *cache = pipeline->createCache();
  
    float worldX = m_bounds.MinX();
    float worldY = m_bounds.MinY();
    float worldZ = m_bounds.MinZ();

    double res = ((m_bounds.MaxX()-m_bounds.MinX())/ChunkManager::CHUNK_SIZE);

    for(int z = 0; z < (ChunkManager::CHUNK_SIZE) + 2; z++)
	{
        for(int y = 0; y < (ChunkManager::CHUNK_SIZE) + 2; y++)
	    {
		    for(int x = 0; x < (ChunkManager::CHUNK_SIZE) + 2; x ++, index ++)
		    {
                float& value = (*tmpVolumeFloat)(x, y, z);


                value = rootelement->getValue((worldX + (double)x * res), (worldY + (double)y * res), (worldZ + (double)z * res), cache) ;
            
            }
        }
    }


 //   float yPos = (m_bounds.MinY());
 //   float worldY = m_bounds.MinY();
 //   double res = ((m_bounds.MaxX()-m_bounds.MinX())/ChunkManager::CHUNK_SIZE);

 //   for(int z = 0; z < (ChunkManager::CHUNK_SIZE) + 2; z++)
	//{
 //       for(int y = 0; y < (ChunkManager::CHUNK_SIZE) + 2; y++)
	//    {
	//	    for(int x = 0; x < (ChunkManager::CHUNK_SIZE) + 2; x ++, index ++)
	//	    {
 //               double worldYCurr = (worldY + (double)y * res);

 //               float value = (*m_baseMap)(x, z);

 //               int height = (value)  * (600) ;

 //               if(height > worldYCurr)
 //               {
 //                   float& value3d = (*tmpVolumeFloat)(x, y, z);
 //                   value3d = value ;
 //                   //printf("%f", value);
 //               }

 //               //if(height == worldYCurr)
 //               //{
 //               //    float& value3d = (*tmpVolumeFloat)(x, y, z);
 //               //    float value1 = x > 0 ? (*m_baseMap)(x-1, z) : value;
 //               //    float value2 = x < ((ChunkManager::CHUNK_SIZE) + 1) ? (*m_baseMap)(x+1, z) : value;
 //               //    value3d = ((value + value1 + value2)/3)/2;
 //               //}
 //           }

 //       }
 //   }
 //      
    m_blockVolumeFloat = tmpVolumeFloat;
	assert(m_blockVolumeFloat);
}

EdgeIndex Chunk::makeEdgeIndex(Vector3Int& vertA, Vector3Int& vertB)
{
    if(vertA < vertB)
    {
        return std::make_pair(vertA, vertB);
    }

    return std::make_pair(vertB, vertA);

}

void Chunk::generateVertices(void)
{
    auto& pVertexData = boost::make_shared<std::vector<Vertex>>();
    auto& pIndexData = boost::make_shared<std::vector<int>>();

    std::map<EdgeIndex, float3> normalMap;
    std::map<EdgeIndex, uint32_t> vertexMap;

    std::size_t index = 0;
    for(std::size_t y = 1; y < (ChunkManager::CHUNK_SIZE + 1); y++)
    {
        for(std::size_t z = 1; z < (ChunkManager::CHUNK_SIZE + 1); z++)
        {
            for(std::size_t x = 1; x < (ChunkManager::CHUNK_SIZE + 1); x++)
            {

                float blocks[8];

                int x0 = x;
                int y0 = y;
                int z0 = z;
	
                int x1 =  x0 + 1;
                int y1 =  y0 + 1;
                int z1 =  z0 + 1;

                blocks[0] = (*m_blockVolumeFloat)(x0, y0, z0);
                blocks[1] = (*m_blockVolumeFloat)(x0, y1, z0);
                blocks[2] = (*m_blockVolumeFloat)(x1, y1, z0);
                blocks[3] = (*m_blockVolumeFloat)(x1, y0, z0);
                blocks[4] = (*m_blockVolumeFloat)(x0, y0, z1);
                blocks[5] = (*m_blockVolumeFloat)(x0, y1, z1);
                blocks[6] = (*m_blockVolumeFloat)(x1, y1, z1);
                blocks[7] = (*m_blockVolumeFloat)(x1, y0, z1);

                Vector3Int verts[8];

                verts[0] = std::make_tuple(x0, y0, z0);
                verts[1] = std::make_tuple(x0, y1, z0);
                verts[2] = std::make_tuple(x1, y1, z0);
                verts[3] = std::make_tuple(x1, y0, z0);
                verts[4] = std::make_tuple(x0, y0, z1);
                verts[5] = std::make_tuple(x0, y1, z1);
                verts[6] = std::make_tuple(x1, y1, z1);
                verts[7] = std::make_tuple(x1, y0, z1);

                float minVal = -0.5;

                //float minVal = 1.1;
                int cubeIndex = int(0);
				for(int n=0; n < 8; n++)
                {
            		if(blocks[n] <= minVal) 
                    {
                        cubeIndex |= (1 << n);
                    }
                }

                if(!edgeTable[cubeIndex]) 
                {
                    continue;
                }

                float3 edgeVerts[12];
                EdgeIndex edgeIndices[12];
                if(edgeTable[cubeIndex] & 1)
                {
                    edgeVerts[0] = LinearInterp(verts[0], blocks[0], verts[1], blocks[1], minVal);

                    edgeIndices[0] = makeEdgeIndex(verts[0], verts[1]);
                }
                if(edgeTable[cubeIndex] & 2) 
                {
                    edgeVerts[1] = LinearInterp(verts[1], blocks[1], verts[2], blocks[2], minVal);

                    edgeIndices[1] = makeEdgeIndex(verts[1], verts[2]);
                }
                if(edgeTable[cubeIndex] & 4)
                {
                    edgeVerts[2] = LinearInterp(verts[2], blocks[2], verts[3], blocks[3], minVal);

                    edgeIndices[2] = makeEdgeIndex(verts[2], verts[3]);
                }
                if(edgeTable[cubeIndex] & 8) 
                {
                    edgeVerts[3] = LinearInterp(verts[3], blocks[3], verts[0], blocks[0], minVal);

                    edgeIndices[3] = makeEdgeIndex(verts[3], verts[0]);
                }
                if(edgeTable[cubeIndex] & 16) 
                {
                    edgeVerts[4] = LinearInterp(verts[4], blocks[4], verts[5], blocks[5], minVal);

                    edgeIndices[4] = makeEdgeIndex(verts[4], verts[5]);
                }
                if(edgeTable[cubeIndex] & 32) 
                {
                    edgeVerts[5] = LinearInterp(verts[5], blocks[5], verts[6], blocks[6], minVal);

                    edgeIndices[5] = makeEdgeIndex(verts[5], verts[6]);
                }
                if(edgeTable[cubeIndex] & 64) 
                {
                    edgeVerts[6] = LinearInterp(verts[6], blocks[6], verts[7], blocks[7], minVal);

                    edgeIndices[6] = makeEdgeIndex(verts[6], verts[7]);
                }
                if(edgeTable[cubeIndex] & 128) 
                {
                    edgeVerts[7] = LinearInterp(verts[7], blocks[7], verts[4], blocks[4], minVal);

                    edgeIndices[7] = makeEdgeIndex(verts[7], verts[4]);
                }
                if(edgeTable[cubeIndex] & 256) 
                {
                    edgeVerts[8] = LinearInterp(verts[0], blocks[0], verts[4], blocks[4], minVal);

                    edgeIndices[8] = makeEdgeIndex(verts[0], verts[4]);
                }
                if(edgeTable[cubeIndex] & 512) 
                {
                    edgeVerts[9] = LinearInterp(verts[1], blocks[1], verts[5], blocks[5], minVal);

                    edgeIndices[9] = makeEdgeIndex(verts[1], verts[5]);
                }
                if(edgeTable[cubeIndex] & 1024) 
                {
                    edgeVerts[10] = LinearInterp(verts[2], blocks[2], verts[6], blocks[6], minVal);

                    edgeIndices[10] = makeEdgeIndex(verts[2], verts[6]);
                }
                if(edgeTable[cubeIndex] & 2048) 
                {
                    edgeVerts[11] = LinearInterp(verts[3], blocks[3], verts[7], blocks[7], minVal);

                    edgeIndices[11] = makeEdgeIndex(verts[3], verts[7]);
                }


                for (int n = 0; triTable[cubeIndex][n] != -1; n += 3) 
                {
                    float3 vec1 = edgeVerts[triTable[cubeIndex][n+2]];
                    float3 vec2 = edgeVerts[triTable[cubeIndex][n+1]];
                    float3 vec3 = edgeVerts[triTable[cubeIndex][n]];

                    EdgeIndex idx1 = edgeIndices[triTable[cubeIndex][n+2]];
                    EdgeIndex idx2 = edgeIndices[triTable[cubeIndex][n+1]];
                    EdgeIndex idx3 = edgeIndices[triTable[cubeIndex][n]];

					//Computing normal as cross product of triangle's edges
                    float3 normal = (vec2 - vec1).Cross(vec3 - vec1);

                    uint32_t index = getOrCreateVertex(vec1, *pVertexData, idx1, vertexMap);
                    (*pVertexData)[index].normal += normal;
                  
                    uint32_t index1 = getOrCreateVertex(vec2, *pVertexData, idx2, vertexMap);
                    (*pVertexData)[index1].normal += normal;

                    uint32_t index2 = getOrCreateVertex(vec3, *pVertexData, idx3, vertexMap);
                    (*pVertexData)[index2].normal += normal;
                    
                    pIndexData->push_back(index);
                    pIndexData->push_back(index1);
                    pIndexData->push_back(index2);

				}
            }
        } 
    }

    if(!(*pVertexData).size())
    {
        return;
    }

    for( auto& vertexInf : (*pVertexData))
    {
        vertexInf.normal.Normalize();
    }

    m_indexData = pIndexData;
    m_vertexData = pVertexData;
}

void Chunk::generateMesh(void)
{
       
    if(!m_vertexData || !(*m_vertexData).size())
    {
        return;
    }

    GfxApi::VertexDeclaration decl;
    decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position"));
    decl.add(GfxApi::VertexElement(GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::FLOAT, 3, "vertex_normal"));

    boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer = boost::make_shared<GfxApi::VertexBuffer>(m_vertexData->size(), decl);
    boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer = boost::make_shared<GfxApi::IndexBuffer>(m_indexData->size(), decl, GfxApi::PrimitiveIndexType::Indices32Bit);

    float * vbPtr = reinterpret_cast<float*>(pVertexBuffer->getCpuPtr());
    unsigned int * ibPtr = reinterpret_cast<unsigned int*>(pIndexBuffer->getCpuPtr());

    memcpy(vbPtr, &(*m_vertexData)[0], m_vertexData->size() * 4 * 6);

    pVertexBuffer->allocateGpu();
    pVertexBuffer->updateToGpu();

    memcpy(ibPtr, &(*m_indexData)[0], m_indexData->size() * 4);

    pIndexBuffer->allocateGpu();
    pIndexBuffer->updateToGpu();
   
    auto mesh = boost::make_shared<GfxApi::Mesh>(GfxApi::PrimitiveType::TriangleList);

    mesh->m_vbs.push_back(pVertexBuffer);
    mesh->m_ib.swap(pIndexBuffer);

   
    mesh->m_sp = m_pChunkManager->m_shaders[0];
    mesh->generateVAO();
    mesh->linkShaders();

    m_pMesh = mesh;
}

uint32_t Chunk::getOrCreateVertex(float3& vertex, std::vector<Vertex>& tmpVectorList, EdgeIndex& idx, std::map<EdgeIndex, uint32_t>& vertexMap)
{
    auto& it = vertexMap.find(idx);
    
    if(it != vertexMap.end())
    {
        return it->second;
    }

    Vertex vInfo;
    vInfo.normal = float3(0,0,0);
    vInfo.vertex = vertex;
    int tmpIdx = tmpVectorList.size();

    tmpVectorList.push_back(vInfo);
    
    vertexMap[idx] = tmpIdx;
    return tmpIdx;

}



