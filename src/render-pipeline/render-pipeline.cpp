//
//  render-pipeline.cpp
//  LearnMetalCPP
//
//  Created by AJ Matheson-Lieber on 11/24/25.
// Based on "Learn Metal with C++" sample code @ https://developer.apple.com/metal/sample-code/
//  Copyright © 2025 Apple. All rights reserved.
//

#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <simd/simd.h>
#pragma region Declarations {
static constexpr size_t kInstanceRows = 10;
static constexpr size_t kInstanceColumns = 10;
static constexpr size_t kInstanceDepth = 10;
static constexpr size_t kNumInstances = (kInstanceRows * kInstanceColumns * kInstanceDepth);
static constexpr int kMaxFramesInFlight = 3;

namespace math
{
    constexpr simd::float3 add( const simd::float3& a, const simd::float3& b );
    constexpr simd_float4x4 makeIdentity();
    simd::float4x4 makePerspective();
    simd::float4x4 makeXRotate( float angleRadians );
    simd::float4x4 makeYRotate( float angleRadians );
    simd::float4x4 makeZRotate( float angleRadians );
    simd::float4x4 makeTranslate( const simd::float3& v );
    simd::float4x4 makeScale( const simd::float3& v );
    simd::float3x3 discardTranslation( const simd::float4x4& m );
}

namespace shader_types
{
    struct VertexData
    {
        simd::float3 position;
        simd::float3 normal;
        simd::float2 texcoord;
    };

    struct InstanceData
    {
        simd::float4x4 instanceTransform;
        simd::float3x3 instanceNormalTransform;
        simd::float4 instanceColor;
        uint32_t materialIndex;
    };

    struct CameraData
    {
        simd::float4x4 perspectiveTransform;
        simd::float4x4 worldTransform;
        simd::float3x3 worldNormalTransform;
        simd::float3 cameraPosition;
        simd::float3 cameraDirection;
    };

    struct MaterialData
    {
        simd::float3 diffuseColor;
        float reflectivity;
        simd::float3 specularColor;
        float shininess;
        simd::float3 emissiveColor;
        float _pad;
    };

    struct LightData
    {
        simd::float3 position;
        float intensity;
        simd::float3 color;
        float _pad;
    };
}

class SceneBuilder
{
public:
    SceneBuilder();
    ~SceneBuilder();

    struct Mesh
    {
        std::vector<shader_types::VertexData> vertices;
        std::vector<uint16_t> indices;
    };

    static Mesh importObjectFile(const std::string& path);

    void loadPreset();
    void setMesh(const Mesh& mesh);

    const Mesh& getMesh() const { return _mesh; }

private:
    Mesh _mesh;
};

class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void buildShaders();
        void buildDepthStencilStates();
        void buildBuffers();
        void draw( MTK::View* pView );

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
        MTL::Library* _pShaderLibrary;
        MTL::RenderPipelineState* _pPSO;
        MTL::DepthStencilState* _pDepthStencilState;
        MTL::Texture* _pTexture;
        MTL::Buffer* _pVertexDataBuffer;
        MTL::Buffer* _pInstanceDataBuffer[kMaxFramesInFlight];
        MTL::Buffer* _pCameraDataBuffer[kMaxFramesInFlight];
        MTL::Buffer* _pIndexBuffer;
        MTL::Buffer* _pMaterialBuffer;
        MTL::Buffer* _pLightBuffer;
        float _angle;
        int _frame;
        dispatch_semaphore_t _semaphore;
        static const int kMaxFramesInFlight;
        size_t _indexCount;
};

class MyMTKViewDelegate : public MTK::ViewDelegate
{
    public:
        MyMTKViewDelegate( MTL::Device* pDevice );
        virtual ~MyMTKViewDelegate() override;
        virtual void drawInMTKView( MTK::View* pView ) override;

    private:
        Renderer* _pRenderer;
};

class MyAppDelegate : public NS::ApplicationDelegate
{
    public:
        ~MyAppDelegate();

        NS::Menu* createMenuBar();

        virtual void applicationWillFinishLaunching( NS::Notification* pNotification ) override;
        virtual void applicationDidFinishLaunching( NS::Notification* pNotification ) override;
        virtual bool applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender ) override;

    private:
        NS::Window* _pWindow;
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        MyMTKViewDelegate* _pViewDelegate = nullptr;
};

#pragma endregion Declarations }


int main( int argc, char* argv[] )
{
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate( &del );
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}

#pragma mark - AppDelegate
#pragma region AppDelegate {

MyAppDelegate::~MyAppDelegate()
{
    _pMtkView->release();
    _pWindow->release();
    _pDevice->release();
    delete _pViewDelegate;
}

