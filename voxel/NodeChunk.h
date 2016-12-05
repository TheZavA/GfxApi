#ifndef _NODECHUNK_H
#define _NODECHUNK_H

#include "../MainClass.h"

#include "TVolume2d.h"
#include "TVolume3d.h"
#include "TBorderVolume3d.h"
#include "TOctree.h"
#include "common.h"

#include <unordered_map>
#include <atomic>
#include <map>

class ChunkManager;


class NodeChunk
{
public:
   ChunkManager* m_pChunkManager;

   float m_scale;

   bool m_bHasNodes;

   std::atomic< bool > m_bGenerated;

   boost::shared_ptr< bool > m_workInProgress;

   AABB m_chunk_bounds;

   boost::shared_ptr< GfxApi::Mesh > m_pMesh;

   std::vector< uint16_t > m_indices;

   std::vector< cl_vertex_t > m_vertices;
   
   std::unordered_map< uint32_t, int16_t > m_vertexToIndex;

   boost::shared_ptr< TOctree< boost::shared_ptr< NodeChunk > > > m_pTree;
   boost::shared_ptr< std::vector<cl_block_info_t> > m_compactedBlocks;

   NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes = false );

   ~NodeChunk();

   void createMesh();

   void generateDensities();
   void generateVertices();
   void generateLocalAO();

   void classifyBlocks();

   uint16_t getOrCreateVertex( uint8_t x, uint8_t y, uint8_t z );

   boost::shared_ptr< GfxApi::Mesh > getMeshPtr();


};

#endif