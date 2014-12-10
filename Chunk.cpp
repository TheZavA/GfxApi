#include "Chunk.h"
#include "ChunkManager.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/random.hpp>
#include <boost/chrono.hpp>

#include "voxel/McTable.h"
#include "voxel/TBorderVolume3d.h"

#include "GfxApi.h"

#include "noisepp/core/Noise.h"
#include "noisepp/utils/NoiseUtils.h"
#include "xmlnoise/xml_noise3d.hpp"
#include "xmlnoise/xml_noise3d_handlers.hpp"
#include "xmlnoise/xml_noise2d.hpp"
#include "xmlnoise/xml_noise2d_handlers.hpp"

#include <hash_map>
#include <tuple>

#include "cubelib\cube.hpp"
#include "qef.h"

#include <CL/cl.h>

#include <math.h>

#include "ocl.h"
    boost::mt19937 rng;
float gen_random_float(float min, float max)
{

    boost::uniform_real<float> u(min, max);
    boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > gen(rng, u);
    return gen();
}




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
    , m_highestDensity(0.0f)
{
    m_workInProgress = boost::make_shared<bool>(false);

}


Chunk::~Chunk(void)
{
    printf("Chunk destructed\n");
}

void Chunk::generateBaseMap(void)
{
    auto& tmpBaseMap = boost::make_shared<TVolume2d<float>>(ChunkManager::CHUNK_SIZE + 8, ChunkManager::CHUNK_SIZE + 8);

    auto& tmpSlopeMap = boost::make_shared<TVolume2d<float>>(ChunkManager::CHUNK_SIZE + 8, ChunkManager::CHUNK_SIZE + 8);

   
    int level = 17 - m_scale * 16;

    boost::scoped_ptr<noisepp::Pipeline2D> pipeline( noisepp::utils::System::createOptimalPipeline2D());
    

    xml_noise2d_t xml_noise2d(*pipeline);
    xml_noise2d_t xml_biome2d(*pipeline);

    register_all_2dhandlers(xml_noise2d.handlers);
    register_all_2dhandlers(xml_biome2d.handlers);

    xml_biome2d.load("zoneDef.xml");
    xml_noise2d.load("baseMap.xml");



    noisepp::PerlinModule baseContinentDef_pe2;
    baseContinentDef_pe2.setSeed (1 + 4);
    baseContinentDef_pe2.setFrequency (0.0004);
    baseContinentDef_pe2.setLacunarity (2.12312);
    baseContinentDef_pe2.setOctaveCount (level);
    baseContinentDef_pe2.setQuality (1);
    baseContinentDef_pe2.setScale(0.7);


    noisepp::ElementID rootid = baseContinentDef_pe2.addToPipeline(pipeline.get());
    noisepp::PipelineElement2D* rootelement = pipeline->getElement(rootid);

    noisepp::Cache *cache = pipeline->createCache();

    size_t index = 0;

    double res = ((m_bounds.MaxX() - m_bounds.MinX())/(ChunkManager::CHUNK_SIZE));

    double worldX = m_bounds.MinX() - res * 4;
    double worldZ = m_bounds.MinZ() - res * 4;


    for(int z = 0; z < ChunkManager::CHUNK_SIZE + 8; z++)
    {
        for(int x = 0; x < ChunkManager::CHUNK_SIZE + 8; x ++, index++)
        {
            double noise =  (1 + (rootelement->getValue((worldX + (double)x * res), (worldZ + (double)z * res), cache)))*0.5  ;

            float& value = (*tmpBaseMap)(x, z);
            value = noise;
           // value = 1;
            /*float& slope = (*tmpSlopeMap)(x, z);
            

            float heightz0 = (1 + rootelement->getValue((worldX + x * res), (worldZ + z * res- res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightz1 = (1 + rootelement->getValue((worldX + x * res), (worldZ + z * res + res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightx0 = (1 + rootelement->getValue((worldX + x * res - res ), (worldZ + z* res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);
            float heightx1 = (1 + rootelement->getValue((worldX + x * res + res ), (worldZ + z* res), cache)) * 0.5 * (ChunkManager::CHUNK_SIZE);

            float3 tt(0, 0, 0); 	
            float3 dv1(0, heightz0 - heightz1, 2);
            float3 dv2(2, heightx0 - heightx1, 0);
            tt = dv1.Cross(dv2);
            tt.Normalize();
            slope = tt.y;*/

        }    
    }
   

    pipeline->freeCache(cache);

    //m_slopeMap = tmpSlopeMap;
    m_baseMap = tmpBaseMap;
    

}