NS::Menu* MyAppDelegate::createMenuBar()
{
    using NS::StringEncoding::UTF8StringEncoding;

    NS::Menu* pMainMenu = NS::Menu::alloc()->init();
    NS::MenuItem* pAppMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* pAppMenu = NS::Menu::alloc()->init( NS::String::string( "Appname", UTF8StringEncoding ) );

    NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
    NS::String* quitItemName = NS::String::string( "Quit ", UTF8StringEncoding )->stringByAppendingString( appName );
    SEL quitCb = NS::MenuItem::registerActionCallback( "appQuit", [](void*,SEL,const NS::Object* pSender){
        auto pApp = NS::Application::sharedApplication();
        pApp->terminate( pSender );
    } );

    NS::MenuItem* pAppQuitItem = pAppMenu->addItem( quitItemName, quitCb, NS::String::string( "q", UTF8StringEncoding ) );
    pAppQuitItem->setKeyEquivalentModifierMask( NS::EventModifierFlagCommand );
    pAppMenuItem->setSubmenu( pAppMenu );

    NS::MenuItem* pWindowMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* pWindowMenu = NS::Menu::alloc()->init( NS::String::string( "Window", UTF8StringEncoding ) );

    SEL closeWindowCb = NS::MenuItem::registerActionCallback( "windowClose", [](void*, SEL, const NS::Object*){
        auto pApp = NS::Application::sharedApplication();
            pApp->windows()->object< NS::Window >(0)->close();
    } );
    NS::MenuItem* pCloseWindowItem = pWindowMenu->addItem( NS::String::string( "Close Window", UTF8StringEncoding ), closeWindowCb, NS::String::string( "w", UTF8StringEncoding ) );
    pCloseWindowItem->setKeyEquivalentModifierMask( NS::EventModifierFlagCommand );

    pWindowMenuItem->setSubmenu( pWindowMenu );

    pMainMenu->addItem( pAppMenuItem );
    pMainMenu->addItem( pWindowMenuItem );

    pAppMenuItem->release();
    pWindowMenuItem->release();
    pAppMenu->release();
    pWindowMenu->release();

    return pMainMenu->autorelease();
}

void MyAppDelegate::applicationWillFinishLaunching( NS::Notification* pNotification )
{
    NS::Menu* pMenu = createMenuBar();
    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->setMainMenu( pMenu );
    pApp->setActivationPolicy( NS::ActivationPolicy::ActivationPolicyRegular );
}

void MyAppDelegate::applicationDidFinishLaunching( NS::Notification* pNotification )
{
    CGRect frame = (CGRect){ {100.0, 100.0}, {1024.0, 1024.0} };

    _pWindow = NS::Window::alloc()->init(
        frame,
        NS::WindowStyleMaskClosable|NS::WindowStyleMaskTitled,
        NS::BackingStoreBuffered,
        false );

    _pDevice = MTL::CreateSystemDefaultDevice();

    _pMtkView = MTK::View::alloc()->init( frame, _pDevice );
    _pMtkView->setColorPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    _pMtkView->setClearColor( MTL::ClearColor::Make( 0.1, 0.1, 0.1, 1.0 ) );
    _pMtkView->setDepthStencilPixelFormat( MTL::PixelFormat::PixelFormatDepth16Unorm );
    _pMtkView->setClearDepth( 1.0f );

    _pViewDelegate = new MyMTKViewDelegate( _pDevice );
    _pMtkView->setDelegate( _pViewDelegate );

    _pWindow->setContentView( _pMtkView );
    _pWindow->setTitle( NS::String::string( "Cpp/Metal Custom Renderer", NS::StringEncoding::UTF8StringEncoding ) );

    _pWindow->makeKeyAndOrderFront( nullptr );

    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->activateIgnoringOtherApps( true );
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender )
{
    return true;
}

#pragma endregion AppDelegate }

#pragma mark - ViewDelegate
#pragma region ViewDelegate {

MyMTKViewDelegate::MyMTKViewDelegate( MTL::Device* pDevice )
: MTK::ViewDelegate()
, _pRenderer( new Renderer( pDevice ) )
{
}

