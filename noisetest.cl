

#define ONE_F1                 (1.0f)
#define ZERO_F1                (0.0f)

#define USE_IMAGES_FOR_RESULTS (0)  // NOTE: It may be faster to use buffers instead of images

////////////////////////////////////////////////////////////////////////////////////////////////////

static __constant int3 CHILD_MIN_OFFSETS[8] =
{
	// needs to match the vertMap from Dual Contouring impl
	 (int3)( 0, 0, 0 ), 
    (int3)( 0, 0, 1 ), 
    (int3)( 0, 1, 0 ), 
    (int3)( 0, 1, 1 ), 
    (int3)( 1, 0, 0 ), 
    (int3)( 1, 0, 1 ), 
    (int3)( 1, 1, 0 ), 
    (int3)( 1, 1, 1 )
};

static __constant int3 CORNER_OFFSETS[3] =
{
	// needs to match the vertMap from Dual Contouring impl
	 (int3)( 1, 0, 0 ), 
    (int3)( 0, 1, 0 ), 
    (int3)( 0, 0, 1 )
};

__constant int P_MASK = 255;

__constant int P_SIZE = 256;

__constant int P[512] = {151,160,137,91,90,15,

  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,

  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,

  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,

  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,

  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,

  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,

  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,

  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,

  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,

  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,

  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,

  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,

  151,160,137,91,90,15,

  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,

  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,

  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,

  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,

  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,

  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,

  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,

  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,

  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,

  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,

  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,

  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,  

  };

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

__constant int G_MASK = 15;

__constant int G_SIZE = 16;

__constant int G_VECSIZE = 4;

__constant float G[16*4] = {

      +ONE_F1,  +ONE_F1, +ZERO_F1, +ZERO_F1, 

      -ONE_F1,  +ONE_F1, +ZERO_F1, +ZERO_F1, 

      +ONE_F1,  -ONE_F1, +ZERO_F1, +ZERO_F1, 

      -ONE_F1,  -ONE_F1, +ZERO_F1, +ZERO_F1,

      +ONE_F1, +ZERO_F1,  +ONE_F1, +ZERO_F1, 

      -ONE_F1, +ZERO_F1,  +ONE_F1, +ZERO_F1, 

      +ONE_F1, +ZERO_F1,  -ONE_F1, +ZERO_F1, 

      -ONE_F1, +ZERO_F1,  -ONE_F1, +ZERO_F1,

     +ZERO_F1,  +ONE_F1,  +ONE_F1, +ZERO_F1, 

     +ZERO_F1,  -ONE_F1,  +ONE_F1, +ZERO_F1, 

     +ZERO_F1,  +ONE_F1,  -ONE_F1, +ZERO_F1, 

     +ZERO_F1,  -ONE_F1,  -ONE_F1, +ZERO_F1,

      +ONE_F1,  +ONE_F1, +ZERO_F1, +ZERO_F1, 

      -ONE_F1,  +ONE_F1, +ZERO_F1, +ZERO_F1, 

     +ZERO_F1,  -ONE_F1,  +ONE_F1, +ZERO_F1, 

     +ZERO_F1,  -ONE_F1,  -ONE_F1, +ZERO_F1

};  

  

////////////////////////////////////////////////////////////////////////////////////////////////////

 

int mod(int x, int a)

{

    int n = (x / a);

    int v = v - n * a;

    if ( v < 0 )

        v += a;

    return v;   

}

 

float smooth(float t)

{

    return t*t*t*(t*(t*6.0f-15.0f)+10.0f); 

}

 

float4 normalized(float4 v)

