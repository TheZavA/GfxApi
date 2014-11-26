#ifndef TBORDERVOLUME3D_H
#define TBORDERVOLUME3D_H

#include <vector>
#include <memory>
#include <boost/scoped_array.hpp>

template<class T>
class TBorderVolume3d
{

public:
    TBorderVolume3d(std::size_t xSize, std::size_t ySize, std::size_t zSize, std::size_t bSize);
    ~TBorderVolume3d();

    void clear();

    const std::size_t toIndex(int32_t x, int32_t y, int32_t z) const;

    T& operator()(int32_t x, int32_t y, int32_t z);
    const T& operator()(int32_t x, int32_t y, int32_t z) const;

    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;

    int32_t m_xSize;
    int32_t m_ySize;
    int32_t m_zSize;

    int32_t m_xMin;
    int32_t m_yMin;
    int32_t m_zMin;

    int32_t m_xMax;
    int32_t m_yMax;
    int32_t m_zMax;

    std::size_t m_xSizeInner;
    std::size_t m_ySizeInner;
    std::size_t m_zSizeInner;

    // including border size
    std::size_t m_xzSize;
    std::size_t m_xyzSize;

    std::size_t m_bSize;

    boost::scoped_array<T> m_data;

    T* getDataPtr()
    {
        return m_data.get();
    }

    void upsample_yzx(TBorderVolume3d<T>& vout)
    {
        size_t vin_size = m_xSize * m_ySize * m_zSize;
        size_t x_length2 = m_xSize << 1;
        size_t y_length2 = m_ySize << 1;
        size_t z_length2 = m_zSize << 1;
        size_t vout_size = vin_size << 3;
    

        ///These are the eight indices around the lowest cube in the vout volume.
        ///Actually, I only include four, because +x is the cheapest operation in the
        /// long array, so it will be implicit.
        size_t base_x_out_indices[4] = {
            toindex(0,0,0,x_length2, y_length2, z_length2),
            toindex(0,0,1,x_length2, y_length2, z_length2),
            toindex(0,1,0,x_length2, y_length2, z_length2),
            toindex(0,1,1,x_length2, y_length2, z_length2)
        };
    
        for(size_t i = 0; i < vout_size; i++) 
        {
            vout[i] = 0;
        }
    
        ///We will go through the volume 8 times, once for each corner value
        /// of the high resolution cube. Each time, we will contribute from
        /// the low resolution value at the center of the cube, to a different
        /// corner, and ultimately, the entire cube. Note how we only seem
        /// to be doing 4 corners of the cube, not 8. This is because the +x
        /// corner of the cube is "easy" to get to from the 4 lower corners,
        /// because +x is cheapest to stride along in a long-array format
        /// for volume. Thus, 4 corners are implicit, and we will not need to
        /// run around the out volume 8 times.
        for (size_t outidxi = 0; outidxi < 4; ++outidxi)
        {
            size_t outidx = base_x_out_indices[ outidxi ];
        
            size_t inidx = 0;
            for (size_t y = 0; y < m_ySize; ++y)
            for (size_t z = 0; z < m_zSize; ++z)
            for (size_t x = 0; x < m_xSize; ++x, ++inidx)
            {
                assert(inidx == toindex(x,y,z,m_xSize,m_ySize,m_zSize));
                assert(outidx == toindex(x*2 + 0,y*2 + (outidxi&0x1),z*2 + (outidxi&0x2),x_length2,y_length2,z_length2));
                assert(inidx < vin_size);
                assert(outidx < vout_size);
            
                ///the -x corner 
                vout[outidx] = vout[outidx] + (*this)[inidx];
                outidx++;
            
                assert(outidx == toindex(x*2 + 1,y*2 + (outidxi&0x1),z*2 + (outidxi&0x2),x_length2,y_length2,z_length2));
                assert(outidx < vout_size);
            
                ///the +x corner
                vout[outidx] = vout[outidx] + (*this)[inidx];
                outidx++;
            }
        }
    
        for (size_t outidx = 0; outidx < vout_size; ++outidx)
        {
           // vout[outidx] = vout[outidx] << 3;
        }
    }

};


template<class T>
TBorderVolume3d<T>::TBorderVolume3d(std::size_t xSize, std::size_t ySize, std::size_t zSize, std::size_t bSize) 
{
    m_bSize = bSize;
    m_xSize = xSize + (bSize * 2);
    m_ySize = ySize + (bSize * 2);
    m_zSize = zSize + (bSize * 2);

    m_xMin = -(int)bSize;
    m_yMin = -(int)bSize;
    m_zMin = -(int)bSize;

    m_xMax = xSize + bSize;
    m_yMax = ySize + bSize;
    m_zMax = zSize + bSize;

    m_xSizeInner = xSize;
    m_ySizeInner = ySize;
    m_zSizeInner = zSize;

    m_xzSize = (m_xSize) * (m_zSize);

    m_xyzSize = (m_xSize) * (m_ySize) * (m_zSize);

    
    m_data.reset(new T[ m_xyzSize]);
}


template<class T>
TBorderVolume3d<T>::~TBorderVolume3d() 
{

}

template<class T>
void TBorderVolume3d<T>::clear()
{
    memset(m_data.get(), 0, m_xyzSize * sizeof(T));
}


template<class T>
__inline const std::size_t TBorderVolume3d<T>::toIndex(int32_t x, int32_t y, int32_t z) const 
{
    assert((x + m_bSize) >= 0);
    assert((x + m_bSize) < m_xSize);

    assert((y + m_bSize) >= 0);
    assert((y + m_bSize) < m_ySize);

    assert((z + m_bSize) >= 0);
    assert((z + m_bSize) < m_zSize);

    std::size_t index = ((y + m_bSize) *  m_xzSize) + ((z + m_bSize) *  m_xSize) + (x + m_bSize);
    assert(index < m_xyzSize);
    return index;
}

template<class T>
__inline T& TBorderVolume3d<T>::operator[](std::size_t index)
{
    assert(index <  m_xyzSize);
    
    return  m_data[index];
}

template<class T>
__inline const T& TBorderVolume3d<T>::operator[](std::size_t index) const 
{
    assert(index <  m_xyzSize);
    return  m_data[index];
}

template<class T>
__inline T& TBorderVolume3d<T>::operator()(int32_t x, int32_t y, int32_t z) 
{
    std::size_t index = toIndex(x, y, z);
    assert(index <  m_xyzSize);


    return  m_data[index];

}


template<class T>
__inline const T& TBorderVolume3d<T>::operator()(int32_t x, int32_t y, int32_t z) const 
{
    std::size_t index = toIndex(x, y, z);
    assert(index <  m_xyzSize);

    return  m_data[index];
}

#endif