MyMTKViewDelegate::~MyMTKViewDelegate()
{
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView( MTK::View* pView )
{
    _pRenderer->draw( pView );
}

#pragma endregion ViewDelegate }


#pragma mark - Math

namespace math
{
    constexpr simd::float3 add( const simd::float3& a, const simd::float3& b )
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    constexpr simd_float4x4 makeIdentity()
    {
        using simd::float4;
        return (simd_float4x4){ (float4){ 1.f, 0.f, 0.f, 0.f },
                                (float4){ 0.f, 1.f, 0.f, 0.f },
                                (float4){ 0.f, 0.f, 1.f, 0.f },
                                (float4){ 0.f, 0.f, 0.f, 1.f } };
    }

    simd::float4x4 makePerspective( float fovRadians, float aspect, float znear, float zfar )
    {
        using simd::float4;
        float ys = 1.f / tanf(fovRadians * 0.5f);
        float xs = ys / aspect;
        float zs = zfar / ( znear - zfar );
        return simd_matrix_from_rows((float4){ xs, 0.0f, 0.0f, 0.0f },
                                     (float4){ 0.0f, ys, 0.0f, 0.0f },
                                     (float4){ 0.0f, 0.0f, zs, znear * zs },
                                     (float4){ 0, 0, -1, 0 });
    }

    simd::float4x4 makeXRotate( float angleRadians )
    {
        using simd::float4;
        const float a = angleRadians;
        return simd_matrix_from_rows((float4){ 1.0f, 0.0f, 0.0f, 0.0f },
                                     (float4){ 0.0f, cosf( a ), sinf( a ), 0.0f },
                                     (float4){ 0.0f, -sinf( a ), cosf( a ), 0.0f },
                                     (float4){ 0.0f, 0.0f, 0.0f, 1.0f });
    }

    simd::float4x4 makeYRotate( float angleRadians )
    {
        using simd::float4;
        const float a = angleRadians;
        return simd_matrix_from_rows((float4){ cosf( a ), 0.0f, sinf( a ), 0.0f },
                                     (float4){ 0.0f, 1.0f, 0.0f, 0.0f },
                                     (float4){ -sinf( a ), 0.0f, cosf( a ), 0.0f },
                                     (float4){ 0.0f, 0.0f, 0.0f, 1.0f });
    }

    simd::float4x4 makeZRotate( float angleRadians )
    {
        using simd::float4;
        const float a = angleRadians;
        return simd_matrix_from_rows((float4){ cosf( a ), sinf( a ), 0.0f, 0.0f },
                                     (float4){ -sinf( a ), cosf( a ), 0.0f, 0.0f },
                                     (float4){ 0.0f, 0.0f, 1.0f, 0.0f },
                                     (float4){ 0.0f, 0.0f, 0.0f, 1.0f });
    }

    simd::float4x4 makeTranslate( const simd::float3& v )
    {
        using simd::float4;
        const float4 col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
        const float4 col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
        const float4 col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
        const float4 col3 = { v.x, v.y, v.z, 1.0f };
        return simd_matrix( col0, col1, col2, col3 );
    }

    simd::float4x4 makeScale( const simd::float3& v )
    {
        using simd::float4;
        return simd_matrix((float4){ v.x, 0, 0, 0 },
                           (float4){ 0, v.y, 0, 0 },
                           (float4){ 0, 0, v.z, 0 },
                           (float4){ 0, 0, 0, 1.0 });
    }

    simd::float3x3 discardTranslation( const simd::float4x4& m )
    {
        return simd_matrix( m.columns[0].xyz, m.columns[1].xyz, m.columns[2].xyz );
    }

}

#pragma mark - SceneBuilder
#pragma region SceneBuilder {
SceneBuilder::SceneBuilder() = default;
SceneBuilder::~SceneBuilder() = default;

void SceneBuilder::setMesh(const Mesh& mesh)
{
    _mesh = mesh;
}
static void parseFaceVertex(const std::string& token,
                            int& vIdx, int& vtIdx, int& vnIdx)
{
    vIdx = vtIdx = vnIdx = -1;

    std::stringstream ss(token);
    std::string part;
    int i = 0;

    while (std::getline(ss, part, '/'))
    {
        if (!part.empty())
        {
            int val = std::stoi(part) - 1; // OBJ is 1-based
            if (i == 0)      vIdx  = val;
            else if (i == 1) vtIdx = val;
            else if (i == 2) vnIdx = val;
        }
        ++i;
    }
}

SceneBuilder::Mesh SceneBuilder::importObjectFile(const std::string& path)
{
    using simd::float3;
    using simd::float2;

    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Failed to open OBJ file at: " + path);

    std::vector<float3> positions;
    std::vector<float3> normals;
    std::vector<float2> texcoords;

    Mesh mesh;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            float3 pos = { x, y, z };
            positions.push_back(pos);
        }
        else if (type == "vn")
        {
            float x, y, z;
            ss >> x >> y >> z;
            float3 n = { x, y, z };
            normals.push_back(n);
        }
        else if (type == "vt")
        {
            float u, v;
            ss >> u >> v;
            float2 t = { u, v };
            texcoords.push_back(t);
        }
        else if (type == "f")
        {
            // Read all face vertices (supports triangles and quads)
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token)
            {
                if (!token.empty())
                    faceTokens.push_back(token);
            }
            
            if (faceTokens.size() < 3)
                continue;
            
            // Triangulate: fan triangulation for n-gons
            // Triangle: 0-1-2
            // Quad: 0-1-2 and 0-2-3
            // N-gon: 0-1-2, 0-2-3, 0-3-4, ...
            int numTriangles = (int)faceTokens.size() - 2;
            
            for (int tri = 0; tri < numTriangles; ++tri)
            {
                int indices[3];
                indices[0] = 0;
                indices[1] = tri + 1;
                indices[2] = tri + 2;
                
                for (int k = 0; k < 3; ++k)
                {
                    int vIdx, vtIdx, vnIdx;
                    parseFaceVertex(faceTokens[indices[k]], vIdx, vtIdx, vnIdx);

                    shader_types::VertexData v{};

                    if (vIdx >= 0 && vIdx < (int)positions.size())
                        v.position = positions[vIdx];

                    if (vnIdx >= 0 && vnIdx < (int)normals.size())
                        v.normal = normals[vnIdx];
                    else
                        v.normal = {0.f, 1.f, 0.f}; // fallback

                    if (vtIdx >= 0 && vtIdx < (int)texcoords.size())
                        v.texcoord = texcoords[vtIdx];
                    else
                        v.texcoord = {0.f, 0.f};

                    mesh.vertices.push_back(v);
                    mesh.indices.push_back(
                        static_cast<uint16_t>(mesh.vertices.size() - 1));
                }
            }
        }
    }

    return mesh;
}

