#pragma once
#include "../MainClass.h"


struct AABBTreeInfo
{
	//heuristics 
	uint8_t max_tree_depth;		//max depth the tree can reach 
								//min number of vertices each leaf can store 

	uint8_t curr_max_depth;

	uint32_t min_vertices;

	uint32_t left_children;
	uint32_t right_children;

	//ensures that an AABB is never generated that 
	//is over min_vertices. Normally, this algorithm 
	//is not required because the best axis 
	//algorithm normally produces a perfectly balanced tree. 
	bool m_bPrune;
};


class AABBNode
{
public:
	AABB m_box;
	AABBNode *children[2];
	std::vector<float3> m_pVerts;	//vertices stored within this node 
	unsigned int m_nNumVerts; //num of vertices stored (3->n) 

	AABBNode(std::vector<float3> vertexList, AABBTreeInfo &treeInfo, uint8_t depth) 
	{
		m_nNumVerts = 0;
		children[0] = nullptr;
		children[1] = nullptr;
		buildTree(vertexList, treeInfo, depth);
	}

	AABB getBoundsToVertices(std::vector<float3> &vertexList);

	int findBestAxis(std::vector<float3> &vertexList);
	void buildTree(std::vector<float3> vertexList, AABBTreeInfo &treeInfo, uint8_t depth);
};


typedef unsigned int udword;

//Defines the behavior of the AABB tree 


struct AABBTree
{
	AABBNode *root;
};

AABB AABBNode::getBoundsToVertices(std::vector<float3> &vertexList)
{
	float3 min = float3(0.0f);
	float3 max = float3(0.0f);
	for (auto& vertex : vertexList)
	{
		if (vertex.x < min.x && vertex.y < min.y && vertex.z < min.z)
		{
			min = vertex;
		}
		else if(vertex.x > min.x && vertex.y > min.y && vertex.z > min.z)
		{
			max = vertex;
		}
	}

	return AABB(min, max);
}

int AABBNode::findBestAxis(std::vector<float3> &vertexList)
{
	size_t vertexNum = vertexList.size();

	//divide this box into two boxes - pick a better axis 
	int iAxis = 0;
	int iAxisResult[3]; //stores how close end result is, the lower the better  

	float3 center = m_box.CenterPoint();

	for (iAxis = 0; iAxis<3; iAxis++)
	{
		int left = 0, right = 0;
		float3 v[3];
		int count = 0;
		for (udword i = 0; i<vertexNum; i++)
		{
			v[count] = vertexList[i];
			if (count == 2)
			{
				float faceCenter[3];
				faceCenter[0] = (v[0].x + v[1].x + v[2].x) / 3.0f;
				faceCenter[1] = (v[0].y + v[1].y + v[2].y) / 3.0f;
				faceCenter[2] = (v[0].z + v[1].z + v[2].z) / 3.0f;

				if (faceCenter[iAxis] <= center[iAxis])
				{
					left++;
				}
				else
				{
					right++;
				}

				count = 0;
			}
			else
				count++;
		} // vertices  

		iAxisResult[iAxis] = abs(left - right);
	} //axis  

	int index = 0;
	int result = iAxisResult[0];
	for (int i = 1; i<3; i++)
	{
		if (iAxisResult[i] < result)
		{
			result = iAxisResult[i];
			index = i;
		}
	}

	//Log("result: %d\n", iAxisResult[index]);  

	return index;
}

