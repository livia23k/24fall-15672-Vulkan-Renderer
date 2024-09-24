#pragma once

#include "Source/Tools/SceneMgr.hpp"
#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "lib/sejp.hpp"

#include <string>
#include <vector>

struct LoadMgr {
    
    LoadMgr() = default;
    ~LoadMgr() = default;
    LoadMgr(LoadMgr const &) = delete;

    // .OBJ
    static void load_line_from_OBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
    static void load_object_from_OBJ(const std::string& path, std::vector<PosNorTexVertex>& mesh_vertices);

    // .s72
    static void load_objects_from_s72(const std::string& path, SceneMgr &scene_mgr);

};