typedef struct cl_edge_info
{
   uchar grid_pos[3];
   uchar edge_info;
} cl_edge_info_t;

typedef struct cl_cell_t
{
   float positions[12][3];
   float normals[12][3];
   uchar materials[12];
   uchar xPos;
   uchar yPos;
   uchar zPos;
   uchar corners;
} cell_t;

kernel
void
classifyEdges(__global const int* densities, __global int* occupied, __global cl_edge_info_t* output)
{

   const int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

   const int3 size = (int3)(get_global_size(0), get_global_size(1), get_global_size(2));

   const uint index = ((coord.y * size.x * size.z)) + ((coord.z * size.x)) + coord.x;

   const int base_density = densities[index];

   const int sizeXZ = size.x * size.z;
   uchar edgeinfo = 0;

   const int3 c1 = coord + (int3)( 1, 0, 0 );
   const int3 c2 = coord + (int3)( 0, 1, 0 );
   const int3 c3 = coord + (int3)( 0, 0, 1 );

   const uint i1 = ((c1.y * sizeXZ)) + ((c1.z * size.x)) + (c1.x);
   const uint i2 = ((c2.y * sizeXZ)) + ((c2.z * size.x)) + (c2.x);
   const uint i3 = ((c3.y * sizeXZ)) + ((c3.z * size.x)) + (c3.x);

   edgeinfo |= (densities[i1] == base_density) ? 0 : 1;
   edgeinfo |= (densities[i2] == base_density) ? 0 : 2;
   edgeinfo |= (densities[i3] == base_density) ? 0 : 4;
	
	if(edgeinfo > 0)
	{
		occupied[index] = 1;

		cl_edge_info_t outputPtr;
		outputPtr.edge_info = edgeinfo;
		outputPtr.grid_pos[0] = (coord.x);
		outputPtr.grid_pos[1] = (coord.y);
		outputPtr.grid_pos[2] = (coord.z);
		
		output[index] = outputPtr;
		
	}
	else
	{
		occupied[index] = 0;
	}

}

kernel
void
computeCornersFromCell(__global const int* densities, __global cell_t* output, int chunk_size)
{

   const int3 size = (int3)(chunk_size, chunk_size, chunk_size);

   const uint index = get_global_id(0);

   const int3 coord = (int3)(output[index].xPos, output[index].yPos, output[index].zPos);

   const int sizeXZ = size.x * size.z;


   for (int i = 0; i < 8; i++)
   {
      const int3 pos1 = coord + CHILD_MIN_OFFSETS[i];
        
      const uint index2 = ((pos1.y * sizeXZ)) + ((pos1.z * size.x)) + pos1.x;

      output[index].corners |= ((densities[index2] == 1 ? 1 : 0) << i);
   }

}

kernel
void
computeCorners(__global const int* densities, __global cl_int4_t* output, int chunk_size)
{

   const int3 size = (int3)(chunk_size, chunk_size, chunk_size);

   const uint index = get_global_id(0);

   const int3 coord = (int3)(output[index].x, output[index].y, output[index].z);

   const int sizeXZ = size.x * size.z;


   for (int i = 0; i < 8; i++)
   {
      const int3 pos1 = coord + CHILD_MIN_OFFSETS[i];
        
      const uint index2 = ((pos1.y * sizeXZ)) + ((pos1.z * size.x)) + pos1.x;

      output[index].w |= ((densities[index2] == 1 ? 1 : 0) << i);
   }

}

kernel
void
compactEdges(__global const unsigned int* occupied, __global const unsigned  int* scan1, __global cl_edge_info_t* output, __global cl_edge_info_t* edges, int numVoxels)
{
    uint i = get_global_id(0);
	

    if( i < numVoxels)
    {
		int scanIndex = scan1[i];
		cl_edge_info_t edge = edges[i];

        if (occupied[i]) 
        {
            output[ scanIndex ].edge_info = edge.edge_info;
            output[ scanIndex ].grid_pos[0] = edge.grid_pos[0];
            output[ scanIndex ].grid_pos[1] = edge.grid_pos[1];
            output[ scanIndex ].grid_pos[2] = edge.grid_pos[2];
        }
    }
  

}

static __constant float3 DIRECTION[3] = 
{ 
	(float3)(1.0f, 0.0f, 0.0f) ,
	(float3)(0.0f, 1.0f, 0.0f) ,
	(float3)(0.0f, 0.0f, 1.0f) 
};

kernel
void
genZeroCross(__global cl_edge_t* edge_array, cl_float3_t base_pos, float res, uchar level)
{

   cl_edge_t edge = edge_array[get_global_id(0)];

   const int3 coord = (int3)(edge.grid_pos[0], edge.grid_pos[1], edge.grid_pos[2]);

   const float3 e1 = (float3)((base_pos.x + coord.x * res ), (base_pos.y + coord.y * res), (base_pos.z + coord.z * res));

   float3 e2;

   if(edge.edge == 0)
   {
      e2 = e1 + DIRECTION[0] * res;
   } 
   else if(edge.edge == 4)
   {
      e2 = e1 + DIRECTION[1] * res;
   } 
   else if(edge.edge == 8)
   {
      e2 = e1 + DIRECTION[2] * res;
   } 

   //const float3 p = e1;
   //const float3 p = approximateZeroCrossingPosition(e1, e2, res, level);

   const float3 p = findCrossingPoint(2, e1, e2, level);

   const float3 pos = (p - (float3)(base_pos.x, base_pos.y, base_pos.z)) / res;

   edge.zero_crossing.x = pos.x;
   edge.zero_crossing.y = pos.y;
   edge.zero_crossing.z = pos.z;
	
   const float3 surfNorm = calculateSurfaceNormal(p, res, level);
   const uchar material = material3d(densities3d(p, level), p, level);
    
   edge.normal.x = surfNorm.x;
   edge.normal.y = surfNorm.y;
   edge.normal.z = surfNorm.z;
   edge.material = material;

   edge_array[get_global_id(0)] = edge;
}

kernel
void
test(__global int* densities, cl_float3_t world_pos, float res, uchar level)
{
	

   const int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

   const int size = get_global_size(0);

   const float3 position = (float3)((world_pos.x + coord.x * res),
                                     (world_pos.y + coord.y * res),
                                     (world_pos.z + coord.z * res));

   const uint index = ((coord.y * size * size)) + ((coord.z * size)) + (coord.x);

   densities[index] = densities3d(position, level) <= 0.0f ? 1 : 0;

}