void AABBNode::buildTree(std::vector<float3> vertexList, AABBTreeInfo &treeInfo, uint8_t depth)
{
	int vertexNum = (int) vertexList.size();

	// Build the node bounding box based from the triangle list 
	//WBox Box(&vertexList[0], vertexNum);

	m_box = getBoundsToVertices(vertexList);

	//debug box bounds 
	//SetBounds(Box);

	if (depth + 1 > treeInfo.curr_max_depth)
		treeInfo.curr_max_depth = depth + 1;

	bool bMakeChildren = false;

	if (vertexNum > treeInfo.min_vertices
		&& depth<treeInfo.max_tree_depth)
	{
		bMakeChildren = true;
	}

	if (bMakeChildren)
	{
		// Find the longest axii of the node's box 
		//int iAxis=Box.GetLongestAxis(); 
		int iAxis = findBestAxis(vertexList);
		//Log("Longest axis: %d\n", iAxis);  

		//Get the Arrays for min, max dimensions 
		//float *min = &Box.minPoint.x;
		//float *max = &Box.maxPoint.x;

		//get center of the box for longest axis 
		//float fSplit = ( max[ iAxis ] + min[ iAxis ] ) / 2.f ; 
		float3 center = m_box.CenterPoint();
		//Log("split: %f\n", fSplit);  

		int count = 0;
		float3 v[3];
		std::vector<float3> leftSide;
		std::vector<float3> rightSide;

		udword leftCount = 0, rightCount = 0; //debug  

											  //Log("verts: %d\n", vertexNum);  

											  //btw, things that could go wrong- if the mesh doesn't 
											  //send over the triangles correctly, then you might see 
											  //huge boxes that are misaligned (bad leaves). 
											  //things to check is making sure the vertex buffer is 
											  //correctly aligned along the adjancey buffers, etc 
		for (int i = 0; i<vertexNum; i++)
		{
			v[count] = vertexList[i];

			if (count == 2)
			{
				float faceCenter[3];
				faceCenter[0] = (v[0].x + v[1].x + v[2].x) / 3.0f;
				faceCenter[1] = (v[0].y + v[1].y + v[2].y) / 3.0f;
				faceCenter[2] = (v[0].z + v[1].z + v[2].z) / 3.0f;
				//WVector faceCenter(x,y,z);  

				if (faceCenter[iAxis] <= center[iAxis]) //fSplit 
				{
					//Store the verts to the left. 
					leftSide.push_back(v[0]);
					leftSide.push_back(v[1]);
					leftSide.push_back(v[2]);

					leftCount++;
				}
				else
				{
					//Store the verts to the right. 
					rightSide.push_back(v[0]);
					rightSide.push_back(v[1]);
					rightSide.push_back(v[2]);

					rightCount++;
				}

				count = 0;
			}
			else
				count++;
		}

		if (treeInfo.m_bPrune && (leftCount == 0 || rightCount == 0)
			)
		{
			//okay, now it's time to cheat. we couldn't use 
			//the best axis to split the 
			//box so now we'll resort to brute force hacks.... 
			leftSide.clear();
			rightSide.clear();

			int leftMaxIndex = vertexNum / 2; //left side  

			int count = 0;
			float3 v[3];
			for (int i = 0; i < vertexNum; i++)
			{
				v[count] = vertexList[i];
				if (count == 2)
				{
					if (i<leftMaxIndex)
					{
						//left node 
						leftSide.push_back(v[0]);
						leftSide.push_back(v[1]);
						leftSide.push_back(v[2]);
					}
					else
					{
						rightSide.push_back(v[0]);
						rightSide.push_back(v[1]);
						rightSide.push_back(v[2]);
					}

					count = 0;
				}
				else
					count++;
			}
		}

		if (leftSide.size() > 0 && rightSide.size() > 0)
		{
			assert(leftSide.size() % 3 == 0);
			assert(rightSide.size() % 3 == 0);

			//Build child nodes 
			if (leftSide.size() > 0)
			{
				treeInfo.left_children++;
				children[0] = new AABBNode(leftSide, treeInfo, depth + 1);
			}
			if (rightSide.size() > 0)
			{
				treeInfo.right_children++;
				children[1] = new AABBNode(rightSide, treeInfo, depth + 1);
			}
		}
		else
		{
			//should never happen 
			bMakeChildren = false;
		}
	}

	if (!bMakeChildren)
	{
		m_pVerts = vertexList;
		//Store the data directly if you want.... 
	}
}