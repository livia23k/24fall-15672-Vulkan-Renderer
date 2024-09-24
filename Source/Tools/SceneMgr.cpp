#include "Source/Tools/SceneMgr.hpp"

SceneMgr::SceneMgr()
{
    sceneObject = nullptr;
    nodeObjectMap.clear();
    meshObjectMap.clear();
    cameraObjectMap.clear();
    driverObjectMap.clear();
    materialObjectMap.clear();
}

SceneMgr::~SceneMgr()
{
    this->clean_all();
}

void SceneMgr::clean_all()
{
    if (sceneObject)
    {
        delete sceneObject;
        sceneObject = nullptr;
    }

    for (auto &pair : nodeObjectMap) {
        delete pair.second;
    }
    nodeObjectMap.clear();

    for (auto &pair : meshObjectMap) {
        delete pair.second;
    }
    meshObjectMap.clear();

    for (auto &pair : cameraObjectMap) {
        delete pair.second;
    }
    cameraObjectMap.clear();

    for (auto &pair : driverObjectMap) {
        delete pair.second;
    }
    driverObjectMap.clear();

    for (auto &pair : materialObjectMap) {
        delete pair.second;
    }
    materialObjectMap.clear();
}