static void createSphereMesh(const float radius,
                             uint32_t stacks,
                             uint32_t slices,
                             std::vector<shader_types::VertexData>& outVerts,
                             std::vector<uint16_t>& outIndices)
{
    using simd::float3;
    using simd::float2;

    for (uint32_t stack = 0; stack <= stacks; ++stack)
    {
        // calculate theta
        float v = stack / float(stacks);        // v: 0 to 1
        float theta = v * M_PI;                 // theta: 0 to pi

        // find place on cirlce
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (uint32_t slice = 0; slice <= slices; ++slice)
        {
            // caluculate phi
            float u = slice / float(slices);    // u: 0 to 1
            float phi = u * 2.0f * M_PI;        // phi: 0 to 2*pi

            // find place on sphere
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            // find x, y, z using theta and phi calculations
            float x = sinTheta * cosPhi;
            float y = cosTheta;
            float z = sinTheta * sinPhi;

            float3 pos = { radius * x, radius * y, radius * z };
            float3 normal = { x, y, z };
            float2 texcoord = { u, v };

            // build buffer data
            shader_types::VertexData vd;
            vd.position = pos;
            vd.normal   = normal;
            vd.texcoord = texcoord;

            outVerts.push_back(vd);
        }
    }

    uint32_t vertsPerRow = slices + 1;
    for (uint32_t stack = 0; stack < stacks; ++stack)
    {
        for (uint32_t slice = 0; slice < slices; ++slice)
        {
            uint32_t first  = stack * vertsPerRow + slice;
            uint32_t second = first + vertsPerRow;

            outIndices.push_back((uint16_t)first);
            outIndices.push_back((uint16_t)second);
            outIndices.push_back((uint16_t)(first + 1));

            outIndices.push_back((uint16_t)(first + 1));
            outIndices.push_back((uint16_t)second);
            outIndices.push_back((uint16_t)(second + 1));
        }
    }
}

