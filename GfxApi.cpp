#include "GfxApi.h"

#include <GLFW/glfw3.h>

#include <boost/foreach.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/integer_traits.hpp>
#include <boost/limits.hpp>

#include <boost/range/adaptor/indirected.hpp>
using namespace boost::adaptors;

#include <assert.h>

#include <sstream>
#include <iostream>
#include <fstream>

#include <algorithm>

#include "ocl.h"

namespace GfxApi {


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int checkOglError(const char *file, int line)
{
    GLenum glErr;
    int    retCode = 0;
    glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        std::string msg = (boost::format("glError in file %s @ line %d: %s\n")  % file % line % gluErrorString(glErr)).str();
        throw std::runtime_error(msg);
        retCode = 1;
    }
    return retCode;
}


GLenum toGL(VertexDataType type)
{
    switch (type)
    {
        case (VertexDataType::BYTE): return GL_BYTE;
        case (VertexDataType::UNSIGNED_BYTE): return GL_UNSIGNED_BYTE;
        case (VertexDataType::UNSIGNED_SHORT): return GL_UNSIGNED_SHORT;
        case (VertexDataType::SHORT): return GL_SHORT;
        case (VertexDataType::INT): return GL_INT;
        case (VertexDataType::FLOAT): return GL_FLOAT;
        case (VertexDataType::DOUBLE): return GL_DOUBLE;
    }
}


Graphics::Graphics() 
{
  

}


Graphics::~Graphics(void)
{
}


