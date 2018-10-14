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
   uchar globalAO;
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

   const int3 size = (int3)( 34, 34, 34 );

   const int sizeXZ = size.x * size.z;

   const uint index = ((coord.y * sizeXZ )) + ((coord.z * size.x)) + coord.x;
   const uint index1 = ((coord1.y * sizeXZ )) + ((coord1.z * size.x)) + coord1.x;
   const uint index2 = ((coord2.y * sizeXZ )) + ((coord2.z * size.x)) + coord2.x;
   const uint index3 = ((coord3.y * sizeXZ )) + ((coord3.z * size.x)) + coord3.x;
   const uint index4 = ((coord4.y * sizeXZ )) + ((coord4.z * size.x)) + coord4.x;
   const uint index5 = ((coord5.y * sizeXZ )) + ((coord5.z * size.x)) + coord5.x;
   const uint index6 = ((coord6.y * sizeXZ )) + ((coord6.z * size.x)) + coord6.x;
   const uint index7 = ((coord7.y * sizeXZ )) + ((coord7.z * size.x)) + coord7.x;

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
calulateGlobalAO( __global cl_vertex_t* vertices, cl_float3_t world_pos, float res, uchar level )
{
   uint i = get_global_id( 0 );

   float step = 255.0f / 8;

   int x1 = vertices[i].px;
   int y1 = vertices[i].py;
   int z1 = vertices[i].pz;



   /*const float3 start = ( float3 )( x1, y1, z1 );

   const int fRatio = 34 / ( 34 / 1 );
   const float fHalfRatio = fRatio * 0.5f;

   const float3 halfRatio = ( float3 )( fHalfRatio, fHalfRatio, fHalfRatio );
   const float3 offset = ( float3 )( 0.5f, 0.5f, 0.5f );*/

   int uVisibleDirections = 0;

   for( int ct = 1; ct < 10 * level; ct++ )
   {
      const float3 position = ( float3 )( ( world_pos.x + x1 * res ),
                                          ( world_pos.y + y1 * res + ct * res),
                                          ( world_pos.z + z1 * res ) );

      if( densities3d( position, level ) > 0.0f )
      {
         uVisibleDirections = 1;
         break;
      }
         
   }

   vertices[i].globalAO = uVisibleDirections == 0 ? 128 : 64;

}

kernel
void
classifyChunk( __global char* outVal, cl_float3_t world_pos, float res )
{
   uint edge = get_global_id( 0 );
   uint sample1 = get_global_id( 1 );

   float length = res * 32;
   float step = length / 50;

   const float3 position = ( float3 )( ( world_pos.x ),
                                       ( world_pos.y ),
                                       ( world_pos.z ) );

   const float3 positionTF = ( float3 )( ( position.x ),
                                         ( position.y + length ),
                                         ( position.z + length ) );

   const float3 positionLB = ( float3 )( ( position.x + length ),
                                         ( position.y + length ),
                                         ( position.z ) );

   const float3 positionTR = ( float3 )( ( position.x + length ),
                                         ( position.y ),
                                         ( position.z + length ) );


   float3 position1; 
   float startN = 0;
   float endN = 0;
   int maxSample = 50;

   if( edge == 0 )
      position1 = ( float3 )( ( position.x + sample1 * step ),
                              ( position.y ),
                              ( position.z ) );
   else if( edge == 1 )
      position1 = ( float3 )( ( position.x ),
                              ( position.y + sample1 * step ),
                              ( position.z ) );
   else if( edge == 2 )
      position1 = ( float3 )( ( position.x ),
                              ( position.y ),
                              ( position.z + sample1 * step ) );
   else if( edge == 3 )
      position1 = ( float3 )( ( positionTF.x + sample1 * step ),
                              ( positionTF.y ),
                              ( positionTF.z ) );
   else if( edge == 4 )
      position1 = ( float3 )( ( positionTF.x ),
                              ( positionTF.y ),
                              ( positionTF.z - sample1 * step ) );
   else if( edge == 5 )
      position1 = ( float3 )( ( positionTF.x ),
                              ( positionTF.y - sample1 * step ),
                              ( positionTF.z ) );
   else if( edge == 6 )
      position1 = ( float3 )( ( positionLB.x ),
                              ( positionLB.y - sample1 * step ),
                              ( positionLB.z ) );
   else if( edge == 7 )
      position1 = ( float3 )( ( positionLB.x - sample1 * step ),
                              ( positionLB.y ),
                              ( positionLB.z ) );
   else if( edge == 8 )
      position1 = ( float3 )( ( positionLB.x ),
                              ( positionLB.y ),
                              ( positionLB.z + sample1 * step ) );
   else if( edge == 9 )
      position1 = ( float3 )( ( positionTR.x ),
                              ( positionTR.y ),
                              ( positionTR.z - sample1 * step ) );
   else if( edge == 10 )
      position1 = ( float3 )( ( positionTR.x - sample1 * step ),
                              ( positionTR.y ),
                              ( positionTR.z ) );
   else if( edge == 11 )
      position1 = ( float3 )( ( positionTR.x ),
                              ( positionTR.y + sample1 * step ),
                              ( positionTR.z ) );

   if( edge == 0 || edge == 1 || edge == 2 )
      startN = densities3d( position, 1 );
   else if( edge == 3 || edge == 4 || edge == 5 )
      startN = densities3d( positionTF, 1 );
   else if( edge == 6 || edge == 7 || edge == 8 )
      startN = densities3d( positionLB, 1 );
   else if( edge == 9 || edge == 10 || edge == 11 )
      startN = densities3d( positionTR, 1 );

   endN = densities3d( position1, 1 );

   if( ( startN > 0.0f && endN <= 0.0f ) || ( startN <= 0.0f && endN > 0.0f ) )
     outVal[ ( maxSample ) * edge + sample1 ] = 1;
   else
     outVal[ ( maxSample ) * edge + sample1 ] = 0;
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
	
	if( blockNeighbourInfo > 0 && blockNeighbourInfo != 63 )
	{
		occupied[index] = 1;
      output[index].block_info = blockNeighbourInfo;
      output[index].grid_pos[0] = (coord.x);
      output[index].grid_pos[1] = (coord.y);
      output[index].grid_pos[2] = (coord.z);
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
   const uint i = get_global_id(0);
	
   if( i < numVoxels)
   {
      const int scanIndex = scan1[i];

      if( occupied[i] ) 
      {
         output[ scanIndex ].block_info = edges[i].block_info;
         output[ scanIndex ].grid_pos[0] = edges[i].grid_pos[0];
         output[ scanIndex ].grid_pos[1] = edges[i].grid_pos[1];
         output[ scanIndex ].grid_pos[2] = edges[i].grid_pos[2];
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
	
   const int3 coord = (int3)( get_global_id(0), 
                              get_global_id(1),
                              get_global_id(2) );

   const int size = get_global_size(0);

   const float3 position = (float3)( ( world_pos.x + coord.x * res ),
                                     ( world_pos.y + coord.y * res ),
                                     ( world_pos.z + coord.z * res ) );

   const uint index = ((coord.y * size * size)) + ((coord.z * size)) + (coord.x);

   densities[index] = densities3d(position, level) > 0.0f ? 1 : 0;

}
