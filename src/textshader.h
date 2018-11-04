#ifndef CHANNELSPANNER_TEXTSHADER_H
#define CHANNELSPANNER_TEXTSHADER_H

#include <GL/glew.h>

const char* text_vert =
        "#version 330 core\n"
                "\n"
                "in vec4 vertex;\n"
                "out vec2 TexCoords;\n"
                "\n"
                "void main(void) {\n"
                "    gl_Position = vec4(vertex.xy, 0.0, 1.0);\n"
                "    TexCoords = vertex.zw;\n"
                "}\n"
;

const char* text_frag =
        "#version 330 core\n"
                "\n"
                "in vec2 TexCoords;\n"
                "out vec4 color;\n"
                "uniform sampler2D text;\n"
                "uniform vec3 textColor;\n"
                "\n"
                "void main(void) {\n"
                "    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"
                "    color = vec4(textColor, 1.0) * sampled;\n"
                "}\n"
;

GLuint create_shader()
{
   GLuint id = glCreateProgram();

   GLuint v = glCreateShader( GL_VERTEX_SHADER );
   glShaderSource( v, 1, &text_vert, NULL );
   glCompileShader( v );

   GLuint f = glCreateShader( GL_FRAGMENT_SHADER );
   glShaderSource( f, 1, &text_frag, NULL );
   glCompileShader( f );

   glAttachShader( id, v );
   glAttachShader( id, f );

   glLinkProgram( id );

   glDeleteShader( v );
   glDeleteShader( f );

   return id;
}

#endif //CHANNELSPANNER_TEXTSHADER_H