void Chunk::generateTerrain(void)
{
    auto tmpVolumeFloat = boost::make_shared<TBorderVolume3d<float>>(
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    4);
	std::size_t index = 0;





    float yPos = (m_bounds.MinY());
    float worldY = m_bounds.MinY();
    float res = ( (m_bounds.MaxX() - m_bounds.MinX()) / ChunkManager::CHUNK_SIZE );

    //for(int z = tmpVolumeFloat->m_zMin; z < (tmpVolumeFloat->m_zMax); z++)
    //{
    //    for(int y = tmpVolumeFloat->m_yMin; y < (tmpVolumeFloat->m_yMax); y++)
    //    {
    //        for(int x = tmpVolumeFloat->m_xMin; x < (tmpVolumeFloat->m_xMax); x++, index ++)
    //        {
    //            float worldYCurr = (worldY + (((float)y) * res));

    //            float value = (*m_baseMap)(x + tmpVolumeFloat->m_bSize, z + tmpVolumeFloat->m_bSize);
   
    //            float height = value * 2600;

    //            float h0 = height - worldYCurr;

    //            float& value3d = (*tmpVolumeFloat)(x, y, z);
    //            value3d = h0 * 0.0002;


    //        }
    //    }
    //}

   /* for(int k = 0; k< 1; k++)
    {

        auto& tmpVolumeFloat1 = boost::make_shared<TBorderVolume3d<float>>(
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    ChunkManager::CHUNK_SIZE,
                                                                    4);

        float factor = 1.0f/22.0f;
        for(int z = tmpVolumeFloat->m_zMin + 1; z < (tmpVolumeFloat->m_zMax - 1); z++)
	    {
            for(int y = tmpVolumeFloat->m_yMin + 1; y < (tmpVolumeFloat->m_yMax - 1); y++)
	        {
		        for(int x = tmpVolumeFloat->m_xMin + 1; x < (tmpVolumeFloat->m_xMax - 1); x ++, index ++)
		        {
               

                    float value = (*tmpVolumeFloat)(x, y, z);
                    float& targetValue = (*tmpVolumeFloat1)(x, y, z);


                    value += (*tmpVolumeFloat)(x+1, y, z);
                    value += (*tmpVolumeFloat)(x, y, z+1);
                    value += (*tmpVolumeFloat)(x, y+1, z);
                    value += (*tmpVolumeFloat)(x-1, y, z);
                    value += (*tmpVolumeFloat)(x, y, z-1);
                    value += (*tmpVolumeFloat)(x, y-1, z);

                    value += (*tmpVolumeFloat)(x-1, y+1, z);
                    value += (*tmpVolumeFloat)(x+1, y+1, z);
                    value += (*tmpVolumeFloat)(x, y+1, z-1);
                    value += (*tmpVolumeFloat)(x, y+1, z+1);
                    value += (*tmpVolumeFloat)(x+1, y+1, z+1);
                    value += (*tmpVolumeFloat)(x-1, y+1, z-1);
                    value += (*tmpVolumeFloat)(x+1, y+1, z-1);
                    value += (*tmpVolumeFloat)(x-1, y+1, z+1);

                
                    value += (*tmpVolumeFloat)(x-1, y-1, z);
                    value += (*tmpVolumeFloat)(x+1, y-1, z);
                    value += (*tmpVolumeFloat)(x, y-1, z-1);
                    value += (*tmpVolumeFloat)(x, y-1, z+1);
                    value += (*tmpVolumeFloat)(x+1, y-1, z+1);
                    value += (*tmpVolumeFloat)(x-1, y-1, z-1);
                    value += (*tmpVolumeFloat)(x+1, y-1, z-1);
                    value += (*tmpVolumeFloat)(x-1, y-1, z+1);

                    targetValue = value * factor;
                }
            }
        }
        tmpVolumeFloat = tmpVolumeFloat1;
    }*/

   /* boost::scoped_ptr<noisepp::Pipeline3D> pipeline( noisepp::utils::System::createOptimalPipeline3D());
    xml_noise3d_t xml_noise3d(*pipeline);
    register_all_3dhandlers(xml_noise3d.handlers);
    xml_noise3d.load("something.xml");
    assert(xml_noise3d.root);

        
    int level = 17 - m_scale * 16;
    noisepp::PerlinModule baseContinentDef_pe2;
    baseContinentDef_pe2.setSeed (1 + 4);
    baseContinentDef_pe2.setFrequency (0.00005);
    baseContinentDef_pe2.setLacunarity (2.12312);
    baseContinentDef_pe2.setOctaveCount (10);
    baseContinentDef_pe2.setQuality (1);
    baseContinentDef_pe2.setScale(2.1);

    noisepp::ElementID rootid = baseContinentDef_pe2.addToPipeline(pipeline.get());
    noisepp::PipelineElement3D* rootelement = pipeline->getElement(rootid);

    noisepp::Cache *cache = pipeline->createCache();*/
  
    float worldX = m_bounds.MinX();

    float worldZ = m_bounds.MinZ();

    //typedef boost::chrono::high_resolution_clock Clock;
    //auto t1 = Clock::now();

    this->m_pChunkManager->m_ocl->test2((float*)(tmpVolumeFloat->getDataPtr()), (ChunkManager::CHUNK_SIZE) + 8, worldX, worldY, worldZ, res);

 //   auto t2 = Clock::now();

 //   auto t3 = ((t2 - t1)/1000) / 1000;


 //   auto t4 = Clock::now();

 //   for(int z = 0; z < (ChunkManager::CHUNK_SIZE) + 4; z++)
	//{
 //       for(int y = 0; y < (ChunkManager::CHUNK_SIZE) + 4; y++)
	//    {
	//	    for(int x = 0; x < (ChunkManager::CHUNK_SIZE) + 4; x ++, index ++)
	//	    {
 //               float& value = (*tmpVolumeFloat)(x, y, z);

 //              // if(value > 0.0001)
 //              // {
 //                   value =  (value + rootelement->getValue((worldX + (double)x * res), (worldY + (double)y * res), (worldZ + (double)z * res), cache))/2 ;
 //             //  }
 //           
 //           }
 //       }
 //   }

 //   auto t5 = Clock::now();
 //      
 //   auto t6 = ((t5 - t4)/1000) / 1000;

    for(int z = 0; z < (ChunkManager::CHUNK_SIZE) + 4; z++)
	{
        for(int y = 0; y < (ChunkManager::CHUNK_SIZE) + 4; y++)
	    {
		    for(int x = 0; x < (ChunkManager::CHUNK_SIZE) + 4; x ++, index ++)
		    {
                float& value = (*tmpVolumeFloat)(x, y, z);

                if(value > m_highestDensity)
                {
                    m_highestDensity = value;
                }
            
            }
        }
    }



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

size_t toMCEdgeIndex(size_t index)
{
    size_t indexList[] = {0,3,1,2,4,7,5,6};

    return indexList[index];
}
            
int testCount = 0;

void Chunk::generateVertices(void)
{
    testCount = 0;
    auto& pVertexData = boost::make_shared<std::vector<Vertex>>();
    auto& pIndexData = boost::make_shared<std::vector<int>>();

    auto& tmpHermite = boost::make_shared<TBorderVolume3d<Hermite>>(ChunkManager::CHUNK_SIZE, ChunkManager::CHUNK_SIZE, ChunkManager::CHUNK_SIZE, 4);


    std::map<EdgeIndex, uint32_t> vertexMap;


    static int intersections[12][2] = 
    {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
        { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

     float minDensity = 0.001f;

    if(m_highestDensity < minDensity)
    {
        return;
    }

    float normalArray[((ChunkManager::CHUNK_SIZE + 2)*(ChunkManager::CHUNK_SIZE + 2)*(ChunkManager::CHUNK_SIZE + 2))*3];

    auto& normalVol = boost::make_shared<TVolume3d<float3>>((ChunkManager::CHUNK_SIZE + 2), (ChunkManager::CHUNK_SIZE + 2), (ChunkManager::CHUNK_SIZE + 2));
    
    memset(normalVol->getDataPtr(), 0, ((ChunkManager::CHUNK_SIZE + 2)*(ChunkManager::CHUNK_SIZE + 2)*(ChunkManager::CHUNK_SIZE + 2))*3);

    float worldY = m_bounds.MinY();
    float worldX = m_bounds.MinX();
    float worldZ = m_bounds.MinZ();
    float res = ( (m_bounds.MaxX() - m_bounds.MinX()) / ChunkManager::CHUNK_SIZE );

    this->m_pChunkManager->m_ocl->surfaceNormal((float*)(normalVol->getDataPtr()), (ChunkManager::CHUNK_SIZE) + 2, worldX, worldY, worldZ, res);

    std::size_t index = 0;
    for(std::size_t y = 0; y < (ChunkManager::CHUNK_SIZE + 2); y++)
    {
        for(std::size_t z = 0; z < (ChunkManager::CHUNK_SIZE + 2); z++)
        {
            for(std::size_t x = 0; x < (ChunkManager::CHUNK_SIZE + 2); x++)
            {

                float densities[8];

                Hermite& hermite = (*tmpHermite)(x,y,z);
                hermite.cubeIndex= 0;


                float3 sNormal = (*normalVol)(x,y,z);
                
                int x0 = x;
                int y0 = y;
                int z0 = z;
	
                int x1 =  x0 + 1;
                int y1 =  y0 + 1;
                int z1 =  z0 + 1;

                Vector3Int verts[8];

                //for(const cube::corner_t& corner : cube::corner_t::all())
                //{
                //    verts[corner.index()] = Vector3Int(x0 + corner.x(), y0 + corner.y(), z0 + corner.z());
                //    densities[corner.index()] = (*m_blockVolumeFloat)(x0 + corner.x(), y0 + corner.y(), z0 + corner.z());
                //}

                // Need to do proper mapping again and put this in the loop...
                verts[0] = Vector3Int(x0,   y0,     z0);
                verts[1] = Vector3Int(x0+1, y0,     z0);
                verts[2] = Vector3Int(x0+1, y0+1,   z0);
                verts[3] = Vector3Int(x0,   y0+1,   z0);
                verts[4] = Vector3Int(x0,   y0,     z0+1);
                verts[5] = Vector3Int(x0+1, y0,     z0+1);
                verts[6] = Vector3Int(x0+1, y0+1,   z0+1);
                verts[7] = Vector3Int(x0,   y0+1,   z0+1);

                densities[0] = (*m_blockVolumeFloat)(x0,    y0,     z0);
                densities[1] = (*m_blockVolumeFloat)(x0+1,  y0,     z0);
                densities[2] = (*m_blockVolumeFloat)(x0+1,  y0+1,   z0);
                densities[3] = (*m_blockVolumeFloat)(x0,    y0+1,   z0);
                densities[4] = (*m_blockVolumeFloat)(x0,    y0,     z0+1);
                densities[5] = (*m_blockVolumeFloat)(x0+1,  y0,     z0+1);
                densities[6] = (*m_blockVolumeFloat)(x0+1,  y0+1,   z0+1);
                densities[7] = (*m_blockVolumeFloat)(x0,    y0+1,   z0+1);

                int cubeIndex = 0;

				for(int n=0; n < 8; n++)
                {
            		if(densities[n] <= minDensity ) 
                    {
                        cubeIndex |= (1 << n);
                    }
                }

                if(!edgeTable[cubeIndex]) 
                {
                    continue;
                }


                float3 edgeVerts[12];
                float3 edgeNormals[12];
                EdgeIndex edgeIndices[12];
                float3 massPoint(0,0,0);
                int numIntersections = 0;

                for (int i = 0; i < 12; ++i) 
                {
                    if (!(edgeTable[cubeIndex] & (1 << i)))
                    {
                        continue;
                    }

                    int n1 = intersections[i][0];
                    int n2 = intersections[i][1];

                    edgeVerts[i] = LinearInterp(verts[n1], densities[n1], verts[n2], densities[n2], minDensity);
                    //edgeIndices[i] = makeEdgeIndex(verts[n1], verts[n2]);

                    // something clever needs to happen here
                    // edgeNormals[i] = 1.Find neighbours 2.??? 3.PROFIT;
                    // Gonna use MC normals for the time being, see "MC Mesh Generation" down.

                    massPoint += edgeVerts[i];
                    ++numIntersections;
                }

                /// MC Mesh Generation /////////////////////////////////////////
                for (int n = 0; triTable[cubeIndex][n] != -1; n += 3) 
                {
                    float3 vec1 = edgeVerts[triTable[cubeIndex][n]];
                    float3 vec2 = edgeVerts[triTable[cubeIndex][n+1]];
                    float3 vec3 = edgeVerts[triTable[cubeIndex][n+2]];

					//Computing normal as cross product of triangle's edges
                    float3 normal = (vec2 - vec1).Cross(vec3 - vec1);
                    normal.Normalize();

                    
                    // DC NORMALS TEST VALUES     //////////////////////////////
                    edgeNormals[triTable[cubeIndex][n]] = normal;
                    edgeNormals[triTable[cubeIndex][n+1]] = normal;
                    edgeNormals[triTable[cubeIndex][n+2]] = normal;
                    ////////////////////////////////////////////////////////////

				}
                ///////////////////////////////////////////////////////////////

                // DC Hermite data 
                massPoint /= (float)numIntersections;

                float3 newPointNormal(0,0,0);

                double matrix[12][3];
                double vector[12];
                int rows = 0;

                for (int i = 0; i < 12; ++i) 
                {
                    if (! (edgeTable[cubeIndex] & (1 << i)))
                    {
                        continue;
                    }

                    float3 &normal = edgeNormals[i];

                    matrix[rows][0] = normal.x;
                    matrix[rows][1] = normal.y;
                    matrix[rows][2] = normal.z;

                    float3 p = edgeVerts[i] - massPoint;
                    float3 sum = normal * p;
                    vector[rows] = sum.x + sum.y + sum.z;

                    ++rows;

                    newPointNormal += normal;

                }

                float3 newPointV(0,0,0);

                // this function is magic to me...
                QEF::evaluate(matrix, vector, rows, &newPointV);

                newPointV += massPoint;
                newPointNormal.Normalize();
                hermite.point = newPointV;
                hermite.normal = newPointNormal;
                hermite.cubeIndex = cubeIndex;

            }
        } 
    }

    for(std::size_t y = 0; y < (ChunkManager::CHUNK_SIZE - 1) ; y++)
    {
        for(std::size_t z = 0; z < (ChunkManager::CHUNK_SIZE - 1) ; z++)
        {
            for(std::size_t x = 0; x < (ChunkManager::CHUNK_SIZE - 1) ; x++)
            {
                Hermite hermite[4];
                hermite[0] = (*tmpHermite)(x,y,z);
                int cube0_edgeInfo = edgeTable[hermite[0].cubeIndex];
                int flip_if_nonzero = 0;

                for (int i = 0; i < 3; ++i) 
                {

                    if (i == 0  && cube0_edgeInfo & (1 << 10)) 
                    {
                        hermite[1] = (*tmpHermite)(x+1,     y,      z);
                        hermite[2] = (*tmpHermite)(x+1,     y+1,    z);
                        hermite[3] = (*tmpHermite)(x,       y+1,    z);

                        flip_if_nonzero = (hermite[0].cubeIndex & (1 << 6));
                    } 
                    else if (i == 1 && cube0_edgeInfo & (1 << 6)) 
                    {
                        hermite[1] = (*tmpHermite)(x,       y,      z+1);
                        hermite[2] = (*tmpHermite)(x,       y+1,    z+1);
                        hermite[3] = (*tmpHermite)(x,       y+1,    z);

                        flip_if_nonzero = (hermite[0].cubeIndex & (1 << 7));
                    } 
                    else if (i == 2 && cube0_edgeInfo & (1 << 5)) 
                    {
                        hermite[1] = (*tmpHermite)(x+1,     y,      z);
                        hermite[2] = (*tmpHermite)(x+1,     y,      z+1);
                        hermite[3] = (*tmpHermite)(x,       y,      z+1);

                        flip_if_nonzero = (hermite[0].cubeIndex & (1 << 5));
                    } 
                    else
                    {
                        continue;
                    }
                
                    // create triangles (cube0,cube2,cube1)
                    //              and (cube0,cube3,cube2)
                    // flipping last two vertices if necessary
                    float3 p0 = hermite[0].point;

                    for (int j = 1; j < 3; ++j) 
                    {
                        int ja, jb;
                        if (flip_if_nonzero) 
                        {
                            ja = j + 0;
                            jb = j + 1;
                        }
                        else 
                        {
                            ja = j + 1;
                            jb = j + 0;
                        }

                        float3 p1 = hermite[ja].point;
                        float3 p2 = hermite[jb].point;

                        EdgeIndex idx1(Vector3Int((int)(p2.x*10000),(int)(p2.y*10000),(int)(p2.z*10000)), Vector3Int(0,0,0));
                        EdgeIndex idx2(Vector3Int((int)(p1.x*10000),(int)(p1.y*10000),(int)(p1.z*10000)), Vector3Int(0,0,0));
                        EdgeIndex idx3(Vector3Int((int)(p0.x*10000),(int)(p0.y*10000),(int)(p0.z*10000)), Vector3Int(0,0,0));
                        

                        // add face normals from border to excisting vertices
                        if(x > ChunkManager::CHUNK_SIZE  ||
                            y > ChunkManager::CHUNK_SIZE || 
                            z > ChunkManager::CHUNK_SIZE )
                        {
                            auto& it = vertexMap.find(idx1);
                            if(it != vertexMap.end())
                            {
                                (*pVertexData)[it->second].normal += hermite[0].normal;
                            }

                            auto& it1 = vertexMap.find(idx2);
                            if(it1 != vertexMap.end())
                            {
                                (*pVertexData)[it1->second].normal += hermite[0].normal;
                            }

                            auto& it2 = vertexMap.find(idx3);
                            if(it2 != vertexMap.end())
                            {
                                (*pVertexData)[it2->second].normal += hermite[0].normal;
                            }
                        }
                        else
                        {

                            uint32_t index1 = getOrCreateVertex(p2, *pVertexData, idx1, vertexMap);
                            (*pVertexData)[index1].normal += hermite[0].normal;
                            pIndexData->push_back(index1);

                            uint32_t index2 = getOrCreateVertex(p1, *pVertexData, idx2, vertexMap);
                            (*pVertexData)[index2].normal += hermite[0].normal;
                            pIndexData->push_back(index2);

                            uint32_t index3 = getOrCreateVertex(p0, *pVertexData, idx3, vertexMap);
                            (*pVertexData)[index3].normal += hermite[0].normal;
                            pIndexData->push_back(index3);
                        }

                    }
                }
            }

        }

    }

    //for(std::size_t y = 0; y < (ChunkManager::CHUNK_SIZE + 2) ; y++)
    //{
    //    for(std::size_t z = 0; z < (ChunkManager::CHUNK_SIZE + 2) ; z++)
    //    {
    //        for(std::size_t x = 0; x < (ChunkManager::CHUNK_SIZE + 2) ; x++)
    //        {
    //            Hermite hermite[4];
    //            hermite[0] = (*tmpHermite)(x,y,z);
    //            int cube0_edgeInfo = edgeTable[hermite[0].cubeIndex];
    //            int flip_if_nonzero = 0;

    //            for (int i = 0; i < 3; ++i) 
    //            {

    //                if (i == 0  && cube0_edgeInfo & (1 << 10)) 
    //                {
    //                    hermite[1] = (*tmpHermite)(x+1,     y,      z);
    //                    hermite[2] = (*tmpHermite)(x+1,     y+1,    z);
    //                    hermite[3] = (*tmpHermite)(x,       y+1,    z);

    //                    flip_if_nonzero = (hermite[0].cubeIndex & (1 << 6));
    //                } 
    //                else if (i == 1 && cube0_edgeInfo & (1 << 6)) 
    //                {
    //                    hermite[1] = (*tmpHermite)(x,       y,      z+1);
    //                    hermite[2] = (*tmpHermite)(x,       y+1,    z+1);
    //                    hermite[3] = (*tmpHermite)(x,       y+1,    z);

    //                    flip_if_nonzero = (hermite[0].cubeIndex & (1 << 7));
    //                } 
    //                else if (i == 2 && cube0_edgeInfo & (1 << 5)) 
    //                {
    //                    hermite[1] = (*tmpHermite)(x+1,     y,      z);
    //                    hermite[2] = (*tmpHermite)(x+1,     y,      z+1);
    //                    hermite[3] = (*tmpHermite)(x,       y,      z+1);

    //                    flip_if_nonzero = (hermite[0].cubeIndex & (1 << 5));
    //                } 
    //                else
    //                {
    //                    continue;
    //                }

    //                float3 p0 = hermite[0].point;

    //                for (int j = 1; j < 3; ++j) 
    //                {
    //                    int ja, jb;
    //                    if (flip_if_nonzero) 
    //                    {
    //                        ja = j + 0;
    //                        jb = j + 1;
    //                    }
    //                    else 
    //                    {
    //                        ja = j + 1;
    //                        jb = j + 0;
    //                    }

    //                    float3 p1 = hermite[ja].point;
    //                    float3 p2 = hermite[jb].point;

    //                    EdgeIndex idx1(Vector3Int((int)(p2.x*10000),(int)(p2.y*10000),(int)(p2.z*10000)), Vector3Int(0,0,0));
    //                    EdgeIndex idx2(Vector3Int((int)(p1.x*10000),(int)(p1.y*10000),(int)(p1.z*10000)), Vector3Int(0,0,0));
    //                    EdgeIndex idx3(Vector3Int((int)(p0.x*10000),(int)(p0.y*10000),(int)(p0.z*10000)), Vector3Int(0,0,0));


    //                    // add face normals from border to excisting vertices
    //                    if(x == 0  ||
    //                        y == 0 || 
    //                        z == 0 )
    //                    {
    //                        auto& it = vertexMap.find(idx1);
    //                        if(it != vertexMap.end())
    //                        {
    //                            (*pVertexData)[it->second].normal += hermite[0].normal;
    //                        }

    //                        auto& it1 = vertexMap.find(idx2);
    //                        if(it1 != vertexMap.end())
    //                        {
    //                            (*pVertexData)[it1->second].normal += hermite[0].normal;
    //                        }

    //                        auto& it2 = vertexMap.find(idx3);
    //                        if(it2 != vertexMap.end())
    //                        {
    //                            (*pVertexData)[it2->second].normal += hermite[0].normal;
    //                        }
    //                    }
    //                }
    //            }
    //        }

     //   }

   // }

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



