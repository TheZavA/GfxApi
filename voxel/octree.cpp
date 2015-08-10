#include "octree.h"
#include <boost/scoped_ptr.hpp>

/*
 * Some of this code is derived from the reference implementation provided by
 * the DC paper's authors. This code is free for any non-commercial use. 
 * The ContourCellProc, ContourEdgeProc, ContourFaceProc and ContourProcessEdge 
 * funcs and the data they use are derived from the reference implementation
 * which can be found here: http://sourceforge.net/projects/dualcontouring/
 */

// ----------------------------------------------------------------------------


void GenerateVertexIndices(int32_t node_index, VertexBuffer& vertexBuffer, std::vector<OctreeNode>& node_list)
{
	if (node_index == -1)
	{
		return;
	}

    OctreeNode* node = &node_list[node_index];

	if (node->type != Node_Leaf)
	{
		for (int i = 0; i < 8; i++)
		{
            if(node->childrenIdx[i] != -1)
			    GenerateVertexIndices(node->childrenIdx[i], vertexBuffer, node_list);
		}
	}

	if (node->type != Node_Internal)
	{
		OctreeDrawInfo* d = &node->drawInfo;
		if (!d)
		{
			printf("Error! Could not add vertex!\n");
			exit(EXIT_FAILURE);
		}

		d->index = vertexBuffer.size();
		vertexBuffer.push_back(MeshVertex(d->position, d->averageNormal));
	}
}

// ----------------------------------------------------------------------------

void ContourProcessEdge(int32_t node_index[4], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list)
{
	int minSize = 1000000;		// arbitrary big number
	int minIndex = 0;
	int indices[4] = { -1, -1, -1, -1 };
	bool flip = false;
	bool signChange[4] = { false, false, false, false };

	for (int i = 0; i < 4; i++)
	{
		const int edge = processEdgeMask[dir][i];
		const int c1 = edgevmap[edge][0];
		const int c2 = edgevmap[edge][1];

		const int m1 = (node_list[node_index[i]].drawInfo.corners >> c1) & 1;
		const int m2 = (node_list[node_index[i]].drawInfo.corners >> c2) & 1;

		if (node_list[node_index[i]].size < minSize)
		{
			minSize = node_list[node_index[i]].size;
			minIndex = i;
			flip = m1 != MATERIAL_AIR; 
		}

		indices[i] = node_list[node_index[i]].drawInfo.index;

		signChange[i] = 
			(m1 == MATERIAL_AIR && m2 != MATERIAL_AIR) ||
			(m1 != MATERIAL_AIR && m2 == MATERIAL_AIR);
	}

	if (signChange[minIndex])
	{
		if (!flip)
		{
			indexBuffer.push_back(indices[0]);
			indexBuffer.push_back(indices[1]);
			indexBuffer.push_back(indices[3]);

			indexBuffer.push_back(indices[0]);
			indexBuffer.push_back(indices[3]);
			indexBuffer.push_back(indices[2]);
		}
		else
		{
			indexBuffer.push_back(indices[0]);
			indexBuffer.push_back(indices[3]);
			indexBuffer.push_back(indices[1]);

			indexBuffer.push_back(indices[0]);
			indexBuffer.push_back(indices[2]);
			indexBuffer.push_back(indices[3]);
		}
	}
}

// ----------------------------------------------------------------------------

void ContourEdgeProc(int32_t node_index[4], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list)
{
	if (node_index[0] == -1 || node_index[1] == -1 || node_index[2] == -1 || node_index[3] == -1)
	{
		return;
	}

	if (node_list[node_index[0]].type != Node_Internal &&
		node_list[node_index[1]].type != Node_Internal &&
		node_list[node_index[2]].type != Node_Internal &&
		node_list[node_index[3]].type != Node_Internal)
	{
		ContourProcessEdge(node_index, dir, indexBuffer, node_list);
	}
	else
	{
		for (int i = 0; i < 2; i++)
		{
			int32_t edgeNodes[4];
			const int c[4] = 
			{
				edgeProcEdgeMask[dir][i][0],
				edgeProcEdgeMask[dir][i][1],
				edgeProcEdgeMask[dir][i][2],
				edgeProcEdgeMask[dir][i][3],
			};

			for (int j = 0; j < 4; j++)
			{
				if (node_list[node_index[j]].type == Node_Leaf || node_list[node_index[j]].type == Node_Psuedo)
				{
					edgeNodes[j] = node_index[j];
				}
				else
				{
					edgeNodes[j] = node_list[node_index[j]].childrenIdx[c[j]];
				}
			}

			ContourEdgeProc(edgeNodes, edgeProcEdgeMask[dir][i][4], indexBuffer, node_list);
		}
	}
}

