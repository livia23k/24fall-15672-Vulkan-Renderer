#pragma once

#include "datastructures/PosColVertex.hpp"

#include <string>
#include <vector>

struct FileMgr {
    
    FileMgr() = default;
    ~FileMgr() = default;
    FileMgr(FileMgr const &) = delete;

    static void loadOBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices);
};