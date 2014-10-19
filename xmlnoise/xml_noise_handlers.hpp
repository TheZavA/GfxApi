#ifndef XML_NOISE_HANDLERS_HPP
#define XML_NOISE_HANDLERS_HPP


#include "xml_noise_decls.hpp"

#include "tinyxml2.h"

#include <boost/bind.hpp>

void check_name_and_size(const std::string& modulename,
                         const tinyxml2::XMLElement& element,
                         const child_modules_t& child_modules,
                         std::size_t expected_children);

bool get_attribute_handle_result(const tinyxml2::XMLElement& element,
                                 const char* attribute,
                                 bool optional,
                                 int err_code);

bool get_int_attribute(const tinyxml2::XMLElement& element,
                       const char* attribute,
                       bool optional,
                       int& value);

bool get_double_attribute(const tinyxml2::XMLElement& element,
                          const char* attribute,
                          bool optional,
                          double& value);

bool get_bool_attribute(const tinyxml2::XMLElement& element,
                          const char* attribute,
                          bool optional,
                          bool& value);


template<typename T>
void set_frequency_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"frequency",true,value))
        module.setFrequency(value);
}

template<typename T>
void set_octaves_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    int value = 0;
    if (get_int_attribute(element,"octaves",true,value))
        module.setOctaveCount(value);
}

template<typename T>
void set_seed_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    int value = 0;
    if (get_int_attribute(element,"seed",true,value))
        module.setSeed(value);
}

template<typename T>
void set_lacunarity_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"lacunarity",true,value))
        module.setLacunarity(value);
}

template<typename T>
void set_persistence_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"persistence",true,value))
        module.setPersistence(value);
}

template<typename T>
void set_scale_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"scale",true,value))
        module.setScale(value);
}

template<typename T>
void set_displacement_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"displacement",true,value))
        module.setDisplacement(value);
}

template<typename T>
void set_enable_distance_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    bool value = 0;
    if (get_bool_attribute(element,"enable-distance",true,value))
        module.setEnableDistance(value);
}

template<typename T>
void set_lower_bounds_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"lower-bound",true,value))
        module.setLowerBound(value);
}

template<typename T>
void set_upper_bounds_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"upper-bound",true,value))
        module.setUpperBound(value);
}

template<typename T>
void set_value_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"value",true,value))
        module.setValue(value);
}

template<typename T>
void set_edge_falloff_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    double value = 0;
    if (get_double_attribute(element,"edge-falloff",true,value))
        module.setEdgeFalloff(value);
}



template<typename T>
void set_quality_shortcut(T& module, const tinyxml2::XMLElement& element)
{
    using namespace noisepp;
    const char* quality = element.Attribute("quality");

    if (quality)
    {
        std::string quality_std(quality);

        if (quality_std == "fast-low")
        {
            module.setQuality(NOISE_QUALITY_FAST_LOW);
        } else if (quality_std == "low")
        {
            module.setQuality(NOISE_QUALITY_LOW);
        } else if (quality_std == "fast-standard") {
            module.setQuality(NOISE_QUALITY_FAST_STD);
        } else if (quality_std == "standard") {
            module.setQuality(NOISE_QUALITY_STD);
        } else if (quality_std == "fast-high") {
            module.setQuality(NOISE_QUALITY_FAST_HIGH);
        } else if (quality_std == "high") {
            module.setQuality(NOISE_QUALITY_HIGH);
        } else {
            throw xml_noise_invalid_attribute_value_error_t(element.Name(), "quality", quality_std);
        }

    }
}




#endif