/*
static void createCubeMesh(const float s, std::vector<shader_types::VertexData>& outVerts, std::vector<uint16_t>& outIndices) {
    outVerts = std::vector<shader_types::VertexData>{
        //   Positions          Normals
        { { -s, -s, +s }, { 0.f,  0.f,  1.f } },
        { { +s, -s, +s }, { 0.f,  0.f,  1.f } },
        { { +s, +s, +s }, { 0.f,  0.f,  1.f } },
        { { -s, +s, +s }, { 0.f,  0.f,  1.f } },
        
        { { +s, -s, +s }, { 1.f,  0.f,  0.f } },
        { { +s, -s, -s }, { 1.f,  0.f,  0.f } },
        { { +s, +s, -s }, { 1.f,  0.f,  0.f } },
        { { +s, +s, +s }, { 1.f,  0.f,  0.f } },
        
        { { +s, -s, -s }, { 0.f,  0.f, -1.f } },
        { { -s, -s, -s }, { 0.f,  0.f, -1.f } },
        { { -s, +s, -s }, { 0.f,  0.f, -1.f } },
        { { +s, +s, -s }, { 0.f,  0.f, -1.f } },
        
        { { -s, -s, -s }, { -1.f, 0.f,  0.f } },
        { { -s, -s, +s }, { -1.f, 0.f,  0.f } },
        { { -s, +s, +s }, { -1.f, 0.f,  0.f } },
        { { -s, +s, -s }, { -1.f, 0.f,  0.f } },
        
        { { -s, +s, +s }, { 0.f,  1.f,  0.f } },
        { { +s, +s, +s }, { 0.f,  1.f,  0.f } },
        { { +s, +s, -s }, { 0.f,  1.f,  0.f } },
        { { -s, +s, -s }, { 0.f,  1.f,  0.f } },
        
        { { -s, -s, -s }, { 0.f, -1.f,  0.f } },
        { { +s, -s, -s }, { 0.f, -1.f,  0.f } },
        { { +s, -s, +s }, { 0.f, -1.f,  0.f } },
        { { -s, -s, +s }, { 0.f, -1.f,  0.f } },
    };
    
    outIndices = std::vector<uint16_t>{
        0,  1,  2,  2,  3,  0,
        4,  5,  6,  6,  7,  4,
        8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20,
    };
    
}
*/

void SceneBuilder::loadPreset()
{
    const float radius  = 0.5f;
    uint32_t    stacks  = 16;
    uint32_t    slices  = 32;

    std::vector<shader_types::VertexData> verts;
    std::vector<uint16_t>                 indices;

    createSphereMesh(radius, stacks, slices, verts, indices);

    _mesh.vertices = std::move(verts);
    _mesh.indices  = std::move(indices);
}

#pragma mark - Renderer
#pragma region Renderer {

const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer( MTL::Device* pDevice )
: _pDevice( pDevice->retain() )
, _pCommandQueue(nullptr)
, _pShaderLibrary(nullptr)
, _pPSO(nullptr)
, _pDepthStencilState(nullptr)
, _pTexture(nullptr)
, _pVertexDataBuffer(nullptr)
, _pIndexBuffer(nullptr)
, _pMaterialBuffer(nullptr)
, _pLightBuffer(nullptr)
, _angle(0.f)
, _frame(0)
{
    for (int i = 0; i < kMaxFramesInFlight; ++i)
    {
        _pInstanceDataBuffer[i] = nullptr;
        _pCameraDataBuffer[i]   = nullptr;
    }

    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildDepthStencilStates();
    buildBuffers();
    
    _semaphore = dispatch_semaphore_create( Renderer::kMaxFramesInFlight );
}
Renderer::~Renderer()
{
    if (_pTexture)         _pTexture->release();
    if (_pShaderLibrary)   _pShaderLibrary->release();
    if (_pVertexDataBuffer)_pVertexDataBuffer->release();

    for ( int i = 0; i < kMaxFramesInFlight; ++i )
    {
        if (_pInstanceDataBuffer[i])
            _pInstanceDataBuffer[i]->release();
    }
    for ( int i = 0; i < kMaxFramesInFlight; ++i )
    {
        if (_pCameraDataBuffer[i])
            _pCameraDataBuffer[i]->release();
    }

    if (_pIndexBuffer)     _pIndexBuffer->release();
    if (_pPSO)             _pPSO->release();
    if (_pCommandQueue)    _pCommandQueue->release();
    if (_pDevice)          _pDevice->release();
    if (_pMaterialBuffer)  _pMaterialBuffer->release();
    if (_pLightBuffer)     _pLightBuffer->release();
}

