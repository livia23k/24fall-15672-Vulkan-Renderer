#pragma once

#include "datastructures/PosColVertex.hpp"
#include "datastructures/PosNorTexVertex.hpp"

#include <string>
#include <vector>

struct FileMgr {
    
    FileMgr() = default;
    ~FileMgr() = default;
    FileMgr(FileMgr const &) = delete;

    static void load_line_from_object(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
    static void load_mesh_from_object(const std::string& path, std::vector<PosNorTexVertex>& mesh_vertices);
};