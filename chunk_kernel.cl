typedef struct cl_block_info
{
   uchar grid_pos[3];
   uchar block_info;
} cl_block_info_t;

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

typedef struct cl_vertex
{
   uchar px;
   uchar py;
   uchar pz;
   uchar localAO;
} cl_vertex_t;


kernel
void
calulateBlockAO(__global const int* densities, __global cl_vertex_t* vertices )
{
   uint i = get_global_id(0);

   float step = 255.0f / 8;

   int x1 = vertices[i].px;
   int y1 = vertices[i].py;
   int z1 = vertices[i].pz;

   const int3 coord =  (int3)( x1,   y1,   z1 );
   const int3 coord1 = (int3)( x1+1, y1,   z1 );
   const int3 coord2 = (int3)( x1,   y1+1, z1 );
   const int3 coord3 = (int3)( x1+1, y1+1, z1 );
   const int3 coord4 = (int3)( x1,   y1,   z1+1 );
   const int3 coord5 = (int3)( x1+1, y1,   z1+1 );
   const int3 coord6 = (int3)( x1,   y1+1, z1+1 );
   const int3 coord7 = (int3)( x1+1, y1+1, z1+1 );

   const int3 size = (int3)( 66, 66, 66 );

   const uint index = ((coord.y * size.x * size.z)) + ((coord.z * size.x)) + coord.x;
   const uint index1 = ((coord1.y * size.x * size.z)) + ((coord1.z * size.x)) + coord1.x;
   const uint index2 = ((coord2.y * size.x * size.z)) + ((coord2.z * size.x)) + coord2.x;
   const uint index3 = ((coord3.y * size.x * size.z)) + ((coord3.z * size.x)) + coord3.x;
   const uint index4 = ((coord4.y * size.x * size.z)) + ((coord4.z * size.x)) + coord4.x;
   const uint index5 = ((coord5.y * size.x * size.z)) + ((coord5.z * size.x)) + coord5.x;
   const uint index6 = ((coord6.y * size.x * size.z)) + ((coord6.z * size.x)) + coord6.x;
   const uint index7 = ((coord7.y * size.x * size.z)) + ((coord7.z * size.x)) + coord7.x;

   int light = 0;
   if( densities[index] == 0 )
      light++;
   if( densities[index1] == 0 )
      light++;
   if( densities[index2] == 0 )
      light++;
   if( densities[index3] == 0 )
      light++;
   if( densities[index4] == 0 )
      light++;
   if( densities[index5] == 0 )
      light++;
   if( densities[index6] == 0 )
      light++;
   if( densities[index7] == 0 )
      light++;

   vertices[i].localAO = light * step;

}

kernel
void
classifyBlocks(__global const int* densities, __global int* occupied, __global cl_block_info_t* output, int borderSize )
{

   const int3 coord = (int3)( get_global_id(0), get_global_id(1), get_global_id(2) );

   const int3 size = (int3)( get_global_size(0), get_global_size(1) , get_global_size(2) );

   const uint index = ((coord.y * size.x * size.z)) + ((coord.z * size.x)) + coord.x;

   const int sizeXZ = size.x * size.z;
   uchar blockNeighbourInfo = 0;


   if ( densities[index] == 0 || coord.x == 0 || coord.y == 0 || coord.z == 0 
                              || coord.x == get_global_size(0) -1 || coord.y == get_global_size(0) -1 || coord.z == get_global_size(0) -1 )
   {
      occupied[index] = 0;
      return;
   }



   const int3 f1 = coord + (int3)( 1, 0, 0 );
   const int3 f2 = coord - (int3)( 1, 0, 0 );
   const int3 f3 = coord + (int3)( 0, 0, 1 );
   const int3 f4 = coord - (int3)( 0, 0, 1 );
   const int3 f5 = coord + (int3)( 0, 1, 0 );
   const int3 f6 = coord - (int3)( 0, 1, 0 );

   const uint i1 = ((f1.y * sizeXZ)) + ((f1.z * size.x)) + (f1.x);
   const uint i2 = ((f2.y * sizeXZ)) + ((f2.z * size.x)) + (f2.x);
   const uint i3 = ((f3.y * sizeXZ)) + ((f3.z * size.x)) + (f3.x);
   const uint i4 = ((f4.y * sizeXZ)) + ((f4.z * size.x)) + (f4.x);
   const uint i5 = ((f5.y * sizeXZ)) + ((f5.z * size.x)) + (f5.x);
   const uint i6 = ((f6.y * sizeXZ)) + ((f6.z * size.x)) + (f6.x);

   blockNeighbourInfo |= (densities[i1] == 1) ? 0 : 32;
   
   blockNeighbourInfo |= (densities[i2] == 1) ? 0 : 16;
   
   blockNeighbourInfo |= (densities[i3] == 1) ? 0 : 4; // front

   blockNeighbourInfo |= (densities[i4] == 1) ? 0 : 8; // back 

   blockNeighbourInfo |= (densities[i5] == 1) ? 0 : 1;

   blockNeighbourInfo |= (densities[i6] == 1) ? 0 : 2;

   if( ( coord.x == 1 ) && blockNeighbourInfo != 0 )
      blockNeighbourInfo |= 16;

   if( ( coord.x == get_global_size(0) -2 ) && blockNeighbourInfo != 0 )
      blockNeighbourInfo |= 32;

   if( ( coord.z == get_global_size(0) -2 ) && blockNeighbourInfo != 0 )
      blockNeighbourInfo |= 4;

   if( ( coord.z == 1 ) && blockNeighbourInfo != 0 )
      blockNeighbourInfo |= 8;
	
	if( blockNeighbourInfo > 0 && blockNeighbourInfo != 63 )
	{
		occupied[index] = 1;
		cl_block_info_t outputPtr;
		outputPtr.block_info = blockNeighbourInfo;
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
compactBlocks( __global const unsigned int* occupied,
               __global const unsigned  int* scan1,
               __global cl_block_info_t* output,
               __global cl_block_info_t* edges,
               int numVoxels)
{
   uint i = get_global_id(0);
	
   if( i < numVoxels)
   {
      int scanIndex = scan1[i];
      cl_block_info_t edge = edges[i];

      if( occupied[i] ) 
      {
         output[ scanIndex ].block_info = edge.block_info;
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

   const float3 p = findCrossingPoint(10, e1, e2, level);

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

   densities[index] = densities3d(position, level) > 0.0f ? 1 : 0;

}
