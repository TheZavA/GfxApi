#include "xml_noise_handlers.hpp"
#include "xml_noise3d_handlers.hpp"

#include "tinyxml2.h"

#include <boost/bind.hpp>


void handle_perlinbase(const std::string& modulename,
                       noisepp::PerlinModuleBase& module,
                       noisepp::Pipeline2D& pipeline,
                       const tinyxml2::XMLElement& element,
                       const child_modules_t& child_modules)
{

    check_name_and_size(modulename, element, child_modules,0);

    //Got all these attributes from http://www.melior-ls.com/nppdoxy/classnoisepp_1_1_perlin_module_base.html

    set_frequency_shortcut(module,element);
    set_octaves_shortcut(module,element);
    set_seed_shortcut(module,element);
    set_lacunarity_shortcut(module,element);
    set_persistence_shortcut(module,element);
    set_scale_shortcut(module,element);
    set_quality_shortcut(module,element);
    




    //TODO: I am not sure if *every* module needs to be added to the pipeline, or only the root.
    // Check noise++ source for hints. If we do need to add each one to the pipeline,
    // we need to return the element ids as well as the module pointer. Alternatively, we can
    // just have the user add everything to the pipeline oneself, by using the ptr_list that
    // keeps all the modules alive. For now, do nothing.

    //noisepp::ElementID id = module.addToPipeline(&pipeline);


}


module_ptr_t handle_const2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules) {

    //For exception safety.
    std::auto_ptr<noisepp::ConstantModule> module_ptr( new noisepp::ConstantModule() );

    noisepp::ConstantModule& module = *module_ptr;

    check_name_and_size("const", element, child_modules, 0);

    set_value_shortcut(module,element);
   
    return module_ptr.release();

}

module_ptr_t handle_clamp2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules)
{
    //For exception safety.
    std::auto_ptr<noisepp::ClampModule> module_ptr( new noisepp::ClampModule() );

    noisepp::ClampModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));

    check_name_and_size("clamp", element, child_modules, 1);

    module.setSourceModule(0, child_modules.at(0));

    set_lower_bounds_shortcut(module,element);
    set_upper_bounds_shortcut(module,element);
    
    return module_ptr.release();
}

module_ptr_t handle_invert2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules) 
{
     //For exception safety.
    std::auto_ptr<noisepp::InvertModule> module_ptr( new noisepp::InvertModule() );

    noisepp::InvertModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));

    check_name_and_size("invert", element, child_modules, 1);

    module.setSourceModule(0, child_modules.at(0));

    return module_ptr.release();
}

module_ptr_t handle_inverse_mul2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules)
{                              
     //For exception safety.
    std::auto_ptr<noisepp::InverseMulModule> module_ptr( new noisepp::InverseMulModule() );

    noisepp::InverseMulModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));

    check_name_and_size("inverse-mul", element, child_modules, 1);

    module.setSourceModule(0, child_modules.at(0));

    return module_ptr.release();
}

module_ptr_t handle_perlin2d(noisepp::Pipeline2D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules)
{
    //For exception safety.
    std::auto_ptr<noisepp::PerlinModule> module( new noisepp::PerlinModule() );
    
    handle_perlinbase("perlin", *module, pipeline, element, child_modules);
    
    return module.release();
}

module_ptr_t handle_billow2d(noisepp::Pipeline2D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules)
{
    //For exception safety.
    std::auto_ptr<noisepp::BillowModule> module_ptr( new noisepp::BillowModule() );
    
    handle_perlinbase("billow", *module_ptr, pipeline, element, child_modules);
    
    return module_ptr.release();
}

module_ptr_t handle_ridged_multi2d(noisepp::Pipeline2D& pipeline,
                             const tinyxml2::XMLElement& element,
                             const child_modules_t& child_modules)
{
    //For exception safety.
    std::auto_ptr<noisepp::BillowModule> module_ptr( new noisepp::BillowModule() );
    
    handle_perlinbase("ridged-multi", *module_ptr, pipeline, element, child_modules);
    
    return module_ptr.release();
}

