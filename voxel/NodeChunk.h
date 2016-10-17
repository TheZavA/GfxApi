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

   AABB m_chunk_bounds;

   uint32_t m_octree_root_index;

   boost::shared_ptr < TVolume3d<cell_t*> > m_pCellBuffer;

   boost::shared_ptr< std::vector< cl_int4_t > > m_compactCorners;
   boost::shared_ptr< std::vector< edge_t > > m_edgesCompact;
   boost::shared_ptr< std::vector< cell_t > > m_zeroCrossCompact;

   boost::shared_ptr< bool > m_workInProgress;


   boost::shared_ptr< IndexBuffer > m_pIndices;
   boost::shared_ptr< VertexBuffer > m_pVertices;
   boost::shared_ptr< GfxApi::Mesh > m_pMesh;

   std::vector<Vertex> m_vertices;
   std::vector<int> m_indices;
   std::vector<int> m_tri_count;

   boost::shared_ptr< TOctree< boost::shared_ptr< NodeChunk > > > m_pTree;

   int32_t m_nodeIdxCurr;

   boost::shared_ptr< std::vector< OctreeNode > > m_pOctreeNodes;
   boost::shared_ptr< std::vector< uint32_t > > m_pOctreeNodeIndices;

   NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes = false );

   ~NodeChunk();

   std::vector<uint32_t> createLeafNodes();

   void ConstructBase( OctreeNodeMdc * pNode, int size, std::vector< OctreeNodeMdc >& nodeList );
   bool ConstructNodes( OctreeNodeMdc * pNode, int& n_index, std::vector<  OctreeNodeMdc >& nodeList );
   bool ConstructLeaf( OctreeNodeMdc * pNode, int& index );

   void buildTree( const int size, const float threshold );

   void generate( float threshold );

   void createVertices();
   void createMesh();

   void classifyEdges();
   void generateZeroCross();

   void generateDensities();

   void generateFullEdges();

   boost::shared_ptr< GfxApi::Mesh > getMeshPtr();

   std::vector<VertexPositionNormal> m_mdcVertices;

};

#endif