{

    float d = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    d = d > 0.0f ? d : 1.0f;

    float4 result = (float4)(v.x, v.y, v.z, 0.0f) / d;

    result.w = 1.0f;

    return result;

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

float mix1d(float a, float b, float t)

{

    float ba = b - a;

    float tba = t * ba;

    float atba = a + tba;

    return atba;    

}

 

float2 mix2d(float2 a, float2 b, float t)

{

    float2 ba = b - a;

    float2 tba = t * ba;

    float2 atba = a + tba;

    return atba;    

}

 

float4 mix3d(float4 a, float4 b, float t)

{

    float4 ba = b - a;

    float4 tba = t * ba;

    float4 atba = a + tba;

    return atba;    

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

int lattice1d(int i)

{

    return P[i];

}

 

int lattice2d(int2 i)

{

    return P[i.x + P[i.y]];

}

 

int lattice3d(int4 i)

{

    return P[i.x + P[i.y + P[i.z]]];

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

float gradient1d(int i, float v)

{

    int index = (lattice1d(i) & G_MASK) * G_VECSIZE;

    float g = G[index + 0];

    return (v * g);

}

 

float gradient2d(int2 i, float2 v)

{

    int index = (lattice2d(i) & G_MASK) * G_VECSIZE;

    float2 g = (float2)(G[index + 0], G[index + 1]);

    return dot(v, g);

}

 

float gradient3d(int4 i, float4 v)

{

    int index = (lattice3d(i) & G_MASK) * G_VECSIZE;

    float4 g = (float4)(G[index + 0], G[index + 1], G[index + 2], 1.0f);

    return dot(v, g);

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

// Signed gradient noise 1d

float sgnoise1d(float position)

{

    float p = position;

    float pf = floor(p);

    int ip = (int)pf;

    float fp = p - pf;        

    ip &= P_MASK;

    

    float n0 = gradient1d(ip + 0, fp - 0.0f);

    float n1 = gradient1d(ip + 1, fp - 1.0f);

 

    float n = mix1d(n0, n1, smooth(fp));

    return n * (1.0f / 0.7f);

}

 

// Signed gradient noise 2d

float sgnoise2d(float2 position)

{

    float2 p = position;

    float2 pf = floor(p);

    int2 ip = (int2)((int)pf.x, (int)pf.y);

    float2 fp = p - pf;        

    ip &= P_MASK;

    

    const int2 I00 = (int2)(0, 0);

    const int2 I01 = (int2)(0, 1);

    const int2 I10 = (int2)(1, 0);

    const int2 I11 = (int2)(1, 1);

    

    const float2 F00 = (float2)(0.0f, 0.0f);

    const float2 F01 = (float2)(0.0f, 1.0f);

    const float2 F10 = (float2)(1.0f, 0.0f);

    const float2 F11 = (float2)(1.0f, 1.0f);

 

    float n00 = gradient2d(ip + I00, fp - F00);

    float n10 = gradient2d(ip + I10, fp - F10);

    float n01 = gradient2d(ip + I01, fp - F01);

    float n11 = gradient2d(ip + I11, fp - F11);

 

    const float2 n0001 = (float2)(n00, n01);

    const float2 n1011 = (float2)(n10, n11);

 

    float2 n2 = mix2d(n0001, n1011, smooth(fp.x));

    float n = mix1d(n2.x, n2.y, smooth(fp.y));

    return n * (1.0f / 0.7f);

}

 

// Signed gradient noise 3d

float sgnoise3d(float4 position)

{

 

    float4 p = position;

    float4 pf = floor(p);

    int4 ip = (int4)((int)pf.x, (int)pf.y, (int)pf.z, 0.0);

    float4 fp = p - pf;        

    ip &= P_MASK;

 

    int4 I000 = (int4)(0, 0, 0, 0);

    int4 I001 = (int4)(0, 0, 1, 0);  

    int4 I010 = (int4)(0, 1, 0, 0);

    int4 I011 = (int4)(0, 1, 1, 0);

    int4 I100 = (int4)(1, 0, 0, 0);

    int4 I101 = (int4)(1, 0, 1, 0);

    int4 I110 = (int4)(1, 1, 0, 0);

    int4 I111 = (int4)(1, 1, 1, 0);

    

    float4 F000 = (float4)(0.0f, 0.0f, 0.0f, 0.0f);

    float4 F001 = (float4)(0.0f, 0.0f, 1.0f, 0.0f);

    float4 F010 = (float4)(0.0f, 1.0f, 0.0f, 0.0f);

    float4 F011 = (float4)(0.0f, 1.0f, 1.0f, 0.0f);

    float4 F100 = (float4)(1.0f, 0.0f, 0.0f, 0.0f);

    float4 F101 = (float4)(1.0f, 0.0f, 1.0f, 0.0f);

    float4 F110 = (float4)(1.0f, 1.0f, 0.0f, 0.0f);

    float4 F111 = (float4)(1.0f, 1.0f, 1.0f, 0.0f);

    

    float n000 = gradient3d(ip + I000, fp - F000);

    float n001 = gradient3d(ip + I001, fp - F001);

    

    float n010 = gradient3d(ip + I010, fp - F010);

    float n011 = gradient3d(ip + I011, fp - F011);

    

    float n100 = gradient3d(ip + I100, fp - F100);

    float n101 = gradient3d(ip + I101, fp - F101);

 

    float n110 = gradient3d(ip + I110, fp - F110);

    float n111 = gradient3d(ip + I111, fp - F111);

 

    float4 n40 = (float4)(n000, n001, n010, n011);

    float4 n41 = (float4)(n100, n101, n110, n111);

 

    float4 n4 = mix3d(n40, n41, smooth(fp.x));

    float2 n2 = mix2d(n4.xy, n4.zw, smooth(fp.y));

    float n = mix1d(n2.x, n2.y, smooth(fp.z));

    return n * (1.0f / 0.7f);

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

// Unsigned Gradient Noise 1d

float ugnoise1d(float position)

{

    return (0.5f - 0.5f * sgnoise1d(position));

}

 

// Unsigned Gradient Noise 2d

float ugnoise2d(float2 position)

{

    return (0.5f - 0.5f * sgnoise2d(position));

}

 

// Unsigned Gradient Noise 3d

float ugnoise3d(float4 position)

{

    return (0.5f - 0.5f * sgnoise3d(position));

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

uchar4

tonemap(float4 color)

{

    uchar4 result = convert_uchar4_sat_rte(color * 255.0f);

    return result;

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

float monofractal2d(

    float2 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 0.0f;

    int iterations = (int)octaves;

    

    for (i = 0; i < iterations; i++)

    {

        fi = (float)i;

        sample = sgnoise2d(position * frequency);

        sample *= pow( lacunarity, -fi * increment );

        value += sample;

        frequency *= lacunarity;

    }

    

    remainder = octaves - (float)iterations;

    if ( remainder > 0.0f )

    {

        sample = remainder * sgnoise2d(position * frequency);

        sample *= pow( lacunarity, -fi * increment );

        value += sample;

    }

        

    return value;   

}

 

float multifractal2d(

    float2 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

   
    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 1.0f;

    int iterations = (int)octaves;

    

    for (i = 0; i < iterations; i++)

    {

        fi = (float)i;

        sample = sgnoise2d(position * frequency) + 1.0f;

        sample *= pow( lacunarity, -fi * increment );

        value *= sample;

        frequency *= lacunarity;

    }

    

    remainder = octaves - (float)iterations;

    if ( remainder > 0.0f )

    {

        sample = remainder * (sgnoise2d(position * frequency) + 1.0f);

        sample *= pow( lacunarity, -fi * increment );

        value *= sample;

    }

        

    return value;   

}
        

 
 

float turbulence2d(

    float2 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 0.0f;

    int iterations = (int)octaves;

    

    for (i = 0; i < iterations; i++)

    {

        fi = (float)i;

        sample = sgnoise2d(position * frequency);

        sample *= pow( lacunarity, -fi * increment );

        value += fabs(sample);

        frequency *= lacunarity;

    }

    

    remainder = octaves - (float)iterations;

    if ( remainder > 0.0f )

    {

        sample = remainder * sgnoise2d(position * frequency);

        sample *= pow( lacunarity, -fi * increment );

        value += fabs(sample);

    }

        

    return value;   

}

 

float ridgedmultifractal2d(

    float2 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 0.0f;

    int iterations = (int)octaves;

 

    float threshold = 0.5f;

    float offset = 1.0f;

    float weight = 1.0f;

 

    float signal = fabs( sgnoise2d(position * frequency) );

    signal = offset - signal;

    signal *= signal;

    value = signal;

 
	#pragma unroll
    for ( i = 0; i < iterations; i++ )

    {

        frequency *= lacunarity;

        weight = clamp( signal * threshold, 0.0f, 1.0f );   

        signal = fabs( sgnoise2d(position * frequency) );

        signal = offset - signal;

        signal *= signal;

        signal *= weight;

        value += signal * pow( lacunarity, -fi * increment );

 

    }

    return value;

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

float multifractal3d(

    float4 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{


    float value = 0.0f;

	float curPersistence = 1.0;

	float persistence = 0.5;

	float scale = frequency;

	for (size_t o=0;o<octaves;++o)
	{


		float4 pos = (float4)(position.x * scale, position.y * scale, position.z * scale, 0);

		float signal = ((sgnoise3d(pos))) * 0.01;

		value += signal * curPersistence;
		
		scale *= lacunarity;
		curPersistence *= persistence;
		
	}

    
    return value;   

}

 

float ridgedmultifractal3d(

    float4 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 0.0f;

    int iterations = (int)octaves;

 

    float threshold = 0.5f;

    float offset = 1.0f;

    float weight = 1.0f;

 

    float signal = fabs( (1.0f - 2.0f * sgnoise3d(position * frequency)) );

    signal = offset - signal;

    signal *= signal;

    value = signal;

 

    for ( i = 0; i < iterations; i++ )

    {

        frequency *= lacunarity;

        weight = clamp( signal * threshold, 0.0f, 1.0f );   

        signal = fabs( (1.0f - 2.0f * sgnoise3d(position * frequency)) );

        signal = offset - signal;

        signal *= signal;

        signal *= weight;

        value += signal * pow( lacunarity, -fi * increment );

 

    }

    return value;

}

 

float turbulence3d(

    float4 position, 

    float frequency,

    float lacunarity, 

    float increment, 

    float octaves)

{

    int i = 0;

    float fi = 0.0f;

    float remainder = 0.0f; 

    float sample = 0.0f;    

    float value = 0.0f;

    int iterations = (int)octaves;

    

    for (i = 0; i < iterations; i++)

    {

        fi = (float)i;

        sample = (1.0f - 2.0f * sgnoise3d(position * frequency));

        sample *= pow( lacunarity, -fi * increment );

        value += fabs(sample);

        frequency *= lacunarity;

    }

    

    remainder = octaves - (float)iterations;

    if ( remainder > 0.0f )

    {

        sample = remainder * (1.0f - 2.0f * sgnoise3d(position * frequency));

        sample *= pow( lacunarity, -fi * increment );

        value += fabs(sample);

    }

        

    return value;   

}

 

////////////////////////////////////////////////////////////////////////////////////////////////////

 

__kernel void 

GradientNoiseArray2d(   

    __global uchar4 *output,

    const float2 bias, 

    const float2 scale,

    const float amplitude) 

{

    int2 coord = (int2)(get_global_id(0), get_global_id(1));

 

    int2 size = (int2)(get_global_size(0), get_global_size(1));

 

    float2 position = (float2)(coord.x / (float)size.x, coord.y / (float)size.y);

    

    float2 sample = (position + bias) * scale;

   

    float value = ugnoise2d(sample);

    

    float4 result = (float4)(value, value, value, 1.0f) * amplitude;

 

    uint index = coord.y * size.x + coord.x;

    output[index] = tonemap(result);

}

 

__kernel void 

MonoFractalArray2d(

    __global uchar4 *output,

    const float2 bias, 

    const float2 scale,

    const float lacunarity, 

    const float increment, 

    const float octaves,    

    const float amplitude)

{

    int2 coord = (int2)(get_global_id(0), get_global_id(1));

 

    int2 size = (int2)(get_global_size(0), get_global_size(1));

 

    float2 position = (float2)(coord.x / (float)size.x, 

                                  coord.y / (float)size.y);

    

    float2 sample = (position + bias);

   

    float value = monofractal2d(sample, scale.x, lacunarity, increment, octaves);

 

    float4 result = (float4)(value, value, value, 1.0f) * amplitude;

 

    uint index = coord.y * size.x + coord.x;

    output[index] = tonemap(result);

}

 

__kernel void 

TurbulenceArray2d(

    __global uchar4 *output,

    const float2 bias, 

    const float2 scale,

    const float lacunarity, 

    const float increment, 

    const float octaves,    

    const float amplitude) 

{

    int2 coord = (int2)(get_global_id(0), get_global_id(1));

 

    int2 size = (int2)(get_global_size(0), get_global_size(1));

 

    float2 position = (float2)(coord.x / (float)size.x, coord.y / (float)size.y);

    

    float2 sample = (position + bias);

 

    float value = turbulence2d(sample, scale.x, lacunarity, increment, octaves);

 

    float4 result = (float4)(value, value, value, 1.0f) * amplitude;

 

    uint index = coord.y * size.x + coord.x;

    output[index] = tonemap(result);

}

 

__kernel void 

RidgedMultiFractalArray2d(  

    __global uchar4 *output,

    const float2 bias, 

    const float2 scale,

    const float lacunarity, 

    const float increment, 

    const float octaves,    

    const float amplitude) 

{

    int2 coord = (int2)(get_global_id(0), get_global_id(1));

 

    int2 size = (int2)(get_global_size(0), get_global_size(1));

 

    float2 position = (float2)(coord.x / (float)size.x, 

                                  coord.y / (float)size.y);

        

    float2 sample = (position + bias);

 

    float value = ridgedmultifractal2d(sample, scale.x, lacunarity, increment, octaves);

 

    float4 result = (float4)(value, value, value, 1.0f) * amplitude;

 

    uint index = coord.y * size.x + coord.x;

    output[index] = tonemap(result);

}

float fBM(float2 position, float frequency, float lacunarity, float octaves)
{

   float total = 0.0f;

   float gain = 0.52f;
   //float gain = 0.48f;

   float amplitude = gain;

   #pragma unroll
   for (int i = 0; i < octaves; ++i)
   {
      total += sgnoise2d(position * frequency) * amplitude;         
      frequency *= lacunarity;
      amplitude *= gain;
   }

   return total;   

}

float sphereFunction(float4 pos)
{
	return (pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
}

/*float densities3d(float3 pos, uchar level)
{
    
	
	level = 14;

    float3 mod_pos = pos / 12800;
    
    float mask = clamp(0.5f - 0.5f * fBM((float2)(mod_pos.x, mod_pos.z), 0.1f, 2.12f, 15), 0.0f, 1.0f);

    float baseMountain =  ridgedmultifractal3d((float2)(mod_pos.x, mod_pos.y, mod_pos.z), 0.31f, 2.19f, 0, 15);


    //return pos.y < 1000 ? -1 : 0;
    float billow = fBM((float2)(mod_pos.x-4000, mod_pos.z), 0.1f, 2.19f, 2);

    float detail =  clamp(1.0f-abs(ridgedmultifractal2d((float2)(mod_pos.x - 4000, mod_pos.z), 0.2, 2.183, 0, 10)), 0.0f, 1.0f);

	

    //return detail - pos.y / 6000;
	
	//return sphereFunction((float4)(pos.x, pos.y, pos.z, 0.0f)) - 819200335.f ;

    return mix(billow - pos.y / 2000, baseMountain - pos.y / 26000, mask);


}*/

float densities3d(float3 pos, uchar level)
{
    
   //return pos.y < 400 ? -1 : 1;
   return sphereFunction((float4)(pos.x, pos.y, pos.z, 0.0f)) - 200000335.f ;
   float3 mod_pos = pos * 0.001f;
   //level = 14;
	     
   float mask = clamp(fBM((float2)(pos.x, pos.z), 0.00001f, 2.12f, 9), 0.0f, 1.0f);
   float mask2 = clamp(fBM((float2)(pos.x + 80000, pos.z), 0.00004f, 2.12f, 12), 0.0f, 1.0f);

   float baseMountain = fBM((float2)(mod_pos.x - 2000, mod_pos.z), 0.016f, 2.19f, min((int)(level), 3));
   float billow = fBM((float2)(mod_pos.x - 4000, mod_pos.z), 0.009f, 2.19f,  min((int)(level), 5));
   float detail = fabs(ridgedmultifractal2d((float2)(mod_pos.x - 14000, mod_pos.z - 8000), 0.045, 2.233, 0, min((int)(level), 12)));

   if( pos.y > 900 )
   {
      detail = detail * 0.85;
   }

   if( pos.y > 1800 )
   {
      detail = detail * 0.85;
   }

   if( pos.y > 2700 )
   {
      detail = detail * 0.85;
   }

   return mix(mix(billow * 2, detail * 10, mask) , baseMountain * 10 , mask2) - pos.y / 2600;


}

uchar material3d(float density, float3 pos, uchar level)
{
    
	uchar material = 1;

	level = 14;

    float3 mod_pos = pos / 1280;
    
    float mask = clamp(fBM((float2)(pos.x, pos.z), 0.00001f, 2.12f, 6), 0.0f, 1.0f);
	float mask2 = clamp(fBM((float2)(pos.x+80000, pos.z), 0.00004f, 2.12f, 6), 0.0f, 1.0f);

	if( mask2 != 0.0f)
	{
		material = 3;
	}

	if( mask != 0.0f)
	{
		material = 2;
	}


 
	return material;

}



typedef struct cl_float3
{
   	float x;
    float y;
    float z;
} cl_float3_t;

typedef struct int2
{
    int values[2];
} int2_cl;

typedef struct cl_edge
{
    uchar grid_pos[3];
    uchar edge;
    cl_float3_t normal;
    cl_float3_t zero_crossing;
	uchar material;
} cl_edge_t;

typedef struct cl_int4
{
    uchar x;
    uchar y;
    uchar z;
    uchar w;
} cl_int4_t;


float3 calculateSurfaceNormal(float3 pos, float res, uchar level)
{
    const float H = 1.f * res * 2.0f;
    const float dx = densities3d(pos + (float3)(H, 0.f, 0.f), level) - densities3d(pos - (float3)(H, 0.f, 0.f), level);
    const float dy = densities3d(pos + (float3)(0.f, H, 0.f), level) - densities3d(pos - (float3)(0.f, H, 0.f), level);
    const float dz = densities3d(pos + (float3)(0.f, 0.f, H), level) - densities3d(pos - (float3)(0.f, 0.f, H), level);

    return (float3)(dx, dy, dz);
} 


// @TODO changing this affects how well different lods match up 
// Potentially use a totally different approach ( going on until a certain threshold is reached)

__constant int ZERO_STEPS = 6;
__constant float ZERO_INCREMENT = 1.f / 6.0f;


float3 approximateZeroCrossingPosition(float3 p0, float3 p1, float res, uchar level)
{

    float minValue = FLT_MAX;
    float currentT = 0.f;
    float t = 0.f;

	#pragma unroll
	for (int i = 0; i <= ZERO_STEPS; i++)
	{
		const float3 p = mix(p0, p1, currentT);
		const float d = fabs(densities3d(p, level));
		if (d < minValue)
		{
			t = currentT;
			minValue = d;
		}

		currentT += ZERO_INCREMENT;
	}

   return mix(p0, p1, t);
}


float3 findCrossingPoint(int quality, float3 pt0, float3 pt1, uchar level)
{

  float isoValue = 0.0f;

  float3 p0 = pt0;
  float v0 = densities3d(p0, level);
  float3 p1 = pt1;
  float v1 = densities3d(p1, level);

  float alpha = (0 - v0) / (v1 - v0);

  // Interpolate
  float3 pos;
  pos.x = p0.x + alpha * (p1.x - p0.x);
  pos.y = p0.y + alpha * (p1.y - p0.y);
  pos.z = p0.z + alpha * (p1.z - p0.z);


  // Re-Sample
  float val = densities3d(pos, level);

  // Return if good enough
  if((fabs(isoValue-val)<0.000001f)||(quality==0))
  {
    return pos;
  }
  else
  {
    if(val<0.f)
    {
      if(v0>0.f)
        pos = findCrossingPoint(quality-1, pos, pt0, level);
      else if(v1>0.f)
        pos = findCrossingPoint(quality-1, pos, pt1, level);
    }
    else if(val>0.f)
    {
      if(v0<0.f)
        pos = findCrossingPoint(quality-1, pt0, pos, level);
      else if(v1<0.f)
        pos = findCrossingPoint(quality-1, pt1, pos, level);
    }
  }

  return pos;
}


float3 LinearInterp(float3 p1, float p1Val, float3 p2, float p2Val, float value)
{
  
    float t = 1 * (p2Val - value) / (p2Val - p1Val);
    float sum1 = (p2Val - p1Val);
    float fSum = (value - p1Val) ;
    float3 sum = (float3)((p2.x - p1.x) * fSum, (p2.y - p1.y) * fSum, (p2.z - p1.z) * fSum);
    sum /= sum1;
    return (float3)(p1.x + sum.x, p1.y + sum.y, p1.z + sum.z);
}