void Renderer::buildShaders()
{
    using NS::StringEncoding::UTF8StringEncoding;
    
    const char* shaderSrc = R"(
    #include <metal_stdlib>
    using namespace metal;

    struct VertexData
    {
        float3 position;
        float3 normal;
        float2 texcoord;
    };

    struct InstanceData
    {
        float4x4 instanceTransform;
        float3x3 instanceNormalTransform;
        float4 instanceColor;
        uint32_t materialIndex;
    };

    struct CameraData
    {
        float4x4 perspectiveTransform;
        float4x4 worldTransform;
        float3x3 worldNormalTransform;
        float3 cameraPosition;
        float3 cameraDirection;
    };
    
    struct MaterialData
    {
        float3 diffuseColor;
        float reflectivity;
        float3 specularColor;
        float shininess;
        float3 emissiveColor;
        float _pad;
    };
    struct LightData
    {
        simd::float3 position;
        float intensity;
        simd::float3 color;
        float _pad;
    };

    struct v2f
    {
        float4 position [[position]];
        float3 normal;
        float3 worldPos;
        half3 color;
        uint materialIndex;
    };

    vertex v2f vertexMain( device const VertexData*   vertexData   [[buffer(0)]],
                           device const InstanceData* instanceData [[buffer(1)]],
                           constant CameraData&       cameraData   [[buffer(2)]],
                           uint vertexId   [[vertex_id]],
                           uint instanceId [[instance_id]] )
    {
        v2f o;

        const device VertexData&   vd   = vertexData[vertexId];
        const device InstanceData& inst = instanceData[instanceId];

        float4 localPos  = float4(vd.position, 1.0);
        float4 worldPos4 = inst.instanceTransform * localPos;
        float4 viewPos   = cameraData.worldTransform * worldPos4;
        float4 clipPos   = cameraData.perspectiveTransform * viewPos;

        o.position = clipPos;

        float3 n = inst.instanceNormalTransform * vd.normal;
        n = cameraData.worldNormalTransform * n;
        o.normal   = n;

        o.worldPos = worldPos4.xyz;
        o.color    = half3(inst.instanceColor.rgb);
    
        o.materialIndex = inst.materialIndex;

        return o;
    }

    fragment half4 fragmentMain( v2f in [[stage_in]],
                                     constant CameraData&       cameraData [[buffer(2)]],
                                     device   const MaterialData* materials [[buffer(3)]],
                                     constant LightData&        light      [[buffer(4)]])
     {
            const device MaterialData& mat = materials[in.materialIndex];

            float3 n       = normalize(in.normal);
            float3 viewDir = normalize(cameraData.cameraPosition - in.worldPos);

            // Point-light direction: from surface point toward light
            float3 L       = normalize(light.position - in.worldPos);

            // Diffuse
            float ndotl = saturate(dot(n, L));

            // Blinn-Phong half vector
            float3 halfDir = normalize(L + viewDir);
            float  specAngle = max(dot(halfDir, n), 0.0);
            float  specular  = pow(specAngle, mat.shininess);

            float3 diffuse = mat.diffuseColor * ndotl * light.intensity;
            float3 spec = mat.specularColor * specular * light.intensity * 2.0;

            float3 ambient = 0.05 * mat.diffuseColor;
            float3 emissive = mat.emissiveColor;

            float3 color = ambient + diffuse + spec + emissive;

            // modulate by light color
            color *= light.color;

            return half4(half3(color), 1.0);
        }
    )";
    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary( NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError );
    if ( !pLibrary )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction( NS::String::string("vertexMain", UTF8StringEncoding) );
    MTL::Function* pFragFn = pLibrary->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    pDesc->setDepthAttachmentPixelFormat( MTL::PixelFormat::PixelFormatDepth16Unorm );

    _pPSO = _pDevice->newRenderPipelineState( pDesc, &pError );
    if ( !_pPSO )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    _pShaderLibrary = pLibrary;
}

void Renderer::buildDepthStencilStates()
{
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    pDsDesc->setDepthWriteEnabled( true );

    _pDepthStencilState = _pDevice->newDepthStencilState( pDsDesc );

    pDsDesc->release();
}

