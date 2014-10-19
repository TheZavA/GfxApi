#include "xml_noise_handlers.hpp"

#include "tinyxml2.h"

#include <boost/bind.hpp>

void check_name_and_size(const std::string& modulename,
                         const tinyxml2::XMLElement& element,
                         const child_modules_t& child_modules,
                         std::size_t expected_children)
{
    
    std::string name = element.Name();

    if (name != modulename)
        throw xml_noise_error_t(std::string("expected "+ modulename + " element, got '") + name + "' instead");

    if (child_modules.size() != expected_children)
        throw xml_noise_wrong_argsize_error_t(modulename, expected_children, child_modules.size());
    
}

bool get_attribute_handle_result(const tinyxml2::XMLElement& element,
                                 const char* attribute,
                                 bool optional,
                                 int err_code)
{
    using namespace tinyxml2;
    switch(err_code)
    {
        case(XML_NO_ERROR): {
            return true;
        } case(XML_WRONG_ATTRIBUTE_TYPE):{
            const char* attribute_value = element.Attribute(attribute);

            assert(attribute_value);

            throw xml_noise_invalid_attribute_value_error_t(element.Name(), attribute, attribute_value);
        } case(XML_NO_ATTRIBUTE): {
            if (optional)
                return false;
            throw xml_noise_invalid_attribute_value_error_t(element.Name(), attribute, "");
        } default: {
            assert(false);
        }
    }
    assert(false);
}

bool get_int_attribute(const tinyxml2::XMLElement& element,
                       const char* attribute,
                       bool optional,
                       int& value)
{
    int err_code = element.QueryIntAttribute(attribute, &value);
    
    return get_attribute_handle_result(element, attribute, optional, err_code);
}

bool get_double_attribute(const tinyxml2::XMLElement& element,
                          const char* attribute,
                          bool optional,
                          double& value)
{
    int err_code = element.QueryDoubleAttribute(attribute, &value);
    
    return get_attribute_handle_result(element, attribute, optional, err_code);
}

bool get_bool_attribute(const tinyxml2::XMLElement& element,
                          const char* attribute,
                          bool optional,
                          bool& value)
{
    int err_code = element.QueryBoolAttribute(attribute, &value);
    
    return get_attribute_handle_result(element, attribute, optional, err_code);
}

