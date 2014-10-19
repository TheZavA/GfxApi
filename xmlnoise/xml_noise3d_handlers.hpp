#ifndef XML_NOISE3D_HANDLERS_HPP
#define XML_NOISE3D_HANDLERS_HPP


#include "xml_noise_decls.hpp"


//example of noise generating element
module_ptr_t handle_perlin3d(noisepp::Pipeline3D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules);

module_ptr_t handle_y3d(noisepp::Pipeline3D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules);

module_ptr_t handle_billow3d(noisepp::Pipeline3D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules);

module_ptr_t handle_voronoi3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_ridged_multi3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);





module_ptr_t handle_clamp3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_const3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_invert3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_inverse_mul3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

//example of multi-source element
module_ptr_t handle_addition3d(noisepp::Pipeline3D& pipeline,
                               const tinyxml2::XMLElement& element,
                               const child_modules_t& child_modules);

module_ptr_t handle_blend3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_select3d(noisepp::Pipeline3D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);


//http://www.melior-ls.com/nppdoxy/classnoisepp_1_1_module.html for more modules to implement!

void register_all_3dhandlers(handlers3d_t& handlers);

#endif
