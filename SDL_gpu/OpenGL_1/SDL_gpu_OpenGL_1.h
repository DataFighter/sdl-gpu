#ifndef _SDL_GPU_OPENGL_1_H__
#define _SDL_GPU_OPENGL_1_H__

#include "SDL_gpu.h"

#if !defined(SDL_GPU_DISABLE_OPENGL) && !defined(SDL_GPU_DISABLE_OPENGL_1)

    // Hacks to fix compile errors due to polluted namespace
    #ifdef _WIN32
    #define _WINUSER_H
    #define _WINGDI_H
    #endif

    #include "glew.h"
	
	#if defined(GL_EXT_bgr) && !defined(GL_BGR)
		#define GL_BGR GL_BGR_EXT
	#endif
	#if defined(GL_EXT_bgra) && !defined(GL_BGRA)
		#define GL_BGRA GL_BGRA_EXT
	#endif
	#if defined(GL_EXT_abgr) && !defined(GL_ABGR)
		#define GL_ABGR GL_ABGR_EXT
	#endif
	
	#undef glCreateShader
	#undef GL_VERTEX_SHADER
	#undef GL_FRAGMENT_SHADER
	#undef glShaderSource
	#undef glCompileShader
	#undef glGetShaderiv
	#undef GL_COMPILE_STATUS
	#undef glGetShaderInfoLog
	#undef glDeleteShader
	#define glCreateShader glCreateShaderObjectARB
	#define GL_VERTEX_SHADER GL_VERTEX_SHADER_ARB
	#define GL_FRAGMENT_SHADER GL_FRAGMENT_SHADER_ARB
	#define glShaderSource glShaderSourceARB
	#define glCompileShader glCompileShaderARB
	#define glGetShaderiv glGetObjectParameterivARB
	#define GL_COMPILE_STATUS GL_OBJECT_COMPILE_STATUS_ARB
	#define glGetShaderInfoLog glGetInfoLogARB
	#define glDeleteShader glDeleteObjectARB
	
	#undef glCreateProgram
	#undef glAttachShader
	#undef glLinkProgram
	#undef GL_LINK_STATUS
	#undef glGetProgramiv
	#undef glGetProgramInfoLog
	#undef glUseProgram
	#undef glDeleteProgram
	#define glCreateProgram glCreateProgramObjectARB
	#define glAttachShader glAttachObjectARB
	#define glLinkProgram glLinkProgramARB
	#define GL_LINK_STATUS GL_OBJECT_LINK_STATUS_ARB
	#define glGetProgramiv glGetObjectParameterivARB
	#define glGetProgramInfoLog glGetInfoLogARB
	#define glUseProgram glUseProgramObjectARB
	#define glDeleteProgram glDeleteObjectARB
	
	#undef glGetUniformLocation
	#undef glGetUniformiv
	#undef glUniform1i
	#undef glUniform1iv
	#undef glUniform2iv
	#undef glUniform3iv
	#undef glUniform4iv
	#undef glUniform1f
	#undef glUniform1fv
	#undef glUniform2fv
	#undef glUniform3fv
	#undef glUniform4fv
	#define glGetUniformLocation glGetUniformLocationARB
	#define glGetUniformiv glGetUniformivARB
	#define glUniform1i glUniform1iARB
	#define glUniform1iv glUniform1ivARB
	#define glUniform2iv glUniform2ivARB
	#define glUniform3iv glUniform3ivARB
	#define glUniform4iv glUniform4ivARB
	#define glUniform1f glUniform1fARB
	#define glUniform1fv glUniform1fvARB
	#define glUniform2fv glUniform2fvARB
	#define glUniform3fv glUniform3fvARB
	#define glUniform4fv glUniform4fvARB
	
	#undef glGetAttribLocation
	#undef glVertexAttrib1f
	#undef glVertexAttrib2f
	#undef glVertexAttrib3f
	#undef glVertexAttrib4f
	#undef glVertexAttribI1i
	#undef glVertexAttribI2i
	#undef glVertexAttribI3i
	#undef glVertexAttribI4i
	#undef glVertexAttribI1ui
	#undef glVertexAttribI2ui
	#undef glVertexAttribI3ui
	#undef glVertexAttribI4ui
	#define glGetAttribLocation glGetAttribLocationARB
	#define glVertexAttrib1f glVertexAttrib1fARB
	#define glVertexAttrib2f glVertexAttrib2fARB
	#define glVertexAttrib3f glVertexAttrib3fARB
	#define glVertexAttrib4f glVertexAttrib4fARB
	#define glVertexAttribI1i glVertexAttrib1sARB
	#define glVertexAttribI2i glVertexAttrib2sARB
	#define glVertexAttribI3i glVertexAttrib3sARB
	#define glVertexAttribI4i glVertexAttrib4sARB
	#define glVertexAttribI1ui glVertexAttrib1sARB
	#define glVertexAttribI2ui glVertexAttrib2sARB
	#define glVertexAttribI3ui glVertexAttrib3sARB
	#define glVertexAttribI4ui glVertexAttrib4sARB
#endif




#define GPU_DEFAULT_TEXTURED_VERTEX_SHADER_SOURCE \
"#version 110\n\
\
varying vec4 color;\n\
varying vec2 texCoord;\n\
\
void main(void)\n\
{\n\
	color = gl_Color;\n\
	texCoord = vec2(gl_MultiTexCoord0);\n\
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n\
}"

#define GPU_DEFAULT_UNTEXTURED_VERTEX_SHADER_SOURCE \
"#version 110\n\
\
varying vec4 color;\n\
\
void main(void)\n\
{\n\
	color = gl_Color;\n\
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n\
}"


#define GPU_DEFAULT_TEXTURED_FRAGMENT_SHADER_SOURCE \
"#version 110\n\
\
varying vec4 color;\n\
varying vec2 texCoord;\n\
\
uniform sampler2D tex;\n\
\
void main(void)\n\
{\n\
    gl_FragColor = texture2D(tex, texCoord) * color;\n\
}"

#define GPU_DEFAULT_UNTEXTURED_FRAGMENT_SHADER_SOURCE \
"#version 110\n\
\
varying vec4 color;\n\
\
void main(void)\n\
{\n\
    gl_FragColor = color;\n\
}"


typedef struct ContextData_OpenGL_1
{
	SDL_Color last_color;
	Uint8 last_use_blending;
	GPU_BlendEnum last_blend_mode;
	GPU_Rect last_viewport;
	GPU_Camera last_camera;
	
	GPU_Image* last_image;
	GPU_Target* last_target;
	float* blit_buffer;  // Holds sets of 4 vertices and 4 tex coords interleaved (e.g. [x0, y0, z0, s0, t0, ...]).
	int blit_buffer_num_vertices;
	int blit_buffer_max_num_vertices;
	unsigned short* index_buffer;  // Indexes into the blit buffer so we can use 4 vertices for every 2 triangles (1 quad)
	int index_buffer_num_vertices;
	int index_buffer_max_num_vertices;
} ContextData_OpenGL_1;

typedef struct RendererData_OpenGL_1
{
	Uint32 handle;
} RendererData_OpenGL_1;

typedef struct ImageData_OpenGL_1
{
	Uint32 handle;
	Uint32 format;
} ImageData_OpenGL_1;

typedef struct TargetData_OpenGL_1
{
	Uint32 handle;
	Uint32 format;
} TargetData_OpenGL_1;



#endif