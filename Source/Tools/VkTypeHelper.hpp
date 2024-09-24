#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <string>
#include <optional>

namespace VkTypeHelper 
{
    extern std::unordered_map<std::string, VkPrimitiveTopology> primitiveTopologyMap;
    extern std::unordered_map<std::string, VkIndexType> indexTypeMap;
    extern std::unordered_map<std::string, VkFormat> formatMap;

    std::optional<VkPrimitiveTopology> findVkPrimitiveTopology(const std::string& topologyStr);
    std::optional<VkIndexType> findVkIndexType(const std::string& indexTypeStr);
    std::optional<VkFormat> findVkFormat(const std::string& formatStr);
}
