#ifndef XML_NOISE2D_HANDLERS_HPP
#define XML_NOISE2D_HANDLERS_HPP


#include "xml_noise_decls.hpp"


//example of noise generating element
module_ptr_t handle_perlin2d(noisepp::Pipeline2D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules);


module_ptr_t handle_billow2d(noisepp::Pipeline2D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules);

module_ptr_t handle_voronoi2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_ridged_multi2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);





module_ptr_t handle_clamp2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_const2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_invert2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_inverse_mul2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

//example of multi-source element
module_ptr_t handle_addition2d(noisepp::Pipeline2D& pipeline,
                               const tinyxml2::XMLElement& element,
                               const child_modules_t& child_modules);

module_ptr_t handle_blend2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);

module_ptr_t handle_select2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules);


//http://www.melior-ls.com/nppdoxy/classnoisepp_1_1_module.html for more modules to implement!

void register_all_2dhandlers(handlers2d_t& handlers);

#endif
