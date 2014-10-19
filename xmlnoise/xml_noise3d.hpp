#ifndef XML_NOISE3D_HPP
#define XML_NOISE3D_HPP

#include <string>


#include "xml_noise_decls.hpp"

#include <boost/noncopyable.hpp>

#include <boost/ptr_container/ptr_list.hpp>
#include <stack>
#include <map>

struct xml_noise3d_t
    : boost::noncopyable
{
    xml_noise3d_t(noisepp::Pipeline3D& pipeline);
    ~xml_noise3d_t();

    void load(const std::string& xml_file_name);
    
    //Simple recursive version, for testing. Broken ATM.
    void simple_load(const std::string& xml_file_name);

    handlers3d_t handlers;

    noisepp::Pipeline3D& pipeline;
    module_ptr_t root;
    boost::ptr_list<module_t> module_ptrs;

    std::map<std::string, module_ptr_t> special_nodes;
    
private:
    void visit(const tinyxml2::XMLElement& element);
    std::stack< module_ptr_t > argument_stack;
    
    
    
    module_ptr_t simple_traverse(const tinyxml2::XMLElement& element);
    
};



#endif
