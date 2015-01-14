#ifndef TVOLUME2D_H
#define TVOLUME2D_H

#include <vector>
#include <memory>
#include <boost/scoped_array.hpp>

static inline size_t toindex(size_t x, size_t y, size_t x_length, size_t y_length)
{
    return y * x_length + x;
}


template<class T>
class TVolume2d
{

public:
    TVolume2d(std::size_t xSize, std::size_t ySize);
    ~TVolume2d();

    const std::size_t toIndex(std::size_t x, std::size_t y) const;
    void fromIndex(std::size_t &x, std::size_t &y, std::size_t index) const;

    T& operator[](std::size_t index);
    const T& operator[](std::size_t index) const;

    T& operator()(std::size_t x, std::size_t y);
    const T& operator()(std::size_t x, std::size_t y) const;

    std::size_t m_xSize;
    std::size_t m_ySize;

    std::size_t m_xySize;

    boost::scoped_array<T> m_data;

    T* getDataPtr()
    {
        return m_data.get();
    }

    void upsample_yx(TVolume2d<T>& vout)
    {
        size_t vin_size = m_xSize * m_ySize;
        size_t x_length2 = m_xSize << 1;
        size_t y_length2 = m_ySize << 1;

        size_t vout_size = vin_size << 3;
    

        ///These are the eight indices around the lowest cube in the vout volume.
        ///Actually, I only include four, because +x is the cheapest operation in the
        /// long array, so it will be implicit.
        size_t base_x_out_indices[4] = {
            toindex(0,0,0,x_length2, y_length2),
            toindex(0,0,1,x_length2, y_length2),
            toindex(0,1,0,x_length2, y_length2),
            toindex(0,1,1,x_length2, y_length2)
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
            for (size_t x = 0; x < m_xSize; ++x, ++inidx)
            {
                assert(inidx == toindex(x,y,m_xSize,m_ySize));
                assert(outidx == toindex(x*2 + 0, y*2 + (outidxi&0x1), x_length2, y_length2));
                assert(inidx < vin_size);
                assert(outidx < vout_size);
            
                ///the -x corner 
                vout[outidx] = vout[outidx] + (*this)[inidx];
                outidx++;
            
                assert(outidx == toindex(x*2 + 1, y*2 + (outidxi&0x1), x_length2, y_length2));
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
TVolume2d<T>::TVolume2d(std::size_t xSize, std::size_t ySize) 
{
     m_xSize = xSize;
     m_ySize = ySize;

     m_xySize = xSize * ySize;

     m_data.reset(new T[ m_xySize]);
}


template<class T>
TVolume2d<T>::~TVolume2d() 
{

}


template<class T>
__inline const std::size_t TVolume2d<T>::toIndex(std::size_t x, std::size_t y) const 
{
   std::size_t index = (y * m_xSize) + x;
   assert(index <  m_xySize);
   return index;
}


template<class T>
__inline void TVolume2d<T>::fromIndex(std::size_t &x, std::size_t &y, std::size_t index) const 
{

    y = index / ( m_xSize *  m_ySize);
    x = index %  m_xSize;

    assert(toIndex(x, y) == index);
}


template<class T>
__inline T& TVolume2d<T>::operator[](std::size_t index)
{
    assert(index < m_xySize);
    
    return  m_data[index];
}


template<class T>
__inline const T& TVolume2d<T>::operator[](std::size_t index) const 
{
    assert(index <  m_xySize);
    return m_data[index];
}


template<class T>
__inline T& TVolume2d<T>::operator()(std::size_t x, std::size_t y) 
{
    std::size_t index = toIndex(x, y);
    assert(index <  m_xySize);

    return  m_data[index];

}


template<class T>
__inline const T& TVolume2d<T>::operator()(std::size_t x, std::size_t y) const 
{
    std::size_t index = toIndex(x, y);
    assert(index <  m_xySize);

    return  m_data[index];
}

#endif