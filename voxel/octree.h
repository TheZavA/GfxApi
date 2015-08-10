#ifndef		HAS_OCTREE_H_BEEN_INCLUDED
#define		HAS_OCTREE_H_BEEN_INCLUDED

#include "qef.h"
//#include "mesh.h"
#include "../MainClass.h"

// ----------------------------------------------------------------------------


const int MATERIAL_AIR = 0;
const int MATERIAL_SOLID = 1;

// ----------------------------------------------------------------------------

const float3 CHILD_MIN_OFFSETS[] =
{
	// needs to match the vertMap from Dual Contouring impl
	float3( 0, 0, 0 ),
	float3( 0, 0, 1 ),
	float3( 0, 1, 0 ),
	float3( 0, 1, 1 ),
	float3( 1, 0, 0 ),
	float3( 1, 0, 1 ),
	float3( 1, 1, 0 ),
	float3( 1, 1, 1 ),
};


// ----------------------------------------------------------------------------
// data from the original DC impl, drives the contouring process

const int edgevmap[12][2] = 
{
	{0,4},{1,5},{2,6},{3,7},	// x-axis 
	{0,2},{1,3},{4,6},{5,7},	// y-axis
	{0,1},{2,3},{4,5},{6,7}		// z-axis
};

const int edgemask[3] = { 5, 3, 6 } ;

const int vertMap[8][3] = 
{
	{0,0,0},
	{0,0,1},
	{0,1,0},
	{0,1,1},
	{1,0,0},
	{1,0,1},
	{1,1,0},
	{1,1,1}
};

const int faceMap[6][4] = {{4, 8, 5, 9}, {6, 10, 7, 11},{0, 8, 1, 10},{2, 9, 3, 11},{0, 4, 2, 6},{1, 5, 3, 7}} ;
const int cellProcFaceMask[12][3] = {{0,4,0},{1,5,0},{2,6,0},{3,7,0},{0,2,1},{4,6,1},{1,3,1},{5,7,1},{0,1,2},{2,3,2},{4,5,2},{6,7,2}} ;
const int cellProcEdgeMask[6][5] = {{0,1,2,3,0},{4,5,6,7,0},{0,4,1,5,1},{2,6,3,7,1},{0,2,4,6,2},{1,3,5,7,2}} ;

const int faceProcFaceMask[3][4][3] = {
	{{4,0,0},{5,1,0},{6,2,0},{7,3,0}},
	{{2,0,1},{6,4,1},{3,1,1},{7,5,1}},
	{{1,0,2},{3,2,2},{5,4,2},{7,6,2}}
} ;

const int edgeToIndex[12] = { 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0 };

const int edgeDuplicateMask[3][4][3] = {
    {{0 ,0, 0}, {0,  0, -1}, {0, -1, 0},  { 0,  -1,  -1}},
	{{0 ,0, 0}, {0,  0, -1}, {-1, 0, 0},  {-1,   0,  -1}},
	{{0 ,0, 0}, {0, -1,  0}, {-1, 0, 0},  {-1,  -1,   0}}
};

const int edgeDuplicateCornerMask[3][4][2] = {
    {{0,4}, {1,5}, {2,6},  {3,7}},
    {{0,2}, {1,3}, {4,6},  {5,7}},
	{{0,1}, {2,3}, {4,5},  {6,7}}
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

const int processEdgeMask[3][4] = {{3,2,1,0},{7,5,6,4},{11,10,9,8}} ;

enum OctreeNodeType : uint8_t
{
	Node_None,
	Node_Internal,
	Node_Psuedo,
	Node_Leaf,
};

struct MeshVertex
{
	MeshVertex(const float3& _xyz, const float3& _normal)
		: m_xyz(_xyz)
		, m_normal(_normal)
	{
	}

	float3 m_xyz;
    float3 m_normal;
};

typedef std::vector<MeshVertex> VertexBuffer;
typedef std::vector<unsigned int> IndexBuffer;

// ----------------------------------------------------------------------------

struct OctreeDrawInfo 
{
	OctreeDrawInfo()
		: index(-1)
		, corners(0)
	{
	}

	int32_t			index;
	int				corners;
	float3			position;
	float3			averageNormal;
	QefData         qef;
};

// ----------------------------------------------------------------------------

class OctreeNode
{
public:

	OctreeNode()
		: type(Node_None)
		, minx(0)
        , miny(0)
        , minz(0)
		, size(0)
		//, drawInfo(nullptr)
	{
        drawInfo.averageNormal = float3(0,0,0);
        memset(&drawInfo, 0, sizeof(OctreeDrawInfo));
        
		for (int i = 0; i < 8; i++)
		{
			//children[i] = nullptr;
            childrenIdx[i] = -1;
		}
	}

	OctreeNode(const OctreeNodeType _type)
		: type(_type)
		, minx(0)
        , miny(0)
        , minz(0)
		, size(0)
		//, drawInfo(nullptr)
	{
        drawInfo.averageNormal = float3(0,0,0);
        memset(&drawInfo, 0, sizeof(OctreeDrawInfo));
		for (int i = 0; i < 8; i++)
		{
			//children[i] = nullptr;
            childrenIdx[i] = -1;
		}
	}
    
	OctreeNodeType	type;
    uint16_t minx;
    uint16_t miny;
    uint16_t minz;
	uint16_t size;
	//OctreeNode*		children[8];
    int32_t         childrenIdx[8];
	OctreeDrawInfo	drawInfo;
};

// ----------------------------------------------------------------------------

OctreeNode* BuildOctree(const float3& min, const int size, const float threshold);
void DestroyOctree(OctreeNode* node);
void GenerateMeshFromOctree(OctreeNode* node, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer);
void GenerateMeshFromOctree(int32_t node_index, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list);

// ----------------------------------------------------------------------------

#endif	// HAS_OCTREE_H_BEEN_INCLUDED

