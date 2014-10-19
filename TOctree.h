#ifndef _TOCTREE_H
#define _TOCTREE_H

#include <boost/scoped_ptr.hpp>
#include <boost/array.hpp>
#include "cubelib/cube.hpp"

template<class T>
class TOctree
{
public:
    typedef TOctree<T> ChildType;

    typedef boost::array<boost::scoped_ptr<ChildType>, 8> ChildrenType;

    TOctree(TOctree* root, 
            TOctree* parent, 
            T value, 
            std::size_t level, 
            const cube::corner_t& corner)
        : m_pRoot(root)
        , m_pParent(parent)
        , m_value(value)
        , m_level(level)
        , m_corner(corner)

    {

    }

    ~TOctree()
    {

    }

    T& getValue()
    {
        return m_value;
    }

    T getValueCopy()
    {
        return m_value;
    }

    const T& getValue() const
    {
        return m_value;
    }

    TOctree* getRoot()
    {
        return m_pRoot;
    }

    const TOctree* getRoot() const
    {
        return m_pRoot;
    }

    bool isRoot() const
    {
        return (m_pParent == nullptr);
    }

    bool hasChildren() const
    {
        return (m_children != nullptr);
    }

    bool isChild() const
    {
        return (m_pParent != nullptr);
    }

    bool isChildOf(const TOctree& other) const;

    bool isParentOf(const TOctree& other) const;

    const TOctree<T>* getPreviousBrother() const;

    const TOctree<T>* getNextBrother() const;

    const TOctree<T>* getFirstChild() const;

    const TOctree<T>* getLastChild() const;

    TOctree<T>* getParent()
    {
        return m_pParent;
    }

    const TOctree<T>* getParent() const
    {
        return m_pParent;
    }

    const cube::corner_t& getCorner() const
    {
        return m_corner;
    }

    std::size_t getLevel() const
    {
        return m_level;
    }

    ChildType& getChild(const cube::corner_t& corner)
    {
        return *(*m_children)[corner.index()].get();
    }

    ChildrenType& getChildren()
    {
        return (*m_children);
    }

    void split()
    {
        TOctree<T>* pRoot = (m_pParent == nullptr) ? this : m_pRoot; 

        m_children.reset( new ChildrenType() );

        for(auto& corner : cube::corner_t::all())
        {
            (*m_children)[corner.index()].reset(new ChildType(pRoot, this, T(), m_level + 1, corner));
        }
    }

    void join()
    {
        m_children.reset();
    }

    boost::scoped_ptr< ChildrenType > m_children;

private:
    TOctree<T>* m_pRoot;
    TOctree<T>* m_pParent;

    T m_value;

    std::size_t m_level;

    cube::corner_t m_corner;




    
};





#endif