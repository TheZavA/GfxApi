#ifndef _NODECHUNK_H
#define _NODECHUNK_H

#include "../MainClass.h"
#include "octree.h"

#include "qef.h"


#include "TVolume2d.h"
#include "TVolume3d.h"
#include "TBorderVolume3d.h"
#include "TOctree.h"
#include "common.h"

#include "octreemdc.h"

#include <unordered_map>
#include <atomic>

class ChunkManager;

class NodeChunk
{
public:
   ChunkManager* m_pChunkManager;

   float m_scale;

   bool m_bHasNodes;

   std::atomic< bool > m_bClassified;

   std::atomic< bool > m_bGenerated;

   boost::shared_ptr< bool > m_workInProgress;

   AABB m_chunk_bounds;

   boost::shared_ptr < TVolume3d<cell_t*> > m_pCellBuffer;

   boost::shared_ptr< std::vector< cl_int4_t > > m_compactCorners;
   boost::shared_ptr< std::vector< edge_t > > m_edgesCompact;
   boost::shared_ptr< std::vector< cell_t > > m_zeroCrossCompact;

   boost::shared_ptr< GfxApi::Mesh > m_pMesh;

   std::vector<unsigned int> m_indices;
   std::vector<int> m_tri_count;

   std::vector<VertexPositionNormal> m_mdcVertices;

   boost::shared_ptr< TOctree< boost::shared_ptr< NodeChunk > > > m_pTree;

   int32_t m_nodeIdxCurr;

   NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes = false );

   ~NodeChunk();

   void ConstructBase( OctreeNodeMdc * pNode, int size, std::vector< OctreeNodeMdc >& nodeList );
   bool ConstructNodes( OctreeNodeMdc * pNode, int& n_index, std::vector<  OctreeNodeMdc >& nodeList );
   bool ConstructLeaf( OctreeNodeMdc * pNode, int& index );

   void generate( float threshold );

   void createMesh();

   void classifyEdges();
   void generateZeroCross();

   void generateDensities();

   void generateFullEdges();

   boost::shared_ptr< GfxApi::Mesh > getMeshPtr();


};

#endif