void Graphics::clear(bool clearDepth, bool clearStencil, bool clearColor,
            float red, float green, float blue, float alpha,
            float depthValue, int stencilValue)
{
    //glClearStencil(stencilValue);
        
    {
        glClearColor(red, green, blue, alpha);
    }
    {
        GLbitfield mask = 0;
            
        mask |= ((clearDepth) ? GL_DEPTH_BUFFER_BIT : 0)
                | ((clearStencil) ? GL_STENCIL_BUFFER_BIT : 0)
                | ((clearColor) ? GL_COLOR_BUFFER_BIT : 0);
            
        glClear(mask);
    }
        
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VertexElement::VertexElement(VertexDataSemantic semantic, VertexDataType type, int count, const std::string& name)
    : m_semantic(semantic)
    , m_type(type)
    , m_count(count)
    , m_name(name)
{
}


VertexDataType VertexElement::getType() const
{
    return m_type;
}


VertexElement::~VertexElement(void)
{
}


int VertexElement::getSize() const
{
    int size = 0;
    switch(m_type)
    {
    case VertexDataType::BYTE:
    case VertexDataType::UNSIGNED_BYTE:
        size = 1;
        break;
    case VertexDataType::DOUBLE:
        size = 8;
        break;
    case VertexDataType::FLOAT:
    case VertexDataType::INT:
        size = 4;
        break;
    case VertexDataType::SHORT:
    case VertexDataType::UNSIGNED_SHORT:
        size = 2;
        break;
    }

    return size * m_count;
}


int VertexElement::getCount() const
{
    return m_count;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VertexDeclaration::VertexDeclaration()
{
}


VertexDeclaration::~VertexDeclaration(void)
{
}


void VertexDeclaration::add(VertexElement elem)
{
    m_elements.push_back(elem);
}


int VertexDeclaration::getSize() const
{

    int size = 0;

    for(auto& elem : m_elements)
    {
        size += elem.getSize();
    }

    return size;
}


const std::vector<VertexElement>& VertexDeclaration::elements() const
{
    return m_elements;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VertexBuffer::VertexBuffer(int numVertices, const VertexDeclaration& dec)
    : m_numVertices(numVertices)
    , m_declaration(dec)
    , m_vbo(0)
    , m_gpuSize(0)
{
    glGenBuffers(1, &m_vbo); 
    checkOpenGLError();
    allocateCpu();
}


VertexBuffer::~VertexBuffer()
{
    if(m_vbo)
    {
        glDeleteBuffers(1, &m_vbo);
    }
}


int VertexBuffer::getNumVertices() const
{
    return m_numVertices;
}
   

int VertexBuffer::getSizeInBytes() const
{
    return m_numVertices * m_declaration.getSize();
}


const unsigned char* VertexBuffer::getCpuPtr() const
{
    return m_cpuData->data();
}


unsigned char* VertexBuffer::getCpuPtr()
{
    return m_cpuData->data();
}


int VertexBuffer::getGpuSizeInBytes() const
{
    return m_gpuSize;
}


const VertexDeclaration& VertexBuffer::getDeclaration() const
{
    return m_declaration;
}


VertexDeclaration& VertexBuffer::getDeclaration()
{
    return m_declaration;
}


void VertexBuffer::setNumVertices(int size, bool perseve_old_cpu_data)
{
    int new_bytes = m_declaration.getSize() * size;
      
    if (perseve_old_cpu_data && !!m_cpuData && new_bytes > m_cpuData->size())
    {
        ///keep the old data around a bit
        boost::shared_ptr<cpu_data_t> oldCpuData = m_cpuData;
         
        ///set the new number of vertices
        m_numVertices = size;
            
        ///allocate the buffer
        allocateCpu();

        ///copy the old buffer to the new one
        memcpy(m_cpuData->data(), oldCpuData->data(), oldCpuData->size());
    } 
    else 
    {
        allocateCpu();
    }
}


void VertexBuffer::allocateCpu()
{
    int bytes = getSizeInBytes();
   
    if (!m_cpuData)
    {
        m_cpuData = boost::make_shared< cpu_data_t >(bytes);
    }
    else 
    {
        m_cpuData->resize(bytes);
    }
}


void VertexBuffer::allocateGpu()
{

    int bytes = getSizeInBytes();
       
    GLenum usage = GL_STATIC_DRAW;
       
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo); 
    checkOpenGLError();
    glBufferData(GL_ARRAY_BUFFER, bytes, NULL, usage); 
    checkOpenGLError();
         
    m_gpuSize = bytes;
        
    //todo: is this necessary?
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    checkOpenGLError();

}


void VertexBuffer::updateToGpu()
{
        
    if ( !m_cpuData )
    {
        throw std::runtime_error( "no cpu data to send to gpu" );
    }

    int bytes = m_cpuData->size();
        
    GLenum usage = GL_STATIC_DRAW;
      
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo); 
    checkOpenGLError();
      
    glBufferData(GL_ARRAY_BUFFER, bytes, m_cpuData->data(), usage);
    checkOpenGLError();
      
    m_gpuSize = bytes;
         
    //todo: is this necessary?
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    checkOpenGLError();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


IndexBuffer::IndexBuffer(int numIndices, const VertexDeclaration& dec, PrimitiveIndexType type)
    : m_indexType(type)
    , m_numIndices(numIndices)
    , m_indexBuffer(0)
    , m_gpuSize(0)
{
    glGenBuffers(1, &m_indexBuffer);
    checkOpenGLError();
    allocateCpu();
}


IndexBuffer::~IndexBuffer(void)
{
    if(m_indexBuffer)
    {
        glDeleteBuffers(1, &m_indexBuffer);
    }
}


int IndexBuffer::getNumIndices() const
{
    return m_numIndices;
}


PrimitiveIndexType IndexBuffer::getIndexType() const
{
    return m_indexType;
}


int IndexBuffer::getSingleIndexSizeInBytes() const
{
    int32_t size = 0;

    switch(m_indexType)
    {
    case PrimitiveIndexType::IndicesNone:
        size = 0;
        break;
    case PrimitiveIndexType::Indices8Bit:
        size = 1;
        break;
    case PrimitiveIndexType::Indices16Bit:
        size = 2;
        break;
    case PrimitiveIndexType::Indices32Bit:
        size = 4;
        break;
    }

    return size;
}


int IndexBuffer::getIndexSizeInBytes() const
{
    int32_t size = 0;

    switch(m_indexType)
    {
    case PrimitiveIndexType::IndicesNone:
        size = 0;
        break;
    case PrimitiveIndexType::Indices8Bit:
        size = m_numIndices;
        break;
    case PrimitiveIndexType::Indices16Bit:
        size = m_numIndices * 2;
        break;
    case PrimitiveIndexType::Indices32Bit:
        size = m_numIndices * 4;
        break;
    }

    return size;
}


void IndexBuffer::allocateCpu()
{
    int bytes = getIndexSizeInBytes();
   
    if (!m_indexData)
    {
        m_indexData = boost::make_shared< cpu_data_t >(bytes);
    }
    else 
    {
        m_indexData->resize(bytes);
    }
}


void IndexBuffer::setNumIndices(int size, bool perseve_old_cpu_data)
{
    int newBytes = getSingleIndexSizeInBytes() * size;
      
    if (perseve_old_cpu_data && !!m_indexData && newBytes > m_indexData->size())
    {
        ///keep the old data around a bit
        boost::shared_ptr<cpu_data_t> oldCpuData = m_indexData;
         
        ///set the new number of vertices
        m_numIndices = size;
            
        ///allocate the buffer
        allocateCpu();

        ///copy the old buffer to the new one
        memcpy(m_indexData->data(), oldCpuData->data(), oldCpuData->size());
    } 
    else 
    {
        allocateCpu();
    }
}


unsigned char * IndexBuffer::getCpuPtr()
{
    return m_indexData->data();
}


const unsigned char * IndexBuffer::getCpuPtr() const
{
    return m_indexData->data();
}


void IndexBuffer::allocateGpu()
{

    int bytes = getIndexSizeInBytes();

    GLenum usage = GL_STATIC_DRAW;
       
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer); 
    checkOpenGLError();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, NULL, usage); 
    checkOpenGLError();
         
    m_gpuSize = bytes;
        
    //todo: is this necessary?
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
    checkOpenGLError();

}


