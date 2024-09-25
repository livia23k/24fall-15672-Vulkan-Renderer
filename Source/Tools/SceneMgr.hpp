#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>

struct SceneMgr
{
    SceneMgr();
	~SceneMgr();
	SceneMgr(SceneMgr const &) = delete;

    // Enums

    enum TextureType
    {
        cube,
    };

    enum TextureFormat
    {
        rgbe,
    };

    enum DriverChannleType
    {
        TRANSLATION,
        SCALE,
        ROTATION,
    };

    enum DriverInterpolation
    {
        STEP,
        LINEAR,
        SLERP,
    };

    enum ProjectionType {
        Perspective,
        Orthographic
    };

    enum MaterialType {
        PBR,
        LAMBERTION,
        MIRROR,
        ENVIRONMENT,
    };

    // Basic types

    struct Texture
    {
        std::string src;
        TextureType type;
        TextureFormat format;
    };

    struct IndiceStream
    {
        std::string src;
        uint32_t offset;
        VkIndexType format;
    };

    struct AttributeStream
    {
        std::string src;
        uint32_t offset;
        uint32_t stride;
        VkFormat format;
    };

    struct PerspectiveParameters {
        float aspect;
        float vfov;
        float nearZ;
        float farZ;
    };

    struct OrthographicParameters {
        float left;
        float right;
        float bottom;
        float top;
        float nearZ;
        float farZ;
    };

    using AlbedoParam = std::variant<glm::vec3, Texture>;
    using RoughnessParam = std::variant<float, Texture>;
    using MetalnessParam = std::variant<float, Texture>;

    struct PBRMaterial {
        AlbedoParam albedo;
        RoughnessParam roughness;
        MetalnessParam metalness;

        PBRMaterial() :
            albedo(glm::vec3(1.0f)),
            roughness(0.5f),
            metalness(0.5f) {}

        PBRMaterial(glm::vec3 albedo, float roughness, float metalness) :
            albedo(albedo),
            roughness(roughness),
            metalness(metalness) {}
    };

    struct LambertionMaterial {
        AlbedoParam albedo;

        LambertionMaterial() :
            albedo(glm::vec3(1.0f)) {}
        
        LambertionMaterial(glm::vec3 albedo) :
            albedo(albedo) {}
    };

    // Object types

    struct SceneObject 
    {
        std::string name;
        std::vector<uint32_t> rootIdx;
    };

    struct NodeObject
    {
        std::string name;

        glm::vec3 translation = glm::vec3(0, 0, 0);
        glm::vec3 scale = glm::vec3(1, 1, 1);
        glm::vec4 rotation = glm::vec4(0, 0, 0, 1);

        std::vector<std::string> childName;

        std::string refCameraName;
        std::string refMeshName;
        std::string refEnvironmentName;
        std::string refLightName;
    };

    struct MeshObject
    {
        std::string name;
        VkPrimitiveTopology topology;
        uint32_t count;
        IndiceStream indices;
        AttributeStream attrPosition;
        AttributeStream attrNormal;
        AttributeStream attrTangent;
        AttributeStream attrTexcoord;

        std::string refMaterialName;   
    };

    struct CameraObject
    {
        std::string name;
        ProjectionType projectionType;
        std::variant<PerspectiveParameters, OrthographicParameters> projectionParameters;
    };

    struct DriverObject
    {
        std::string name;
        std::string refObjectName; // target object
        DriverChannleType channel;
        uint32_t channelDim;
        std::vector<float> times;
        std::vector<float> values;
        DriverInterpolation interpolation = DriverInterpolation::LINEAR;

    };

    struct MaterialObject {
        std::string name;
        std::optional<Texture> normalmap;
        std::optional<Texture> displacementmap;
        MaterialType type;
        std::variant<std::monostate, PBRMaterial, LambertionMaterial> parameters;
    };

    struct EnvironmentObject
    {
        std::string name;
        Texture radiance;
    };

    struct LightObject
    {
        std::string name;
        glm::vec3 tint;

    };

    SceneObject* sceneObject;
    std::unordered_map<std::string, NodeObject*> nodeObjectMap;
    std::unordered_map<std::string, MeshObject*> meshObjectMap;
    std::unordered_map<std::string, CameraObject*> cameraObjectMap;
    std::unordered_map<std::string, DriverObject*> driverObjectMap;
    std::unordered_map<std::string, MaterialObject*> materialObjectMap;
    std::unordered_map<std::string, EnvironmentObject*> environmentObjectMap;
    std::unordered_map<std::string, LightObject*> lightObjectMap;

    // methods

    void clean_all();
};