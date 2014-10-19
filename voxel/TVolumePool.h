#pragma once
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <boost/utility.hpp>



template<class T>
class TVolumePool
{
public:
    typedef std::function< std::shared_ptr<T>() > generator_f;

    TVolumePool(generator_f generatorFunc);
    ~TVolumePool();

    std::shared_ptr<T> pop();

    //we can pass this in by reference, instead of copying
    void push(const std::shared_ptr<T>& object);

    std::size_t size();

    std::size_t m_objectCount;
    std::size_t m_usedObjectCount;
	

protected:
    std::queue< std::shared_ptr<T> >	m_queue;
    std::mutex		m_mutex;
    //generator_f& m_generatorFunc;
    generator_f m_generatorFunc2;
};

template <class T>
TVolumePool<T>::TVolumePool(generator_f generatorFunc) 
    : m_generatorFunc2(generatorFunc), 
    m_usedObjectCount(0), 
    m_objectCount(0)
{

    m_generatorFunc2();
}

template <class T>
std::size_t TVolumePool<T>::size() 
{
    std::lock_guard<std::mutex> lock( m_mutex);
    return  m_queue.size();
}


template <class T>
TVolumePool<T>::~TVolumePool() 
{


}


template <class T>
std::shared_ptr<T> TVolumePool<T>::pop() 
{
	std::lock_guard<std::mutex> lock(m_mutex);
    m_usedObjectCount++;
	if( m_queue.empty())
	{
        m_objectCount++;
        auto& object = m_generatorFunc2();
		return object;
	}

	std::shared_ptr<T> result =  m_queue.front();

	m_queue.pop();

	return result;
}

template <class T>
void TVolumePool<T>::push(const std::shared_ptr<T>& object) 
{

	std::lock_guard<std::mutex> lock( m_mutex);
    m_usedObjectCount--;
	m_queue.push(object);
}