// ----------------------------------------------------------------------------

void ContourFaceProc(int32_t node_index[2], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list)
{
	if (node_index[0] == -1 || node_index[1] == -1)
	{
		return;
	}

    OctreeNode* node0 = &node_list[node_index[0]];
    OctreeNode* node1 = &node_list[node_index[1]];

	if (node0->type == Node_Internal || 
		node1->type == Node_Internal)
	{
		for (int i = 0; i < 4; i++)
		{
			int32_t faceNodes[2];
			const int c[2] = 
			{
				faceProcFaceMask[dir][i][0], 
				faceProcFaceMask[dir][i][1], 
			};

			for (int j = 0; j < 2; j++)
			{
				if (node_list[node_index[j]].type != Node_Internal)
				{
					faceNodes[j] = node_index[j];
				}
				else
				{
					faceNodes[j] = node_list[node_index[j]].childrenIdx[c[j]];
				}
			}

			ContourFaceProc(faceNodes, faceProcFaceMask[dir][i][2], indexBuffer, node_list);
		}
		
		const int orders[2][4] =
		{
			{ 0, 0, 1, 1 },
			{ 0, 1, 0, 1 },
		};
		for (int i = 0; i < 4; i++)
		{
			int32_t edgeNodes[4];
			const int c[4] =
			{
				faceProcEdgeMask[dir][i][1],
				faceProcEdgeMask[dir][i][2],
				faceProcEdgeMask[dir][i][3],
				faceProcEdgeMask[dir][i][4],
			};

			const int* order = orders[faceProcEdgeMask[dir][i][0]];
			for (int j = 0; j < 4; j++)
			{
				if (node_list[node_index[order[j]]].type == Node_Leaf ||
					node_list[node_index[order[j]]].type == Node_Psuedo)
				{
					edgeNodes[j] = node_index[order[j]];
				}
				else
				{
					edgeNodes[j] = node_list[node_index[order[j]]].childrenIdx[c[j]];
				}
			}

			ContourEdgeProc(edgeNodes, faceProcEdgeMask[dir][i][5], indexBuffer, node_list);
		}
	}
}


void ContourCellProc(int32_t node_index, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list)
{
	if (node_index == -1)
	{
		return;
	}

    OctreeNode* node = &node_list[node_index];

	if (node->type == Node_Internal)
	{
		for (int i = 0; i < 8; i++)
		{
			ContourCellProc(node->childrenIdx[i], indexBuffer, node_list);
		}

		for (int i = 0; i < 12; i++)
		{
			int32_t faceNodes[2];
			const int c[2] = { cellProcFaceMask[i][0], cellProcFaceMask[i][1] };
			
			faceNodes[0] = node->childrenIdx[c[0]];
			faceNodes[1] = node->childrenIdx[c[1]];

			ContourFaceProc(faceNodes, cellProcFaceMask[i][2], indexBuffer, node_list);
		}

		for (int i = 0; i < 6; i++)
		{
			int32_t edgeNodes[4];
			const int c[4] = 
			{
				cellProcEdgeMask[i][0],
				cellProcEdgeMask[i][1],
				cellProcEdgeMask[i][2],
				cellProcEdgeMask[i][3],
			};

			for (int j = 0; j < 4; j++)
			{
				edgeNodes[j] = node->childrenIdx[c[j]];
			}

			ContourEdgeProc(edgeNodes, cellProcEdgeMask[i][4], indexBuffer, node_list);
		}
	}
}


// ----------------------------------------------------------------------------

void GenerateMeshFromOctree(int32_t node_index, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list)
{
	if (node_index == -1)
	{
		return;
	}

	vertexBuffer.clear();
	indexBuffer.clear();

	GenerateVertexIndices(node_index, vertexBuffer, node_list);
	ContourCellProc(node_index, indexBuffer, node_list);
}

// -------------------------------------------------------------------------------