void Renderer::buildBuffers()
{
    using simd::float3;
    
    SceneBuilder builder;
    bool meshLoaded = false;
    
    try
    {
        SceneBuilder::Mesh loadedMesh = builder.importObjectFile("./objects/Monkey.obj");
        if (!loadedMesh.vertices.empty() && !loadedMesh.indices.empty())
        {
            builder.setMesh(loadedMesh);
            meshLoaded = true;
            __builtin_printf("Successfully loaded OBJ file\n");
        }
        else
        {
            __builtin_printf("OBJ file is empty, using preset sphere mesh\n");
        }
    }
    catch (const std::exception& e)
    {
        __builtin_printf("Failed to load OBJ file: %s, using preset sphere mesh\n", e.what());
    }
    
    // Fallback to preset if OBJ loading failed
    if (!meshLoaded)
    {
        builder.loadPreset();
    }

    const SceneBuilder::Mesh& mesh = builder.getMesh();
    
    // Assert that mesh is not empty
    assert(!mesh.vertices.empty() && !mesh.indices.empty() && "Mesh must not be empty");
    
    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        __builtin_printf("Error: Mesh is empty! Vertices: %zu, Indices: %zu\n", 
                         mesh.vertices.size(), mesh.indices.size());
        assert(false && "Mesh must not be empty");
    }

    const size_t vertexDataSize = mesh.vertices.size() * sizeof(shader_types::VertexData);
    const size_t indexDataSize  = mesh.indices.size()  * sizeof(uint16_t);
    _indexCount = mesh.indices.size();

    _pVertexDataBuffer = _pDevice->newBuffer(vertexDataSize, MTL::ResourceStorageModeManaged);
    _pIndexBuffer = _pDevice->newBuffer(indexDataSize,  MTL::ResourceStorageModeManaged);

    memcpy(_pVertexDataBuffer->contents(), mesh.vertices.data(), vertexDataSize);
    memcpy(_pIndexBuffer->contents(),      mesh.indices.data(),  indexDataSize);

    _pVertexDataBuffer->didModifyRange(NS::Range::Make(0, _pVertexDataBuffer->length()));
    _pIndexBuffer->didModifyRange(NS::Range::Make(0, _pIndexBuffer->length()));
    
    // Instance data
    const size_t instanceDataSize = kNumInstances * sizeof(shader_types::InstanceData);
    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        _pInstanceDataBuffer[i] =
            _pDevice->newBuffer(instanceDataSize, MTL::ResourceStorageModeManaged);
    }

    const size_t cameraDataSize = sizeof(shader_types::CameraData);
    for (size_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        _pCameraDataBuffer[i] =
            _pDevice->newBuffer(cameraDataSize, MTL::ResourceStorageModeManaged);
    }
    
    shader_types::MaterialData materials[3];

        // 0: shiny plastic
        materials[0].diffuseColor   = { 0.6f, 0.05f, 0.05f };
        materials[0].specularColor  = { 1.0f, 0.9f, 0.9f };
        materials[0].shininess      = 96.0f;
        materials[0].emissiveColor  = { 0.02f, 0.01f, 0.01f };
        materials[0].reflectivity   = 0.2f; //unused right now

        // 1: dull rubber
        materials[1].diffuseColor   = { 0.15f, 0.7f, 0.15f };
        materials[1].specularColor  = { 0.03f, 0.03f, 0.03f };
        materials[1].shininess      = 2.0f;
        materials[1].emissiveColor  = { 0.0f, 0.0f, 0.0f };
        materials[1].reflectivity   = 0.0f; //unused right now

        // 2: gold-like
        materials[2].diffuseColor = {0.05f, 0.04f, 0.02f};
        materials[2].specularColor = {1.0f, 0.9f, 0.6f};
        materials[2].shininess     = 32.0f;
        materials[2].emissiveColor  = { 0.05f, 0.04f, 0.02f };
        materials[2].reflectivity   = 0.7f; //unused right now

        _pMaterialBuffer =
            _pDevice->newBuffer(sizeof(materials), MTL::ResourceStorageModeManaged);
        memcpy(_pMaterialBuffer->contents(), materials, sizeof(materials));
        _pMaterialBuffer->didModifyRange(NS::Range::Make(0, _pMaterialBuffer->length()));
    
    // Light buffer - will be updated each frame in draw()
    shader_types::LightData light;
    _pLightBuffer = _pDevice->newBuffer(sizeof(light), MTL::ResourceStorageModeManaged);
    
}

    void Renderer::draw( MTK::View* pView )
    {
        using simd::float3;
        using simd::float4;
        using simd::float4x4;

        NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

        _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
        MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[ _frame ];

        MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
        dispatch_semaphore_wait( _semaphore, DISPATCH_TIME_FOREVER );
        Renderer* pRenderer = this;
        pCmd->addCompletedHandler( ^void( MTL::CommandBuffer* pCmd ){
            dispatch_semaphore_signal( pRenderer->_semaphore );
        });

        _angle += 0.002f;

        // Update instance positions:

        const float scl = 0.2f;
        shader_types::InstanceData* pInstanceData = reinterpret_cast< shader_types::InstanceData *>( pInstanceDataBuffer->contents() );

        float3 objectPosition = { 0.f, 0.f, -10.f };

        float4x4 rt = math::makeTranslate( objectPosition );
        float4x4 rr1 = math::makeYRotate( -_angle );
        float4x4 rr0 = math::makeXRotate( _angle * 0.5 );
        float4x4 rtInv = math::makeTranslate( { -objectPosition.x, -objectPosition.y, -objectPosition.z } );
        float4x4 fullObjectRot = rt * rr1 * rr0 * rtInv;

        size_t ix = 0;
        size_t iy = 0;
        size_t iz = 0;
        for ( size_t i = 0; i < kNumInstances; ++i )
        {
            if ( ix == kInstanceRows )
            {
                ix = 0;
                iy += 1;
            }
            if ( iy == kInstanceRows )
            {
                iy = 0;
                iz += 1;
            }

            float4x4 scale = math::makeScale( (float3){ scl, scl, scl } );
            float4x4 zrot = math::makeZRotate( _angle * sinf((float)ix) );
            float4x4 yrot = math::makeYRotate( _angle * cosf((float)iy));

            float x = ((float)ix - (float)kInstanceRows/2.f) * (2.f * scl) + scl;
            float y = ((float)iy - (float)kInstanceColumns/2.f) * (2.f * scl) + scl;
            float z = ((float)iz - (float)kInstanceDepth/2.f) * (2.f * scl);
            float4x4 translate = math::makeTranslate( math::add( objectPosition, { x, y, z } ) );

            pInstanceData[ i ].instanceTransform = fullObjectRot * translate * yrot * zrot * scale;
            pInstanceData[ i ].instanceNormalTransform = math::discardTranslation( pInstanceData[ i ].instanceTransform );

            float iDivNumInstances = i / (float)kNumInstances;
            float r = iDivNumInstances;
            float g = 1.0f - r;
            float b = sinf( M_PI * 2.0f * iDivNumInstances );
            pInstanceData[ i ].instanceColor = (float4){ r, g, b, 1.0f };
            pInstanceData[i].materialIndex = static_cast<uint32_t>(iy % 3);

            ix += 1;
        }
        pInstanceDataBuffer->didModifyRange( NS::Range::Make( 0, pInstanceDataBuffer->length() ) );

        // Update camera state:
        MTL::Buffer* pCameraDataBuffer = _pCameraDataBuffer[ _frame ];
        shader_types::CameraData* pCameraData = reinterpret_cast< shader_types::CameraData* >( pCameraDataBuffer->contents() );

        pCameraData->perspectiveTransform = math::makePerspective( 45.f * M_PI / 180.f, 1.f, 0.03f, 500.0f );

        pCameraData->worldTransform       = math::makeIdentity();
        pCameraData->worldNormalTransform = math::discardTranslation( pCameraData->worldTransform );

        // Define camera position + direction in world space
        float radius = 20.0f;
        float camX = radius * cosf(_angle) * 0; // no rotation rn
        float camZ = radius * sinf(_angle) * 0;
        float camY = 10.0f;   // a bit above

        simd::float3 cameraPos = { camX, camY, camZ };
        simd::float3 target    = { 0.f, 0.f, -10.f };
        simd::float3 forward   = simd::normalize(target - cameraPos);

        // Fill struct
        pCameraData->cameraPosition  = cameraPos;
        pCameraData->cameraDirection = forward;

        pCameraDataBuffer->didModifyRange( NS::Range::Make( 0, sizeof( shader_types::CameraData ) ) );

        // Update light position - position it above and to the side of the scene for good illumination
        shader_types::LightData* pLightData = reinterpret_cast< shader_types::LightData* >( _pLightBuffer->contents() );
        
        // Position light above and to the side, rotating around the scene for dynamic lighting
        // Objects are centered at (0, 0, -10), so position light relative to that
        float lightRadius = 12.0f;
        float lightAngle = _angle * 0.3f; // Slow rotation for dynamic lighting
        
        pLightData->position = {
            lightRadius * cosf(lightAngle),  // X: rotating around
            12.0f,                           // Y: above the scene
            -10.0f + lightRadius * 0.3f * sinf(lightAngle)  // Z: slightly moving
        };
        pLightData->intensity = 1.5f;  // Increased intensity for better visibility
        pLightData->color = { 1.0f, 1.0f, 1.0f }; // white light
        pLightData->_pad = 0.0f;
        
        _pLightBuffer->didModifyRange( NS::Range::Make( 0, sizeof( shader_types::LightData ) ) );

        // Begin render pass:

        MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder( pRpd );

        pEnc->setRenderPipelineState( _pPSO );
        pEnc->setDepthStencilState( _pDepthStencilState );

        pEnc->setVertexBuffer( _pVertexDataBuffer, 0, 0 );
        pEnc->setVertexBuffer( pInstanceDataBuffer, 0, 1 );
        pEnc->setVertexBuffer( pCameraDataBuffer,   0, 2 );
        pEnc->setFragmentBuffer( pCameraDataBuffer, 0, 2 );
        pEnc->setFragmentBuffer( _pMaterialBuffer,  0, 3 );
        pEnc->setFragmentBuffer( _pLightBuffer,     0, 4 );

        pEnc->setCullMode( MTL::CullModeBack );
        pEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );

        
        pEnc->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, _indexCount, MTL::IndexType::IndexTypeUInt16, _pIndexBuffer, 0, kNumInstances );

        pEnc->endEncoding();
        pCmd->presentDrawable( pView->currentDrawable() );
        pCmd->commit();
        
        pPool->release();
    }

    #pragma endregion Renderer }
