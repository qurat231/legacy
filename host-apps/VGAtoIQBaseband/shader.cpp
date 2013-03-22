#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>

using std::stringstream;
using std::cout;
using std::endl;
using std::ends;

#define GL_GLEXT_PROTOTYPES

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#endif

#include "glext.h"

#ifdef _WIN32
#warn untested code
PFNGLCREATESHADERARBPROC pglCreateShaderARB = 0;
PFNGLSHADERSOURCEARBPROC pglShaderSourceARB = 0;
PFNGLCOMPILESHADERARBPROC pglCompileShaderARB = 0;
PFNGLCREATEPROGRAMARBPROC pglCreateProgramARB = 0;
PFNGLATTACHSHADERARBPROC pglAttachShaderARB = 0;
PFNGLLINKPROGRAMARBPROC pglLinkProgramARB = 0;
PFNGLUSEPROGRAMMARBPROC pglUseProgramARB = 0;

#define glCreateShader            pglCreateShaderARB
#define glShaderSource            pglShaderSourceARB
#define glCompileShader           pglCompileShaderARB
#define glCreateProgram           pglCreateProgramARB
#define glAttachShader            pglAttachShaderARB
#define glLinkProgram             pglLinkProgramARB
#define glUseProgram              pglUseProgramARB
#endif

#define MAX_KERNEL_SIZE 31

const char text_fragment_2[] =
"    if( float(gl_TexCoord[0].s - %f*step_w) < step_w )\n"
"    {\n"
"	tmp = texture2D( colorMap, gl_TexCoord[0].st +  vec2(-step_w * %f + 1.0) , -step_h );\n"
"    }else{\n"
"	tmp = texture2D( colorMap, gl_TexCoord[0].st +  vec2(-step_w * %f), 0 );\n"
"    }\n"
"    sum += tmp * %f;\n"
;

const char text_fragment_3[] =
"   gl_FragColor = sum;\n"
"}\n"
;

const char text_fragment_1[] =
"uniform sampler2D colorMap;\n"
"uniform float width;\n"
"uniform float height;\n"

"float step_w = 1.0/width;\n"
"float step_h = 1.0/height;\n"

"void main(void)\n"
"{\n"
"   vec4 sum = vec4(0.0);\n"
"   vec4 tmp;\n"
;

//Got this from http://www.lighthouse3d.com/opengl/glsl/index.php?oglinfo
// it prints out shader info (debugging!)
void printShaderInfoLog(GLuint obj, bool beVerbose)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
        if( beVerbose && charsWritten ){
		cout << "printShaderInfoLog:" << infoLog << endl;
	}
	else
	{
		if( beVerbose )
			cout << "Shader Info Log: OK" << endl;
	}
	free(infoLog);
    }
}

//Got this from http://www.lighthouse3d.com/opengl/glsl/index.php?oglinfo
// it prints out shader info (debugging!)
void printProgramInfoLog(GLuint obj, bool beVerbose)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        if( beVerbose && charsWritten )
		cout << "printProgramInfoLog: " << infoLog << endl;
    	free(infoLog);    
    }
    else
    {
    	    if( beVerbose )
		cout << "Program Info Log: OK" << endl;
    }
}

GLuint f,p;			//Handlers for our programm and fragment shaders

//Setup shaders
void setShaders(float width, float height, bool beVerbose, float max_freq, float sample_rate, int conv_depth) 
{

#ifdef _WIN32
	glCreateShader = (PFNGLCREATESHADERARBPROC)wglGetProcAddress("glCreateShaderARB");
        glShaderSource = (PFNGLSHADERSOURCEARBPROC)wglGetProcAddress("glShaderSourceARB");
        glCompileShader = (PFNGLCOMPILESHADERARBPROC)wglGetProcAddress("glCompileShaderARB");
        glCreateProgram = (PFNGLCREATEPROGRAMARBPROC)wglGetProcAddress("glCreateProgramARB");
        glAttachShader = (PFNGLATTACHSHADERARBPROC)wglGetProcAddress("glAttachShaderARB");
        glLinkProgram = (PFNGLLINKPROGRAMARBPROC)wglGetProcAddress("glLinkProgramARB");
        glUseProgram = (PFNGLUSEPROGRAMMARBPROC)wglGetProcAddress("glUseProgramARB");
#endif

	float sinc[MAX_KERNEL_SIZE];
	float sum;
	int i, len;
	char *buf;
	char *tmp;

	if( conv_depth > MAX_KERNEL_SIZE )
		conv_depth = MAX_KERNEL_SIZE;
	
	if( (conv_depth & 1) == 0 )
		cout << "even conv_depth specified, this might not work" << endl;
	
	/* sinc(max_freq * pi * n) = 0, for n = 1,2,3... */
	sinc[(conv_depth-1)/2]=1;
	//	if( beVerbose )
	//	cout << sample_rate << " " << max_freq << endl;
	for( i=1; i < (conv_depth+1)/2 ;i++ )
	{
		sinc[(conv_depth-1)/2 - i] = sinc[(conv_depth-1)/2 + i] = sin( 3.1415926f * (2*max_freq/sample_rate) * (float)i ) / (3.1415926f * (2*max_freq/sample_rate) * (float)i);
	}
	sum = 0.0f;
	for( i=0; i<conv_depth; i++)
	{
		sum += sinc[i];
	}
	//if( beVerbose )
	//	cout << "sinc filter: " << endl;
	
	for( i=0; i<conv_depth; i++)
	{
		sinc[i] /= sum;
	//	if( beVerbose )
	//		cout << "sinc " << i <<" : " << sinc[i] << endl;
		
	}
	
	//First, create our shaders 
	f = glCreateShader( GL_FRAGMENT_SHADER );

	len = sizeof( text_fragment_1 ) + (sizeof( text_fragment_2 )+40)*conv_depth + sizeof( text_fragment_3 );
	tmp = (char *) malloc( (sizeof( text_fragment_2 )+40)*conv_depth );
	buf = (char *)calloc( len, 1 );
	std::strcat( buf, text_fragment_1 );
	for( i=0;i<conv_depth; i++ ){	
		std::snprintf( tmp, (sizeof( text_fragment_2 )+40)*conv_depth, text_fragment_2, (float)i,(float)i,(float)i,sinc[i] );
		std::strcat( buf, tmp );
	}
	std::strcat( buf,text_fragment_3 );
	//if( beVerbose )
	//	cout << "fragment shader:" << endl;
	//	cout << buf << endl;
		
        //char filename[] = "fragment.glsl";
        
	//Read in the programs
	//fs = text_fragment;
	//fs = textFileRead(&filename[0]);

	//Setup a few constant pointers for below
	const char * ff = buf;

	glShaderSource( f, 1, &ff, NULL );
	free( buf );
	free( tmp );
	//free(fs);

	glCompileShader( f );

	p = glCreateProgram();

	glAttachShader( p,f );
        
	glLinkProgram( p );
	glUseProgram( p );
        GLint myLoc = glGetUniformLocation( p, "width" );
        glUniform1f( myLoc, width );

        myLoc = glGetUniformLocation( p, "height" );
        glUniform1f( myLoc, height );

	printShaderInfoLog( f, beVerbose );
	printProgramInfoLog( p, beVerbose );
}
