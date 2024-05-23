#pragma once

#include <boost/property_tree/ptree.hpp>
#include <iostream>

template <class T>
T GetValueFromPTree(const boost::property_tree::ptree& ptree,
                    const std::string& key) {
  if (boost::optional<T> value = ptree.get_optional<T>(key)) {
    return value.get();
  } else {
    std::string error_message = "JSON does not contain " + key + " data";
    std::cerr << error_message << std::endl;
    abort();
  }
}