void IndexBuffer::updateToGpu()
{
        
    if ( !m_indexData )
    {
        throw std::runtime_error( "no cpu data to send to gpu" );
    }

    int bytes = m_indexData->size();
        
    //if (bytes > m_gpu_size)
    //    throw gfxapi_error( "cpu data too large to send to small gpu buffer" );
        
    GLenum usage = GL_STATIC_DRAW;
      
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer); 
    checkOpenGLError();
      
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, m_indexData->data(), usage);
    checkOpenGLError();
      
    m_gpuSize = bytes;
         
    //todo: is this necessary?
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
    checkOpenGLError();

    glBindVertexArray(0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Shader::Shader(ShaderType type)
    : m_type(type)
    , m_shaderHandle(0)
{
    if (type == ShaderType::VertexShader) 
    {
        m_shaderHandle = glCreateShader (GL_VERTEX_SHADER); 
        assert(m_shaderHandle != -1);
        checkOpenGLError();
    } 
    else if (type == ShaderType::PixelShader) 
    {
        m_shaderHandle = glCreateShader (GL_FRAGMENT_SHADER); 
        assert(m_shaderHandle != -1);
        checkOpenGLError();
    } 
    else
    {
        throw std::runtime_error("Cannot create Shader: Invalid shader type");
    }
}

    
void Shader::LoadFromString(ShaderType type,
                            const char* shaderData,
                            const char* entryPoint,
                            const char* profile)
{
    assert(entryPoint == 0);
    assert(profile == 0);        
    if (type != m_type)
    {
        throw std::runtime_error("Cannot load Shader: Invalid shader type; does not match type indicated in constructor");
    }
         
    assert(m_shaderHandle);
         
    glShaderSource(m_shaderHandle, 1, &shaderData, NULL); 
    checkOpenGLError();
    glCompileShader(m_shaderHandle); checkOpenGLError();
        
    GLint params = 0;
    glGetShaderiv(m_shaderHandle,GL_COMPILE_STATUS, &params); 
    checkOpenGLError();
        
    if (params == GL_FALSE)
    {
        throw std::runtime_error("Shader: Failed to compile");
    }
}


void Shader::LoadFromFile(ShaderType type,
                        const char* fileName,
                        const char* entryPoint,
                        const char* profile)
{
   

    //open file
    std::ifstream f;
    f.open(fileName, std::ios::in | std::ios::binary);
    if(!f.is_open()){
        throw std::runtime_error(std::string("Failed to open file: ") + fileName);
    }

    //read whole file into stringstream buffer
    std::stringstream buffer;
    buffer << f.rdbuf();

    LoadFromString(type,buffer.str().c_str(), entryPoint, profile);

}


Shader::~Shader(void)
{
    glDeleteShader(m_shaderHandle); 
    checkOpenGLError();
    m_shaderHandle = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


ShaderProgram::ShaderProgram(void)
    : m_programHandle(0)
{
    m_programHandle = glCreateProgram();
    assert(m_programHandle != -1);
    checkOpenGLError();
    
}


ShaderProgram::~ShaderProgram(void)
{
    glDeleteProgram(m_programHandle);
    checkOpenGLError();
}


void ShaderProgram::use()
{
    glUseProgram(m_programHandle);
    checkOpenGLError();
}


int ShaderProgram::getUniformLocation(const char* name)
{
    return glGetUniformLocation(m_programHandle, name);
}


void ShaderProgram::setFloat4x4(int parameterIndex, const float4x4& matrix)
{
    float4x4 colMajor = matrix.Transposed();
    glUniformMatrix4fv(parameterIndex, 1 /*count*/, GL_FALSE /*transpose*/, colMajor.ptr() /*value*/);
}

void ShaderProgram::attach(Shader& shader)
{
    assert(m_programHandle);
    glAttachShader (m_programHandle, shader.m_shaderHandle); 
    checkOpenGLError();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Mesh::Mesh(PrimitiveType primType)
    : m_primType(primType)
    , m_vao(0)
    , m_numVertices(0)
{
    glGenVertexArrays (1, &m_vao); 
    checkOpenGLError();
}


Mesh::~Mesh(void)
{
    glDeleteVertexArrays (1, &m_vao); 
    checkOpenGLError();
}


PrimitiveType Mesh::getPrimType() const
{
    return m_primType;
}


void Mesh::generateVAO(int startVertexOffset, bool check_index_bounds)
{
    checkValid(startVertexOffset, check_index_bounds);
 
    ///determine numVertices
    {
        m_numVertices = boost::integer_traits<int>::max();
 
        BOOST_FOREACH(const VertexBuffer& vb, m_vbs | indirected)
        {
            m_numVertices = std::min(vb.getNumVertices(), m_numVertices);
        }
    }
      
    ///Fill in declaration.elements
    {
        BOOST_FOREACH(const VertexBuffer& vb, m_vbs | indirected)
        {
            BOOST_FOREACH(const VertexElement& element, vb.getDeclaration().elements())
            m_declaration.add(element);
        }
    }
     
    glBindVertexArray(m_vao); 
    checkOpenGLError();
       
    int attr_index = 0;
    ///vbo
    BOOST_FOREACH(VertexBuffer& vb, m_vbs | indirected)
    {
        /**
        * From http://antongerdelan.net/opengl/vertexbuffers.html
        * 
        * Also see http://www.arcsynthesis.org/gltut/Positioning/Tutorial%2005.html
        */
 
        const VertexDeclaration& dec = vb.getDeclaration();
      
        
        glBindBuffer(GL_ARRAY_BUFFER, vb.m_vbo); 
        checkOpenGLError();
       
        int offset = 0;
        int stride = dec.getSize();
       
        ///FIXME: Make this a NDEBUG runtime check too
        assert( startVertexOffset < vb.getSizeInBytes() );
        BOOST_FOREACH(const VertexElement& element, dec.elements())
        {
               
            //element.check_valid();
            
            glEnableVertexAttribArray(attr_index);
            glVertexAttribPointer (attr_index,
                                   element.getCount(),
                                   toGL(element.getType()),
                                   GL_FALSE,
                                   stride,
                                   (void*)(startVertexOffset + offset)); 
            checkOpenGLError();
             
           
            offset += element.getSize();
            ++attr_index;
        }
          
          
    }

    for (int i = 0; i < attr_index; ++i)
    {
   //     glEnableVertexAttribArray (i); 
    //    checkOpenGLError();
    }
   
    ///indices
    if (m_ib)
    {
        /**
        * From http://www.opengl.org/wiki/Vertex_Specification#Index_buffers
        * 
        * 
        * Indexed rendering, as defined above, requires an array of indices;
        * all vertex attributes will use the same index from this index array.
        * The index array is provided by a Buffer Object bound to the
        * GL_ELEMENT_ARRAY_BUFFER​ binding point. When a buffer is bound to
        * GL_ELEMENT_ARRAY_BUFFER​, all rendering commands of the form
        * gl*Draw*Elements*​ will use indexes from that buffer. Indices can be
        * unsigned bytes, unsigned shorts, or unsigned ints.
        */                
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ib->m_indexBuffer); 
        checkOpenGLError();
    }
       
       
    glBindVertexArray(0); 
    checkOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    checkOpenGLError();
        
    if (m_ib)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
        checkOpenGLError();
    }
}


void Mesh::linkShaders()
{
    if (!m_sp)
    {
        throw std::runtime_error("Mesh.LinkShaders(): No shader program; no shaders to link");
    }

    GLint programHandle = m_sp->m_programHandle;

    int attr_index = 0;

    BOOST_FOREACH(const VertexBuffer& vb, m_vbs | indirected)
    {
        const VertexDeclaration& dec = vb.getDeclaration();
  
        BOOST_FOREACH(const VertexElement& element, dec.elements())
        {
            std::string attribute_name = element.m_name;
            glBindAttribLocation (programHandle, attr_index, attribute_name.c_str()); 
            checkOpenGLError();
            ++attr_index;
        }
    }
      
    glLinkProgram (programHandle); 
    checkOpenGLError();
}


void Mesh::applyVAO()
{
    checkValid();
    if (m_vao == 0)
        throw std::runtime_error("Can't render the mesh: no VAO");
      
    glBindVertexArray(m_vao); 
    checkOpenGLError();
}

void Mesh::unbindVAO()
{
    glBindVertexArray(0); 
    checkOpenGLError();
}

void Mesh::draw(int numIndices, int startIndexOffset)
{
    checkValid();
        
    if (m_vao == 0)
        throw std::runtime_error("Can't render the mesh: no VAO");
            
        
        
    if (m_ib)
    {
        int index_count = m_ib->getNumIndices();
        
        if (numIndices == -1)
            numIndices = index_count - startIndexOffset;
            
        if (!(startIndexOffset + numIndices <= index_count))
            throw std::runtime_error("Can't render the mesh: startIndexOffset/numIndices lie outside the range of possible indices");
            
        GLenum data_type = 0;
            
        ///FIXME: need to get a primitve count based on the primitive type
        assert(getPrimType() == PrimitiveType::TriangleList);
        switch (m_ib->getIndexType())
        {
            case(PrimitiveIndexType::IndicesNone):
                throw std::runtime_error("IndexBuffer has no index type specified");
            case(PrimitiveIndexType::Indices8Bit):
                data_type = GL_UNSIGNED_BYTE; 
                break;
            case(PrimitiveIndexType::Indices16Bit):
                data_type = GL_UNSIGNED_SHORT; 
                break;
            case(PrimitiveIndexType::Indices32Bit):
                data_type = GL_UNSIGNED_INT; 
                break;
            default:
                assert(false);
                
        }
        
        //GLint bound_buff; 
        
        //glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &bound_buff);
        
        //printf("%i\n", bound_buff);
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ib->m_indexBuffer);
        //printf("%i\n", index_count - startIndexOffset);
        glDrawElements(GL_TRIANGLES, index_count - startIndexOffset, data_type, (void*)startIndexOffset);
        checkOpenGLError();
        
    } 
    else 
    {
        switch(getPrimType())
        {
        case PrimitiveType::TriangleList:
            glDrawArrays (GL_TRIANGLES, startIndexOffset, m_numVertices); 
            checkOpenGLError();
            break;
        case PrimitiveType::LineList:
            glDrawArrays (GL_LINES, 0, m_numVertices); 
            checkOpenGLError();
            break;
        case PrimitiveType::Point:
            glDrawArrays (GL_POINTS, 0, m_numVertices); 
            checkOpenGLError();
            break;
        }
    }
        
}


