typedef struct cl_edge_info
{
    uchar grid_pos[3];
    uchar edge_info;
} cl_edge_info_t;


kernel
void
classifyEdges(__global float* densities, __global cl_edge_info_t* output, __global int* occupied, cl_float3_t base_pos, float res)
{
    const int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

    const int3 size = (int3)(get_global_size(0), get_global_size(1), get_global_size(2));

    const uint index = ((coord.y * size.x * size.z)) + ((coord.z * size.x)) + coord.x;

    const float base_density = densities[index];

    const int3 c1 = coord + CORNER_OFFSETS[0];
    const int3 c2 = coord + CORNER_OFFSETS[1];
    const int3 c3 = coord + CORNER_OFFSETS[2];

    output[index].grid_pos[0] = coord.x;
    output[index].grid_pos[1] = coord.y;
    output[index].grid_pos[2] = coord.z;

    const uint i1 = ((c1.y * size.x * size.z)) + ((c1.z * size.x)) + (c1.x);
    const uint i2 = ((c2.y * size.x * size.z)) + ((c2.z * size.x)) + (c2.x);
    const uint i3 = ((c3.y * size.x * size.z)) + ((c3.z * size.x)) + (c3.x);

    const uchar sign1 = ((densities[i1] >= 0.f && base_density < 0.f) || (base_density >= 0.f && densities[i1] < 0.f)) ? 1 : 0;
    const uchar sign2 = ((densities[i2] >= 0.f && base_density < 0.f) || (base_density >= 0.f && densities[i2] < 0.f)) ? 2 : 0;
    const uchar sign3 = ((densities[i3] >= 0.f && base_density < 0.f) || (base_density >= 0.f && densities[i3] < 0.f)) ? 4 : 0;

    output[index].edge_info = 0;
    output[index].edge_info |= sign1;
    output[index].edge_info |= sign2;
    output[index].edge_info |= sign3;

    occupied[index] = 0;

    if(coord.x < (size.x ) && 
       coord.y < (size.y ) && 
       coord.z < (size.z ) && 
       output[index].edge_info != 0)
    {
        occupied[index] = 1;
    }

}


kernel
void
computeCorners(__global float* densities, __global cl_int4_t* output, int chunk_size)
{

    const int3 size = (int3)(chunk_size, chunk_size, chunk_size);

    const uint index = get_global_id(0);

    const int3 coord = (int3)(output[index].x, output[index].y, output[index].z);


    for (int i = 0; i < 8; i++)
    {
        const int3 pos1 = coord + CHILD_MIN_OFFSETS[i];
        
        const uint index2 = ((pos1.y * size.x * size.z)) + ((pos1.z * size.x)) + pos1.x;

        if(densities[index2] < 0.f)
            output[index].w |= (1 << i);
    }

}

kernel
void
compactEdges(__global unsigned int* occupied, __global unsigned  int* scan1, __global cl_edge_info_t* output, __global cl_edge_info_t* edges, int numVoxels)
{
    uint i = get_global_id(0);

    if( i < numVoxels)
    {
        if (occupied[i]) 
        {
            output[ scan1[i] ].edge_info = edges[i].edge_info;
            output[ scan1[i] ].grid_pos[0] = edges[i].grid_pos[0];
            output[ scan1[i] ].grid_pos[1] = edges[i].grid_pos[1];
            output[ scan1[i] ].grid_pos[2] = edges[i].grid_pos[2];
        }
    }
  

}

kernel
void
genZeroCross(__global cl_edge_t* edge_array, cl_float3_t base_pos, float res)
{

    const int index = get_global_id(0);

    const int3 coord = (int3)(edge_array[index].grid_pos[0], edge_array[index].grid_pos[1], edge_array[index].grid_pos[2]);

    const float3 e1 = (float3)((base_pos.x + coord.x * res ), (base_pos.y + coord.y * res), (base_pos.z + coord.z * res));

    float3 e2;

    if(edge_array[index].edge == 0)
    {
        e2 = e1 + (float3)(res, 0, 0);
    } 
    else if(edge_array[index].edge == 4)
    {
        e2 = e1 + (float3)(0, res, 0);
    } 
    else if(edge_array[index].edge == 8)
    {
        e2 = e1 + (float3)(0, 0, res);
    } 

    const float3 p = approximateZeroCrossingPosition(e1, e2, res);

    const float3 pos = ( p - (float3)(base_pos.x, base_pos.y, base_pos.z)) / res;

    edge_array[index].zero_crossing.x = pos.x;
    edge_array[index].zero_crossing.y = pos.y;
    edge_array[index].zero_crossing.z = pos.z;

    const float3 surfNorm = calculateSurfaceNormal(p, res);
    
    edge_array[index].normal.x = surfNorm.x;
    edge_array[index].normal.y = surfNorm.y;
    edge_array[index].normal.z = surfNorm.z;

}

kernel
void
test(__global float* densities, cl_float3_t world_pos, float res)
{

    const int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));

    const int size = get_global_size(0);

	const float3 position = (float3)((world_pos.x + coord.x * res),
                                     (world_pos.y + coord.y * res),
                                     (world_pos.z + coord.z * res));

	const uint index = ((coord.y * size * size)) + ((coord.z * size)) + (coord.x);

    densities[index] = densities3d(position);

}