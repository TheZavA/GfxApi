#ifndef XML_NOISE3D_DELCS_HPP
#define XML_NOISE3D_DELCS_HPP


#include "Noise.h"
#include "xml_noise_error.hpp"

#include <boost/function.hpp>
#include <exception>
#include <string>
#include "tinyxml2.h"

typedef noisepp::Module module_t;
typedef noisepp::Module* module_ptr_t;
typedef std::vector< module_ptr_t > child_modules_t;

typedef boost::function< module_ptr_t (noisepp::Pipeline3D& pipeline,
                                       const tinyxml2::XMLElement& element,
                                       const child_modules_t& child_modules) >
    tag_handler3d_t;


typedef std::map< std::string, tag_handler3d_t > handlers3d_t;

typedef boost::function< module_ptr_t (noisepp::Pipeline2D& pipeline,
                                       const tinyxml2::XMLElement& element,
                                       const child_modules_t& child_modules) >
    tag_handler2d_t;


typedef std::map< std::string, tag_handler2d_t > handlers2d_t;


#endif