void Mesh::checkValid(int startVertexOffset, bool check_index_bounds) const
{
    if (m_ib)
    {
        if (m_ib->getIndexType() == PrimitiveIndexType::IndicesNone)
            throw std::runtime_error("Invalid Mesh: Index Buffer does not have a valid index type");
           
    }
          
    if (m_vbs.size() == 0)
        throw std::runtime_error("Invalid Mesh: no Vertex Buffer");
        
        
    BOOST_FOREACH(const VertexBuffer& vb, m_vbs | indirected)
    {
        if (vb.getGpuSizeInBytes() < vb.getSizeInBytes())
            throw std::runtime_error("Invalid Mesh: GPU side of VertexBuffer does not have enough memory");
    }
        
        
    int minNumVertices = boost::integer_traits<int>::max();
        
    {
        BOOST_FOREACH(const VertexBuffer& vb, m_vbs | indirected)
        {
            minNumVertices = std::min(minNumVertices,vb.getNumVertices());
        }
            
        if (!(startVertexOffset < minNumVertices))
            throw std::runtime_error("Invalid Mesh: startVertexOffset overflows one or more vertex buffers");
    }
        
    {
        int usabeIndices = minNumVertices - startVertexOffset;
            
        if (check_index_bounds)
            assert(false && "TODO");
    }
        
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Input::Input(GLFWwindow* window)
    : m_pWindow(window)
{
}


Input::~Input(void)
{
}


int Input::keyPressed(int key)
{
    return glfwGetKey(m_pWindow, key);
}


void Input::poll()
{
    glfwPollEvents();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


RenderNode::RenderNode(boost::shared_ptr<Mesh> mesh, float3 pos, float3 scale, float3 rotation)
    : m_pMesh(mesh)
    , m_pos(pos)
    , m_scale(scale)
    , m_rotation(rotation)
{
    
   m_xForm = float4x4::FromTRS(m_pos, float4x4(Quat::identity, float3(0,0,0)), m_scale); 

}


RenderNode::~RenderNode(void)
{
}


float4x4 RenderNode::getXForm() const
{
    return m_xForm;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


Texture::Texture(void)
{
}


Texture::~Texture(void)
{
}


#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII

void Texture::loadDDS(const char * imagepath)
{

	unsigned char header[124];

	FILE *fp; 
 
	/* try to open the file */ 
	fp = fopen(imagepath, "rb"); 
	if (fp == NULL)
    {
        throw std::runtime_error("Image path could not be opened. Are you in the right directory ?"); 
	}
   
	/* verify the type of file */ 
	char filecode[4]; 
	fread(filecode, 1, 4, fp); 
	if (strncmp(filecode, "DDS ", 4) != 0) { 
		fclose(fp); 
		throw std::runtime_error("No valid DDS header");
	}
	
	/* get the surface desc */ 
	fread(&header, 124, 1, fp); 

	m_height = *(unsigned int*)&(header[8 ]);
	m_width  = *(unsigned int*)&(header[12]);
	unsigned int linearSize	 = *(unsigned int*)&(header[16]);
	unsigned int mipMapCount = *(unsigned int*)&(header[24]);
	unsigned int fourCC      = *(unsigned int*)&(header[80]);

 
	unsigned char * buffer;
	unsigned int bufsize;
	/* how big is it going to be including all mipmaps? */ 
	bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize; 
	buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char)); 
	fread(buffer, 1, bufsize, fp); 
	/* close the file pointer */ 
	fclose(fp);

	unsigned int components  = (fourCC == FOURCC_DXT1) ? 3 : 4; 
	unsigned int format;
	switch(fourCC) 
	{ 
	case FOURCC_DXT1: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; 
		break; 
	case FOURCC_DXT3: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; 
		break; 
	case FOURCC_DXT5: 
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; 
		break; 
	default: 
		free(buffer); 
		throw std::runtime_error("Unknown DXT type.");
	}

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);	
	
	unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16; 
	unsigned int offset = 0;

    unsigned int height = m_height;
    unsigned int width = m_width;

	/* load the mipmaps */ 
	for (unsigned int level = 0; level < mipMapCount && (m_width || m_height); ++level) 
	{ 
		unsigned int size = ((width+3)/4)*((height+3)/4)*blockSize; 
		glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height,  
			0, size, buffer + offset); 
	 
		offset += size; 
		width  /= 2; 
		height /= 2; 

		// Deal with Non-Power-Of-Two textures. This code is not included in the webpage to reduce clutter.
		if(width < 1)
            width = 1;
		if(height < 1) 
            height = 1;

	} 

	free(buffer); 

    m_textureHandle = textureID;


}


