#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <assert.h>

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
//#include <VG/openvg.h>
//#include <VG/vgu.h>

#include <bcm_host.h>
#include <ilclient.h>
#include "esUtil.h"

typedef struct
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
} EGL_STATE_T;


EGL_STATE_T state, *p_state = &state;

typedef struct
{
   // Handle to a program object
   GLuint programObject;

} UserData;

///
// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader ( GLenum type, const char *shaderSrc )
{
   GLuint shader;
   GLint compiled;
   
   // Create the shader object
   shader = glCreateShader ( type );

   if ( shader == 0 )
   	return 0;

   // Load the shader source
   glShaderSource ( shader, 1, &shaderSrc, NULL );
   
   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if ( !compiled ) 
   {
      GLint infoLen = 0;

      glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
         esLogMessage ( "Error compiling shader:\n%s\n", infoLog );            
         
         free ( infoLog );
      }

      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}

///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
   esContext->userData = malloc(sizeof(UserData));

   UserData *userData = esContext->userData;
   GLbyte vShaderStr[] =  
      "attribute vec4 vPosition;    \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = vPosition;  \n"
      "}                            \n";
   
   GLbyte fShaderStr[] =  
      "precision mediump float;\n"\
      "void main()                                  \n"
      "{                                            \n"
      "  gl_FragColor = vec4 ( 1.0, 0.0, 0.0, 0.0 );\n"
      "}                                            \n";

   GLuint vertexShader;
   GLuint fragmentShader;
   GLuint programObject;
   GLint linked;

   // Load the vertex/fragment shaders
   vertexShader = LoadShader ( GL_VERTEX_SHADER, vShaderStr );
   fragmentShader = LoadShader ( GL_FRAGMENT_SHADER, fShaderStr );

   // Create the program object
   programObject = glCreateProgram ( );
   
   if ( programObject == 0 )
      return 0;

   glAttachShader ( programObject, vertexShader );
   glAttachShader ( programObject, fragmentShader );

   // Bind vPosition to attribute 0   
   glBindAttribLocation ( programObject, 0, "vPosition" );

   // Link the program
   glLinkProgram ( programObject );

   // Check the link status
   glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );

   if ( !linked ) 
   {
      GLint infoLen = 0;

      glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );
      
      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
         esLogMessage ( "Error linking program:\n%s\n", infoLog );            
         
         free ( infoLog );
      }

      glDeleteProgram ( programObject );
      return GL_FALSE;
   }

   // Store the program object
   userData->programObject = programObject;

   glClearColor ( 0.0f, 0.0f, 0.0f, 0.0f );
   return GL_TRUE;
}

///
// Draw a triangle using the shader pair created in Init()
//
float incr = 0.0f;
void Draw ( ESContext *esContext )
{
   UserData *userData = esContext->userData;
   GLfloat vVertices[] = {  0.0f-incr,  0.5f-incr, 0.0f+incr, 
                           -0.5f, -0.5f, 0.0f,
                            0.5f, -0.5f, 0.0f };
    
   incr += 0.01f;  
   // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   // Use the program object
   glUseProgram ( userData->programObject );

   // Load the vertex data
   glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, vVertices );
   glEnableVertexAttribArray ( 0 );

   glDrawArrays ( GL_TRIANGLES, 0, 3 );
}

#define IMG  "cimg0135.jpg"
//#define IMG "hype.jpg"

void printState(OMX_HANDLETYPE handle) {
    // elided
}

char *err2str(int err) {
    return "error elided";
}

void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
    fprintf(stderr, "Got eos event\n");
}

void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
    fprintf(stderr, "OMX error %s\n", err2str(data));
}

int get_file_size(char *fname) {
    struct stat st;

    if (stat(fname, &st) == -1) {
	    perror("Stat'ing img file");
	    return -1;
	}
    return(st.st_size);
}

unsigned int uWidth;
unsigned int uHeight;

