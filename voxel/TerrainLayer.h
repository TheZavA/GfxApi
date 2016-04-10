#ifndef _TERRAINLAYER_H
#define _TERRAINLAYER_H

#include "common.h"
#include <boost\shared_ptr.hpp>
/// What kind of noise is going to be used
enum class NoiseType
{
	NOISE_PERLIN2D,
	NOISE_PERLIN3D,
	NOISE_FBM2D,
	NOISE_FBM3D,
	NOISE_RIDGED2D,
	NOISE_TURBULENCE2D,
	NOISE_TURBULENCE3D,
};

/// how this layer is going to be combined with the previous layer
enum class LayerCombineMode
{
	COMBINE_REPLACE,
	COMBINE_ADD,
	COMBINE_SUBTRACT,
	COMBINE_MASK_SELECT,
};

/// actual noise provider
class NoiseSource
{
public:
	NoiseSource();
	~NoiseSource();

	NoiseType m_noiseType;
	uint8_t m_octaves;
	float m_frequency;
	float m_lacunarity;
	float m_scale;
};

typedef boost::shared_ptr<NoiseSource> NoiseSourcePtr;


class TerrainLayer
{
public:
	TerrainLayer();
	~TerrainLayer();

	LayerCombineMode m_combineMode;

	NoiseSourcePtr m_layerNoise;
	NoiseSourcePtr m_maskNoise;
};


class Terrain
{
public:
	Terrain();
	~Terrain();

	std::vector <TerrainLayer> m_layers;

	std::string& getCLCode();

};

#endif