void Texture::loadBMP(const char * imagepath)
{

	printf("Reading image %s\n", imagepath);

	// Data read from the header of the BMP file
	unsigned char header[54];
	unsigned int dataPos;
	unsigned int imageSize;
	unsigned int width, height;
	// Actual RGB data
	unsigned char * data;

	// Open the file
	FILE * file = fopen(imagepath,"rb");
	if (!file)							    
    {
        throw std::runtime_error("Image path could not be opened. Are you in the right directory ? "); 
    }

	// Read the header, i.e. the 54 first bytes

	// If less than 54 bytes are read, problem
	if ( fread(header, 1, 54, file)!=54 )
    { 
		throw std::runtime_error("Not a correct BMP file");
	}

	// A BMP files always begins with "BM"
	if ( header[0]!='B' || header[1]!='M' )
    {
		throw std::runtime_error("Not a correct BMP file");
	}

	// Make sure this is a 24bpp file
	if ( *(int*)&(header[0x1E])!=0  )         
    {
        throw std::runtime_error("Not a correct BMP file\n");    
    }

	if ( *(int*)&(header[0x1C])!=24 )         
    {
        throw std::runtime_error("Not a correct BMP file\n");    
    }

	// Read the information about the image
	dataPos    = *(int*)&(header[0x0A]);
	imageSize  = *(int*)&(header[0x22]);
	m_width      = *(int*)&(header[0x12]);
	m_height     = *(int*)&(header[0x16]);

	// Some BMP files are misformatted, guess missing information
	if (imageSize==0)    imageSize=m_width*m_height*3; // 3 : one byte for each Red, Green and Blue component
	if (dataPos==0)      dataPos=54; // The BMP header is done that way

	// Create a buffer
	data = new unsigned char [imageSize];

	// Read the actual data from the file into the buffer
	fread(data,1,imageSize,file);

	// Everything is in memory now, the file wan be closed
	fclose (file);

	// Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Give the image to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, m_width, m_height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);

	// OpenGL has now copied the data. Free our own version
	delete [] data;

	// Poor filtering, or ...
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 

	// ... nice trilinear filtering.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
	glGenerateMipmap(GL_TEXTURE_2D);

	// Return the ID of the texture we just created
    m_textureHandle = textureID;
}


GLuint Texture::getHandle() const
{
    return m_textureHandle;
}


}