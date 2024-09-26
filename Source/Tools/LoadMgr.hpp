#pragma once

#include "Source/Tools/SceneMgr.hpp"
#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "Source/DataType/MeshAttribute.hpp"
#include "lib/sejp.hpp"
#include <vulkan/utility/vk_format_utils.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

struct LoadMgr {
    
    LoadMgr() = default;
    ~LoadMgr() = default;
    LoadMgr(LoadMgr const &) = delete;

    using PropertyMap = std::map<std::string, sejp::value>;
    using OptionalPropertyMap = std::optional<std::map<std::string, sejp::value>>;

    // .OBJ
    static void load_line_from_OBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
    static void load_object_from_OBJ(const std::string& path, std::vector<MeshAttribute>& mesh_vertices);

    // .s72
    static void load_objects_from_s72(const std::string& path, SceneMgr &targetSceneMgr);
    static void parse_scene_graph_info(const sejp::value &sceneGraphInfo, SceneMgr &targetSceneMgr);
    static void parse_scene_object_info(OptionalPropertyMap &sceneObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_node_object_info(OptionalPropertyMap &nodeObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_mesh_object_info(OptionalPropertyMap &meshObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_camera_object_info(OptionalPropertyMap &cameraObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_driver_object_info(OptionalPropertyMap &driverObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_material_object_info(OptionalPropertyMap &materialObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_environment_object_info(OptionalPropertyMap &environmentObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_light_object_info(OptionalPropertyMap &lightObjectInfo, SceneMgr &targetSceneMgr);
    static void parse_sub_attribute_info(OptionalPropertyMap &subAttributeInfo, SceneMgr::AttributeStream &attrStream);


};