#pragma once

#include "Source/Tools/SceneMgr.hpp"
#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "Source/DataType/MeshAttribute.hpp"
#include "lib/sejp.hpp"
#include "Source/VkMemory/Helpers.hpp"

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

    // ===============================
    // .OBJ 
    static void load_line_from_OBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
    static void load_object_from_OBJ(const std::string& path, std::vector<MeshAttribute>& mesh_vertices);

    // ===============================
    // .s72

    // load scene graph info
    static void load_scene_graph_info_from_s72(const std::string& path, SceneMgr &targetSceneMgr);
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

    // load mesh
    template <typename T>
    static void read_s72_mesh_attribute_to_list(std::vector<T> &targetList, SceneMgr::AttributeStream &attrStream, std::string srcFolder);

    // load matrices
    static void load_s72_node_matrices(SceneMgr &targetSceneMgr);

    // load materials
    static void load_texture_from_file(unsigned char *&dst, const char *src,  int &w, int &h, const int &desired_channels);
    static void load_cubemap_from_file(unsigned char **dst, const char *src, int &w, int &h, int &orgChannel, const int &desiredChannel, const int &NUM_CUBE_FACES, bool flip);
    static void save_cubemap_faces_as_images(unsigned char **dst, int face_w, int face_h, int desired_channels);
    static void rotate_cubemap_face_by_90_cw(unsigned char *face, const int &w, const int &h, const int &channels);
    static void rotate_cubemap_face_by_90_ccw(unsigned char *face, const int &w, const int &h, const int &channels);
    static void horizontal_flip_cubemap_face(unsigned char *face, const int &w, const int &h, const int &channels);
    static void vertical_flip_cubemap_face(unsigned char *face, const int &w, const int &h, const int &channels);


};