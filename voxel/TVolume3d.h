#ifndef TVOLUME3D_H
#define TVOLUME3D_H

#include <vector>
#include <memory>
#include <boost/scoped_array.hpp>

static inline size_t toindex(size_t x, size_t y, size_t z,
                            size_t x_length, size_t y_length, size_t z_length)
{
    return y * x_length + z * z_length + x;
}


template<class T>
class TVolume3d
{

public:
    TVolume3d(std::size_t xSize, std::size_t ySize, std::size_t zSize);
    ~TVolume3d();

    void clear();

    const std::size_t toIndex(std::size_t x, std::size_t y, std::size_t z) const;
    void fromIndex(std::size_t &x, std::size_t &y, std::size_t &z, std::size_t index) const;

    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;

    T& operator()(std::size_t x, std::size_t y, std::size_t z);
    const T& operator()(std::size_t x, std::size_t y, std::size_t z) const;

    std::size_t m_xSize;
    std::size_t m_ySize;
    std::size_t m_zSize;

    std::size_t m_xzSize;
    std::size_t m_xyzSize;

    boost::scoped_array<T> m_data;

    void upsample_yzx(TVolume3d<T>& vout)
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
TVolume3d<T>::TVolume3d(std::size_t xSize, std::size_t ySize, std::size_t zSize) 
{
     m_xSize = xSize;
     m_ySize = ySize;
     m_zSize = zSize;

     m_xzSize = xSize * zSize;

     m_xyzSize = xSize * ySize * zSize;

    
     m_data.reset(new T[ m_xyzSize]);
}


template<class T>
TVolume3d<T>::~TVolume3d() 
{

}

template<class T>
void TVolume3d<T>::clear()
{
    memset(m_data.get(), 0, m_xyzSize * sizeof(T));
}


template<class T>
__inline const std::size_t TVolume3d<T>::toIndex(std::size_t x, std::size_t y, std::size_t z) const 
{
   std::size_t index = (y *  m_xzSize) + (z *  m_xSize) + x;
   assert(index <  m_xyzSize);
   return index;
}


template<class T>
__inline void TVolume3d<T>::fromIndex(std::size_t &x, std::size_t &y, std::size_t &z, std::size_t index) const 
{
    z = (index /  m_zSize) %  m_ySize;
    y = index / ( m_zSize *  m_zSize);
    x = index %  m_zSize;

    assert(toIndex(x, y, z) == index);
}


template<class T>
__inline T& TVolume3d<T>::operator[](std::size_t index)
{
    assert(index <  m_xyzSize);
    
    return  m_data[index];
}

template<class T>
__inline const T& TVolume3d<T>::operator[](std::size_t index) const 
{
    assert(index <  m_xyzSize);
    return  m_data[index];
}


template<class T>
__inline T& TVolume3d<T>::operator()(std::size_t x, std::size_t y, std::size_t z) 
{
    std::size_t index = toIndex(x, y, z);
    assert(index <  m_xyzSize);


    return  m_data[index];

}


template<class T>
__inline const T& TVolume3d<T>::operator()(std::size_t x, std::size_t y, std::size_t z) const 
{
    std::size_t index = toIndex(x, y, z);
    assert(index <  m_xyzSize);

    return  m_data[index];
}

#endif