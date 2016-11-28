#ifndef _NODECHUNK_H
#define _NODECHUNK_H

#include "../MainClass.h"

#include "qef.h"

#include "TVolume2d.h"
#include "TVolume3d.h"
#include "TBorderVolume3d.h"
#include "TOctree.h"
#include "common.h"

#include <unordered_map>
#include <atomic>

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

   boost::shared_ptr< TOctree< boost::shared_ptr< NodeChunk > > > m_pTree;
   boost::shared_ptr< std::vector<cl_block_info_t> > m_compactedBlocks;
   boost::shared_ptr< std::vector<uint8_t> > m_compactedLocalAO;

   NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes = false );

   ~NodeChunk();

   void generate( float threshold );

   void createMesh();

   void generateDensities();
   void generateVertices();

   void classifyBlocks();

   boost::shared_ptr< GfxApi::Mesh > getMeshPtr();


};

#endif