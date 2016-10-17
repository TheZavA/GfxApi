#ifndef		HAS_OCTREE_H_BEEN_INCLUDED
#define		HAS_OCTREE_H_BEEN_INCLUDED

#include "qef.h"
#include "../MainClass.h"

// ----------------------------------------------------------------------------


const int MATERIAL_AIR = 0;
const int MATERIAL_SOLID = 1;


// ----------------------------------------------------------------------------
// data from the original DC impl, drives the contouring process

const int edgevmap[12][2] =
{
   {0,4},{1,5},{2,6},{3,7},	// x-axis 
   {0,2},{1,3},{4,6},{5,7},	// y-axis
   {0,1},{2,3},{4,5},{6,7}		// z-axis
};

const int vertMap[8][3] = { { 0,0,0 },{ 0,0,1 },{ 0,1,0 },{ 0,1,1 },{ 1,0,0 },{ 1,0,1 },{ 1,1,0 },{ 1,1,1 } };

const int cellProcFaceMask[12][3] = { {0,4,0},{1,5,0},{2,6,0},{3,7,0},{0,2,1},{4,6,1},{1,3,1},{5,7,1},{0,1,2},{2,3,2},{4,5,2},{6,7,2} };
const int cellProcEdgeMask[6][5] = { {0,1,2,3,0},{4,5,6,7,0},{0,4,1,5,1},{2,6,3,7,1},{0,2,4,6,2},{1,3,5,7,2} };

const int faceProcFaceMask[3][4][3] = {
   {{4,0,0},{5,1,0},{6,2,0},{7,3,0}},
   {{2,0,1},{6,4,1},{3,1,1},{7,5,1}},
   {{1,0,2},{3,2,2},{5,4,2},{7,6,2}}
};

const int edgeToIndex[12] = { 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0 };

const int edgeDuplicateMask[3][4][3] = {
    {{0 ,0, 0}, {0,  0, -1}, {0, -1, 0},  { 0,  -1,  -1}},
    {{0 ,0, 0}, {0,  0, -1}, {-1, 0, 0},  {-1,   0,  -1}},
    {{0 ,0, 0}, {0, -1,  0}, {-1, 0, 0},  {-1,  -1,   0}}
};

const int edgeDuplicateIndex[3][4] = {
   { 0, 1, 2, 3 },
   { 4, 5, 6, 7 },
   { 8, 9, 10, 11 }
};

const int faceProcEdgeMask[3][4][6] = {
   {{1,4,0,5,1,1},{1,6,2,7,3,1},{0,4,6,0,2,2},{0,5,7,1,3,2}},
   {{0,2,3,0,1,0},{0,6,7,4,5,0},{1,2,0,6,4,2},{1,3,1,7,5,2}},
   {{1,1,0,3,2,0},{1,5,4,7,6,0},{0,1,5,0,4,1},{0,3,7,2,6,1}}
};

const int edgeProcEdgeMask[3][2][5] = {
   {{3,2,1,0,0},{7,6,5,4,0}},
   {{5,1,4,0,1},{7,3,6,2,1}},
   {{6,4,2,0,2},{7,5,3,1,2}},
};

const int dirCell2[3][4][3] = {
    { { 0,-1,-1 },{ 0,-1,0 },{ 0,0,-1 },{ 0,0,0 } },
    { { -1,0,-1 },{ 0,0,-1 },{ -1,0,0 },{ 0,0,0 } },
    { { -1,-1,0 },{ -1,0,0 },{ 0,-1,0 },{ 0,0,0 } } };

const int processEdgeMask[3][4] = { {3,2,1,0},{7,5,6,4},{11,10,9,8} };

enum OctreeNodeType : uint8_t
{
   Node_None,
   Node_Internal,
   Node_Psuedo,
   Node_Leaf,
};

struct VertexTreeNode
{
   QefData qef_data;
   float3 position;
   bool collapsible;
   VertexTreeNode* pParent;

};

struct MeshVertex
{
   MeshVertex( const float3& _xyz, const float3& _normal, const int& _material )
      : m_xyz( _xyz )
      , m_normal( _normal )
      , m_material( _material )
   {
   }

   float3 m_xyz;
   float3 m_normal;
   int m_material;
};

typedef std::vector<MeshVertex> VertexBuffer;
typedef std::vector<unsigned int> IndexBuffer;

class HashMap;


// ----------------------------------------------------------------------------

class OctreeNode
{
public:

   OctreeNode()
      : type( Node_None )
      , minx( 0 )
      , miny( 0 )
      , minz( 0 )
      , size( 0 )
      , index( -1 )
      , material( 0 )
   {
      averageNormal = float3( 0, 0, 0 );

      for( int i = 0; i < 8; i++ )
      {
         childrenIdx[i] = -1;
      }
   }

   OctreeNode( const OctreeNodeType _type )
      : type( _type )
      , minx( 0 )
      , miny( 0 )
      , minz( 0 )
      , size( 0 )
      , index( -1 )
      , material( 0 )
   {
      averageNormal = float3( 0, 0, 0 );

      for( int i = 0; i < 8; i++ )
      {
         childrenIdx[i] = -1;
      }
   }

   OctreeNodeType	type;
   uint8_t        minx;
   uint8_t        miny;
   uint8_t        minz;
   uint8_t        size;
   int32_t        childrenIdx[8];
   int32_t			index;
   uint8_t			corners;
   float3			position;
   float3			averageNormal;
   uint8_t			material;
   QefData        qef;

};

// ----------------------------------------------------------------------------

void GenerateMeshFromOctree( OctreeNode* node, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer );
void GenerateMeshFromOctree( int32_t node_index, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );

void GenerateVertexIndices( int32_t node_index, VertexBuffer& vertexBuffer, std::vector<OctreeNode>& node_list );
void CellProc( int32_t node_index, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );

uint32_t createOctree( std::vector<uint32_t>& leaf_indices, std::vector<OctreeNode>& node_list, const float threshold, bool simplify, int32_t& node_counter );
uint32_t simplifyOctree( uint32_t node_index, std::vector<OctreeNode>& node_list, float threshold );


// intersection free test -----------------------------------------------------
void cellProcContourNoInter2( int32_t node_index, int st[3], int len, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );
void faceProcContourNoInter2( int32_t node_index[2], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );
void edgeProcContourNoInter2( int32_t node_index[4], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );

void processEdgeNoInter2( int32_t node_index[4], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list );

int testFace( int st[3], int len, int dir, float v1[3], float v2[3] );
int testEdge( int st[3], int len, int dir, int32_t node_index[4], float v[4][3], std::vector<OctreeNode>& node_list );
void makeEdgeVertex( int st[3], int len, int dir, float mp[4][3], float v[3] );
// ----------------------------------------------------------------------------

#endif	// HAS_OCTREE_H_BEEN_INCLUDED

