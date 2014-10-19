#ifndef XML_NOISE3D_ERROR_HPP
#define XML_NOISE3D_ERROR_HPP


#include "Noise.h"

#include <boost/function.hpp>
#include <exception>
#include <string>
#include "tinyxml2.h"


struct xml_noise_error_t : std::runtime_error
{
    explicit xml_noise_error_t(const std::string& what_arg)
        : std::runtime_error(what_arg)
    {}
    virtual ~xml_noise_error_t() throw() {}
};

struct xml_noise_unknown_handler_error_t : xml_noise_error_t
{
    explicit xml_noise_unknown_handler_error_t(const std::string& handler)
        : xml_noise_error_t(std::string("Unknown handler: ") + handler)
    {}
    
    virtual ~xml_noise_unknown_handler_error_t() throw() {}
};

struct xml_noise_wrong_argsize_error_t : xml_noise_error_t
{
    explicit xml_noise_wrong_argsize_error_t(const std::string& element_name, std::size_t expected, std::size_t got)
        : xml_noise_error_t(std::string("Wrong number of arguments for ") + element_name)
        , element_name(element_name)
        , expected(expected)
        , got(got)
    {}

    virtual ~xml_noise_wrong_argsize_error_t() throw() {}

    std::string element_name;
    std::size_t expected;
    std::size_t got;
};

struct xml_noise_duplicate_id_error_t : xml_noise_error_t
{
    explicit xml_noise_duplicate_id_error_t(const std::string& element_name, const std::string& element_id)
        : xml_noise_error_t(std::string("Element ") + element_name
                            + " with id, '" + element_id
                            + "' has a duplicate id")
        , element_name(element_name)
        , element_id(element_id)
    {}
    
    virtual ~xml_noise_duplicate_id_error_t() throw() {}

    std::string element_name;
    std::string element_id;
};

struct xml_noise_invalid_attribute_value_error_t : xml_noise_error_t
{
    explicit xml_noise_invalid_attribute_value_error_t(const std::string& element_name, const std::string& attribute, const std::string& value)
        : xml_noise_error_t(std::string("Element ")
                            + element_name
                            +" has invalid attribute value: '" + attribute
                            + "'='" + value + "'")
        , element_name(element_name)
        , attribute(attribute)
        , value(value)
    {}

    virtual ~xml_noise_invalid_attribute_value_error_t() throw() {}
    
    std::string element_name;
    std::string attribute;
    std::string value;
};

#endif
