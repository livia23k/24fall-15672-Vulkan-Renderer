#include "Source/Tools/SceneMgr.hpp"

SceneMgr::SceneMgr()
{
    sceneMap.clear();
    nodeMap.clear();
    meshMap.clear();
    cameraMap.clear();
    driverMap.clear();
    materialMap.clear();
}

SceneMgr::~SceneMgr()
{
    this->clean_all();
}

void SceneMgr::clean_all()
{
    for (auto &pair : sceneMap) {
        delete pair.second;
    }
    sceneMap.clear();

    for (auto &pair : nodeMap) {
        delete pair.second;
    }
    nodeMap.clear();

    for (auto &pair : meshMap) {
        delete pair.second;
    }
    meshMap.clear();

    for (auto &pair : cameraMap) {
        delete pair.second;
    }
    cameraMap.clear();

    for (auto &pair : driverMap) {
        delete pair.second;
    }
    driverMap.clear();

    for (auto &pair : materialMap) {
        delete pair.second;
    }
    materialMap.clear();
}