module_ptr_t handle_voronoi2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules)
{
    
    check_name_and_size("voronoi", element, child_modules,0);
    
    std::auto_ptr<noisepp::VoronoiModule> module_ptr( new noisepp::VoronoiModule() );
    
    noisepp::VoronoiModule& module = *module_ptr;
    
    set_frequency_shortcut(module,element);
    set_displacement_shortcut(module,element);
    set_enable_distance_shortcut(module,element);

    return module_ptr.release();
}

module_ptr_t handle_addition2d(noisepp::Pipeline2D& pipeline,
                               const tinyxml2::XMLElement& element,
                               const child_modules_t& child_modules)
{


    check_name_and_size("addition", element, child_modules, 2);
    
    //For exception safety.
    std::auto_ptr<noisepp::AdditionModule> module_ptr( new noisepp::AdditionModule() );

    noisepp::AdditionModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));
    module.setSourceModule(1, child_modules.at(1));


    return module_ptr.release();
}

module_ptr_t handle_blend2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules)
{
    check_name_and_size("blend", element, child_modules, 3);
    
    //For exception safety.
    std::auto_ptr<noisepp::BlendModule> module_ptr( new noisepp::BlendModule() );

    noisepp::BlendModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));
    module.setSourceModule(1, child_modules.at(1));
    module.setControlModule(child_modules.at(2));

    return module_ptr.release();

}

module_ptr_t handle_select2d(noisepp::Pipeline2D& pipeline,
                              const tinyxml2::XMLElement& element,
                              const child_modules_t& child_modules)
{
    check_name_and_size("select", element, child_modules, 3);
    
    //For exception safety.
    std::auto_ptr<noisepp::SelectModule> module_ptr( new noisepp::SelectModule() );

    noisepp::SelectModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));
    module.setSourceModule(1, child_modules.at(1));
    module.setControlModule(child_modules.at(2));

    set_lower_bounds_shortcut(module,element);
    set_upper_bounds_shortcut(module,element);
    set_edge_falloff_shortcut(module,element);

    return module_ptr.release();

}


module_ptr_t handle_multiply2d(noisepp::Pipeline2D& pipeline,
                               const tinyxml2::XMLElement& element,
                               const child_modules_t& child_modules)
{


    check_name_and_size("multiply", element, child_modules, 2);
    
    //For exception safety.
    std::auto_ptr<noisepp::MultiplyModule> module_ptr( new noisepp::MultiplyModule() );

    noisepp::MultiplyModule& module = *module_ptr;

    module.setSourceModule(0, child_modules.at(0));
    module.setSourceModule(1, child_modules.at(1));


    return module_ptr.release();
}


void register_all_2dhandlers(handlers2d_t& handlers)
{
    handlers["perlin"] = boost::bind(handle_perlin2d, _1, _2, _3);
    handlers["billow"] = boost::bind(handle_billow2d, _1, _2, _3);
    handlers["voronoi"] = boost::bind(handle_voronoi2d, _1, _2, _3);
    handlers["ridged-multi"] = boost::bind(handle_ridged_multi2d, _1, _2, _3);

    handlers["clamp"] = boost::bind(handle_clamp2d, _1, _2, _3);


    handlers["const"] = boost::bind(handle_const2d, _1, _2, _3);
    handlers["invert"] = boost::bind(handle_invert2d, _1, _2, _3);
    handlers["inverse-mul"] = boost::bind(handle_inverse_mul2d, _1, _2, _3);

    handlers["addition"] = boost::bind(handle_addition2d, _1, _2, _3);
    handlers["multiply"] = boost::bind(handle_multiply2d, _1, _2, _3);
    handlers["blend"] = boost::bind(handle_blend2d, _1, _2, _3);
    handlers["select"] = boost::bind(handle_select2d, _1, _2, _3);

    //....

    //etc.

    //http://www.melior-ls.com/nppdoxy/classnoisepp_1_1_module.html for more modules to implement!
}