OMX_ERRORTYPE read_into_buffer_and_empty(FILE *fp,
					 COMPONENT_T *component,
					 OMX_BUFFERHEADERTYPE *buff_header,
					 int *toread) {
    OMX_ERRORTYPE r;

    int buff_size = buff_header->nAllocLen;
    int nread = fread(buff_header->pBuffer, 1, buff_size, fp);

    printf("Read %d\n", nread);

    buff_header->nFilledLen = nread;
    *toread -= nread;
    if (*toread <= 0) {
	printf("Setting EOS on input\n");
	buff_header->nFlags |= OMX_BUFFERFLAG_EOS;
    }
    r = OMX_EmptyThisBuffer(ilclient_get_handle(component),
			buff_header);
    if (r != OMX_ErrorNone) {
	fprintf(stderr, "Empty buffer error %s\n",
		err2str(r));
    }
    return r;
}

static void set_image_decoder_input_format(COMPONENT_T *component) {
   // set input image format
    printf("Setting image decoder format\n");
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    //setHeader(&imagePortFormat,  sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;

    imagePortFormat.nPortIndex = 320;
    imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    OMX_SetParameter(ilclient_get_handle(component),
                     OMX_IndexParamImagePortFormat, &imagePortFormat);

}

void setup_decodeComponent(ILCLIENT_T  *handle, 
			   char *decodeComponentName, 
			   COMPONENT_T **decodeComponent) {
    int err;

    err = ilclient_create_component(handle,
                                decodeComponent,
                                decodeComponentName,
                                ILCLIENT_DISABLE_ALL_PORTS
				    |
				    ILCLIENT_ENABLE_INPUT_BUFFERS
				    /* |
				    ILCLIENT_ENABLE_OUTPUT_BUFFERS
				    */
                                );
    if (err == -1) {
	fprintf(stderr, "DecodeComponent create failed\n");
	exit(1);
    }
    printState(ilclient_get_handle(*decodeComponent));

    err = ilclient_change_component_state(*decodeComponent,
					  OMX_StateIdle);
    if (err < 0) {
	fprintf(stderr, "Couldn't change state to Idle\n");
	exit(1);
    }
    printState(ilclient_get_handle(*decodeComponent));

    // must be before we enable buffers
    set_image_decoder_input_format(*decodeComponent);
}

void setup_renderComponent(ILCLIENT_T  *handle, 
			      char *renderComponentName, 
			      COMPONENT_T **renderComponent) {
    int err;

   err = ilclient_create_component(handle,
                                renderComponent,
                                renderComponentName,
                                ILCLIENT_DISABLE_ALL_PORTS
				   /* |
				    ILCLIENT_ENABLE_INPUT_BUFFERS
				   */
                                );
    if (err == -1) {
	fprintf(stderr, "RenderComponent create failed\n");
	exit(1);
    }
    printState(ilclient_get_handle(*renderComponent));

    err = ilclient_change_component_state(*renderComponent,
					  OMX_StateIdle);
    if (err < 0) {
	fprintf(stderr, "Couldn't change state to Idle\n");
	exit(1);
    }
    printState(ilclient_get_handle(*renderComponent));
}

void overlay_gles() {

    ESContext esContext;
    UserData  userData;
    
    //sleep(5);

    esInitContext ( &esContext );
    esContext.userData = &userData;
    
    esCreateWindow ( &esContext, "Hello Triangle", 1920, 1080, ES_WINDOW_RGB );
    
    if ( !Init ( &esContext ) )
	return 0;
    
    esRegisterDrawFunc ( &esContext, Draw );
    
    esMainLoop ( &esContext );
}

