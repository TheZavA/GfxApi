#ifndef _GFXAPI_H
#define _GFXAPI_H

#include <GL/glew.h>
#include <stdint.h>

#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <mgl/MathGeoLib.h>

struct GLFWwindow;

namespace GfxApi {


#define checkOpenGLError() checkOglError(__FILE__, __LINE__)
int checkOglError(const char *file, int line);


enum class VertexDataSemantic 
{
    COLOR, 
    COLOR2, 
    NORMAL, 
    TCOORD, 
    VCOORD
};


enum class VertexDataType
{
    BYTE, 
    UNSIGNED_BYTE, 
    UNSIGNED_SHORT,   
    SHORT, 
    INT, 
    FLOAT, 
    DOUBLE
};


enum class ShaderType 
{
    NullShader, 
    VertexShader, 
    PixelShader
};


enum class PrimitiveType 
{
    Point, 
    LineList, 
    LineStrip, 
    TriangleList, 
    TriangleStrip
};


enum class PrimitiveIndexType 
{
    IndicesNone,
    Indices8Bit, 
    Indices16Bit, 
    Indices32Bit
};


GLenum toGL(VertexDataType type);


class Graphics
{
public:
    Graphics();
    ~Graphics(void);

    void clear(bool clearDepth = true, bool clearStencil = true, bool clearColor = true,
               float red = 0.f, float green = 0.f, float blue = 0.f, float alpha = 0.f,
               float depthValue = 1.f, int stencilValue = 0);
};



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class VertexElement
{
public:
    VertexElement(VertexDataSemantic semantic, VertexDataType type, int count, const std::string& name);
    ~VertexElement(void);

    int getSize() const;
    int getCount() const;
    VertexDataType getType() const;
        
    //void save_to_file(std::ostream& out);
    //void load_from_file(std::istream& in);

    //void dump_to_console();
        
    /**
    * Returns a string of format "(semantic): : (name) semantic: (type), count: (num)".
    */
    std::string toString() const;
        
    int m_count;
    VertexDataSemantic m_semantic;
    VertexDataType m_type;
    std::string m_name;
        
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class VertexDeclaration
{
public:
    VertexDeclaration();
    ~VertexDeclaration(void);
 
    /**
    * Adds a new data element to the end of the element list.
    */
    void add(VertexElement elem);
        
    /**
    * The size of a single vertex in the stream in bytes, including any padding.
    */
    int getSize() const;
        
        
    const std::vector<VertexElement>& elements() const;
        
    //void save_to_file(std::ostream& out);
    //void load_from_file(std::istream& in);
    //void dump_to_console();
private:

        
    std::vector<VertexElement> m_elements;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class VertexBuffer : boost::noncopyable
{
public:
    explicit VertexBuffer(int numVertices, const VertexDeclaration& dec);
    ~VertexBuffer();

    typedef std::vector<unsigned char> cpu_data_t;
        
    std::string name;
        
    int getNumVertices() const;
   
    int getSizeInBytes() const;

    void setNumVertices(int size, bool perseve_old_cpu_data = true);
        
    void updateToGpu();
    void updateToCpu();
    void allocateCpu();
    void allocateGpu();
        
    const VertexDeclaration& getDeclaration() const;
    VertexDeclaration& getDeclaration();
        
        
    const unsigned char* getCpuPtr() const;
    unsigned char* getCpuPtr();
        
    bool hasGpuMemory() const;
    int getGpuSizeInBytes() const;
    int getCpuSizeInBytes() const;
        
private:
        
    GLuint m_vbo;
        
    int m_numVertices;
    boost::shared_ptr< cpu_data_t > m_cpuData;
    int m_gpuSize;
        
    VertexDeclaration m_declaration;
    friend class Graphics;
    friend class Mesh;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class IndexBuffer : boost::noncopyable
{
public:
    typedef std::vector<unsigned char> cpu_data_t;

    explicit IndexBuffer(int numIndices, const VertexDeclaration& dec, PrimitiveIndexType type);
    ~IndexBuffer();
        
    std::string name;
        
    void updateToGpu();
    void updateToCpu();
    void allocateCpu();
    void allocateGpu();
        
    int getNumIndices() const;
    ///The size of the IndexBuffer, calculated according to the number of indices, and the index-type.
    int getIndexSizeInBytes() const;
    int getSingleIndexSizeInBytes() const;

    void setNumIndices(int size, bool perseve_old_cpu_data);
        
    const std::string& getName() const;

    unsigned char * getCpuPtr();
    const unsigned char * getCpuPtr() const;
        

    PrimitiveIndexType getIndexType() const;

    GLuint m_indexBuffer;

private:
    PrimitiveIndexType m_indexType;
        
    int m_numIndices;

    boost::shared_ptr< cpu_data_t > m_indexData;
    int m_gpuSize;
        
    friend class Graphics;
    friend class Mesh;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Texture
{
public:
    Texture(void);
    ~Texture(void);

    void loadDDS(const char * imagepath);
    void loadBMP(const char * imagepath);

    GLuint getHandle() const;

private:
    GLuint m_textureHandle;

    unsigned int m_height;
    unsigned int m_width;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Shader : boost::noncopyable
{
public:
    explicit Shader(ShaderType type);
    ~Shader();
        
    void LoadFromString(ShaderType type,
                        const char* shaderData,
                        const char* entryPoint=0,
                        const char* profile=0);

    void LoadFromFile(ShaderType type,
                        const char* fileName,
                        const char* entryPoint=0,
                        const char* profile=0);

    GLuint m_shaderHandle;
        
private:
    ShaderType m_type;
    
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class ShaderProgram : boost::noncopyable
{
public:
    explicit ShaderProgram();
    ~ShaderProgram();
        
    void attach(Shader& shader);
        
    int getUniformLocation(const char* name);
        
    void use();

    static void setFloat4x4(int parameterIndex, const float4x4& matrix);


    GLuint m_programHandle;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Mesh : boost::noncopyable
{
public:
    explicit Mesh(PrimitiveType primType);
    ~Mesh();
        
    std::vector<boost::shared_ptr<VertexBuffer> > m_vbs;
    boost::shared_ptr<IndexBuffer> m_ib;
    boost::shared_ptr<ShaderProgram> m_sp;
        
    void generateVAO(int startVertexOffset=0, bool check_index_bounds=false);
    void linkShaders();
    void applyVAO();

    static void unbindVAO();
        
    void draw(int numIndices=-1, int startIndexOffset=0);

    void checkValid(int startVertexOffset=0, bool check_index_bounds=false) const;

    const VertexDeclaration& Declaration() const;

    /**
    * Returns the type of primitives used by this mesh (PrimPoint, PrimLineList, PrimLineStrip, PrimTriangleList or PrimTriangleStrip).
    */
    PrimitiveType getPrimType() const;

private:
        
    GLuint m_vao;
    int m_numVertices;
    VertexDeclaration m_declaration;

    /**
    * The primitive type that is used for rendering.
    */
    PrimitiveType m_primType;
        
        
    friend class Graphics;
        
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class Input
{
public:
    Input(GLFWwindow* window);
    ~Input(void);

    int keyPressed(int key);

    void poll();

private: 
    GLFWwindow* m_pWindow;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class RenderNode
{
public:
    RenderNode(boost::shared_ptr<Mesh> mesh, float3 pos, float3 scale = float3(1,1,1), float3 rotation = float3(0,0,0));
    ~RenderNode(void);

    float4x4 getXForm() const;

    boost::shared_ptr<Mesh> m_pMesh;
    float3 m_pos;
    float3 m_scale;
    float3 m_rotation;
    float4x4 m_xForm;

};


}



#endif