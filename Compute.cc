#include <stdio.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
 
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/Xatom.h>



// This includes the new stuff, supplied by the application
#include "GL/glext.h"


#define WIN_WIDTH 800
#define WIN_HEIGHT 600



Display *d_dpy;
Window d_win;
GLXContext d_ctx;


std::string shader_source= R"(
    #version 310 es
    layout(local_size_x = 128) in;
    layout(std430) buffer;

    precision highp float;


    layout(binding = 0) readonly buffer B0 {
      float elements[];
    } input_a;

    layout(binding = 1) readonly buffer B1 {
      float elements[];
    } input_b;

    layout(binding = 2) writeonly buffer B2 {
      float elements[];
    } output_c; 

    void main()
    {
      uint gid =   gl_GlobalInvocationID.x;
      float a = input_a.elements[gid];
      float b = input_b.elements[gid];
      output_c.elements[gid] = a + b ;
    }
)";



GLuint create_program()
{
    auto program = glCreateProgram();
    auto shader = glCreateShader(GL_COMPUTE_SHADER);

    const char* source =  shader_source.c_str();         
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);

    int rvalue;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &rvalue);
    if (!rvalue) {
        fprintf(stderr, "Error in compiling the compute shader\n");
        GLchar log[10240];
        GLsizei length;
        glGetShaderInfoLog(shader, 10239, &length, log);
        fprintf(stderr, "Compiler log:\n%s\n", log);
        exit(40);
    }

    glAttachShader( program, shader );
		glLinkProgram(program);

		glGetProgramiv(program, GL_LINK_STATUS, &rvalue);
    if (!rvalue) {
        fprintf(stderr, "Error in linking compute shader program\n");
        GLchar log[10240];
        GLsizei length;
        glGetProgramInfoLog(program, 10239, &length, log);
        fprintf(stderr, "Linker log:\n%s\n", log);
        exit(41);
    }  

		glUseProgram(program); 

    return program;
}

void checkErrors(std::string desc) {
	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		fprintf(stderr, "OpenGL error in \"%s\": %s (%d)\n", desc.c_str(), gluErrorString(e), e);
		exit(20);
	}
}

void initGL() {
	if (!(d_dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Couldn't open X11 display\n");
		exit(10);
	}

	int attr[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		None
	};

	int scrnum = DefaultScreen(d_dpy);
	Window root = RootWindow(d_dpy, scrnum);
    
	int elemc;
	GLXFBConfig *fbcfg = glXChooseFBConfig(d_dpy, scrnum, NULL, &elemc);
	if (!fbcfg) {
		fprintf(stderr, "Couldn't get FB configs\n");
		exit(11);
	}

	XVisualInfo *visinfo = glXChooseVisual(d_dpy, scrnum, attr);

	if (!visinfo) {
		fprintf(stderr, "Couldn't get a visual\n");
		exit(12);
	}

	// Window parameters
	XSetWindowAttributes winattr;
	winattr.background_pixel = 0;
	winattr.border_pixel = 0;
	winattr.colormap = XCreateColormap(d_dpy, root, visinfo->visual, AllocNone);
	winattr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	printf("Window depth %d, %dx%d\n", visinfo->depth, WIN_WIDTH, WIN_HEIGHT);
	d_win = XCreateWindow(d_dpy, root, -1, -1, WIN_WIDTH, WIN_HEIGHT, 0, 
			visinfo->depth, InputOutput, visinfo->visual, mask, &winattr);

	// OpenGL version 4.3, forward compatible core profile
	int gl3attr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
    };

	d_ctx = glXCreateContextAttribsARB(d_dpy, fbcfg[0], NULL, true, gl3attr);

	if (!d_ctx) {
		fprintf(stderr, "Couldn't create an OpenGL context\n");
		exit(13);
	}

	XFree(visinfo);

	// Setting the window name
	XTextProperty windowName;
	windowName.value = (unsigned char *) "OpenGL compute shader demo";
	windowName.encoding = XA_STRING;
	windowName.format = 8;
	windowName.nitems = strlen((char *) windowName.value);

	XSetWMName(d_dpy, d_win, &windowName);

	XMapWindow(d_dpy, d_win);
	glXMakeCurrent(d_dpy, d_win, d_ctx);
	
	printf("OpenGL:\n\tvendor %s\n\trenderer %s\n\tversion %s\n\tshader language %s\n",
			glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION),
			glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Finding the compute shader extension
	int extCount;
	glGetIntegerv(GL_NUM_EXTENSIONS, &extCount);
	bool found = false;
	for (int i = 0; i < extCount; ++i)
		if (!strcmp((const char*)glGetStringi(GL_EXTENSIONS, i), "GL_ARB_compute_shader")) {
			printf("Extension \"GL_ARB_compute_shader\" found\n");
			found = true;
			break;
		}

	if (!found) {
		fprintf(stderr, "Extension \"GL_ARB_compute_shader\" not found\n");
		exit(14);
	}

	glViewport(0, 0, WIN_WIDTH, WIN_HEIGHT);

	checkErrors("Window init");
}


int main()
{
    // start_gl();

    int length = 1024;
    size_t size = sizeof(float)*length;
    float*pA = (float*)malloc(size);
    if(pA == nullptr){
        printf("malloc error!\n");
        return -1;
    }
    float*pB = (float*)malloc(size);
    if(pB == nullptr){
        printf("malloc error!\n");
        return -1;
    }

    float*pC = (float*)malloc(size);
    if(pC == nullptr){
        printf("malloc error!\n");
        return -1;
    }


    for(int i=0;i<length;i++){
        pA[i] = i;
        pB[i] = i;
    }

    initGL();
    auto program = create_program();

		/*bufferA*/
		GLuint id_A;
		glGenBuffers(1,&id_A);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER,id_A);
		glBufferData(GL_SHADER_STORAGE_BUFFER,size,pA,GL_STATIC_READ);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER,0,id_A,0,size);

		/*bufferB*/
		GLuint id_B;
		glGenBuffers(1,&id_B);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER,id_B);
		glBufferData(GL_SHADER_STORAGE_BUFFER,size,pB,GL_STATIC_READ);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER,1,id_B,0,size);

		/*bufferC*/
		GLuint id_C;
		glGenBuffers(1,&id_C);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER,id_C);
		glBufferData(GL_SHADER_STORAGE_BUFFER,size,pB,GL_STREAM_COPY);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER,2,id_C,0,size);


		glDispatchCompute(length/16,1,1);


		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER,id_C);
		void *p = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,size,GL_MAP_READ_BIT);
		if(p == NULL){
			printf("map buffer error!\n");
		}
		
		memcpy(pC,p,size);

		for(int i=0;i<length;i++){
			if(pC[i] != (pA[i] + pB[i])){
					printf("Error at:%d:(%f,%f,%f)\n",i,pC[i],pA[i] ,pB[i]);
			}
		}

    printf("over！\n"); 

}