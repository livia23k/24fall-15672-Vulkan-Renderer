#pragma once

#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"

#include <string>
#include <vector>

struct LoadMgr {
    
    LoadMgr() = default;
    ~LoadMgr() = default;
    LoadMgr(LoadMgr const &) = delete;

    // .OBJ
    static void load_line_from_OBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
    static void load_object_from_OBJ(const std::string& path, std::vector<PosNorTexVertex>& mesh_vertices);

    // .M

};