draw_openmax_image() {

    int i;
    char *decodeComponentName;
    char *renderComponentName;
    int err;
    ILCLIENT_T  *handle;
    COMPONENT_T *decodeComponent;
    COMPONENT_T *renderComponent;
    FILE *fp = fopen(IMG, "r");
    int toread = get_file_size(IMG);
    OMX_BUFFERHEADERTYPE *buff_header;

    decodeComponentName = "image_decode";
    renderComponentName = "video_render";

    //sleep(5);

    bcm_host_init();

    handle = ilclient_init();
    if (handle == NULL) {
	fprintf(stderr, "IL client init failed\n");
	exit(1);
    }

   if (OMX_Init() != OMX_ErrorNone) {
        ilclient_destroy(handle);
        fprintf(stderr, "OMX init failed\n");
	exit(1);
    }

   ilclient_set_error_callback(handle,
			       error_callback,
			       NULL);
   ilclient_set_eos_callback(handle,
			     eos_callback,
			     NULL);


   setup_decodeComponent(handle, decodeComponentName, &decodeComponent);
   setup_renderComponent(handle, renderComponentName, &renderComponent);
   // both components now in Idle state, no buffers, ports disabled

    // input port
    ilclient_enable_port_buffers(decodeComponent, 320, 
				 NULL, NULL, NULL);
    ilclient_enable_port(decodeComponent, 320);


    err = ilclient_change_component_state(decodeComponent,
					  OMX_StateExecuting);
    if (err < 0) {
	fprintf(stderr, "Couldn't change state to Executing\n");
	exit(1);
    }
    printState(ilclient_get_handle(decodeComponent));


    // Read the first block so that the decodeComponent can get
    // the dimensions of the image and call port settings
    // changed on the output port to configure it
    buff_header = 
	ilclient_get_input_buffer(decodeComponent,
				  320,
				  1 /* block */);
    if (buff_header != NULL) {
	read_into_buffer_and_empty(fp,
				   decodeComponent,
				   buff_header,
				   &toread);

	// If all the file has been read in, then
	// we have to re-read this first block.
	// Broadcom bug?
	if (toread <= 0) {
	    printf("Rewinding\n");
	    // wind back to start and repeat
	    fp = freopen(IMG, "r", fp);
	    toread = get_file_size(IMG);
	}
    }

    // wait for first input block to set params for output port
    err = ilclient_wait_for_event(decodeComponent, 
			    OMX_EventPortSettingsChanged, 
			    321, 0, 0, 1,
			    ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 
			    5);
    if (err < 0) {
	fprintf(stderr, "No port settings changed\n");
    }
    printf("Port settings changed\n");

    TUNNEL_T tunnel;
    set_tunnel(&tunnel, decodeComponent, 321, renderComponent, 90);
    if ((err = ilclient_setup_tunnel(&tunnel, 0, 0)) < 0) {
	fprintf(stderr, "Error setting up tunnel %X\n", err);
	exit(1);
    } else {
	printf("Tunnel set up ok\n");
    }	

   // Okay to go back to processing data
   // enable the decode output ports
   
   OMX_SendCommand(ilclient_get_handle(decodeComponent), 
		   OMX_CommandPortEnable, 321, NULL);
   
   ilclient_enable_port(decodeComponent, 321);

   // enable the render output ports
   /*
   OMX_SendCommand(ilclient_get_handle(renderComponent), 
		   OMX_CommandPortEnable, 90, NULL);
   */
   ilclient_enable_port(renderComponent, 90);

   // set both components to executing state
    err = ilclient_change_component_state(decodeComponent,
					  OMX_StateExecuting);
    if (err < 0) {
	fprintf(stderr, "Couldn't change state to Idle\n");
	exit(1);
    }
    err = ilclient_change_component_state(renderComponent,
					  OMX_StateExecuting);
    if (err < 0) {
	fprintf(stderr, "Couldn't change state to Idle\n");
	exit(1);
    }

    // now work through the file
    while (toread > 0) {
	OMX_ERRORTYPE r;

	// do we have a decode input buffer we can fill and empty?
	buff_header = 
	    ilclient_get_input_buffer(decodeComponent,
				      320,
				      1 /* block */);
	if (buff_header != NULL) {
	    read_into_buffer_and_empty(fp,
				   decodeComponent,
				   buff_header,
				   &toread);
	}
    }

    ilclient_wait_for_event(renderComponent, 
			    OMX_EventBufferFlag, 
			    90, 0, OMX_BUFFERFLAG_EOS, 0,
			    ILCLIENT_BUFFER_FLAG_EOS, 10000);
    printf("EOS on render\n");
}

int main(int argc, char** argv) {

    draw_openmax_image();
    overlay_gles();

    exit(0);
}
