/*******************************************************************************
#                                                                              #
# OpenCV input plugin                                                          #
# Copyright (C) 2016 Dustin Spicuzza                                           #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

//mjpg_streamer -i "/home/pi/mjpg-streamer/mjpg-streamer-experimental/plugins/input_opencv/input_opencv.so"  -o "output_http.so -w /usr/local/www"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dlfcn.h>
#include <pthread.h>
#include <libfreenect.h>


#include "input_opencv.h"

#include "opencv2/opencv.hpp"

using namespace cv;
using namespace std;

/* private functions and variables to this plugin */
static globals     *pglobal;

typedef struct {
    
    
    
    char *filter_args;
    int fps_set, fps,
        quality_set, quality,
        co_set, co,
        br_set, br,
        sa_set, sa,
        gain_set, gain,
        ex_set, ex;
} context_settings;

// filter functions
typedef bool (*filter_init_fn)(const char * args, void** filter_ctx);
typedef Mat (*filter_init_frame_fn)(void* filter_ctx);
typedef void (*filter_process_fn)(void* filter_ctx, Mat &src, Mat &dst);
typedef void (*filter_free_fn)(void* filter_ctx);



typedef struct {
    pthread_t   worker;
    
    context_settings *init_settings;
    
    void* filter_handle;
    void* filter_ctx;
    
    filter_init_fn filter_init;
    filter_init_frame_fn filter_init_frame;
    filter_process_fn filter_process;
    filter_free_fn filter_free;
    
} context;

void *worker_thread(void *);
void worker_cleanup(void *);

void *workerKinect_thread(void *);
//void workerKinect_cleanup(void *);

#define INPUT_PLUGIN_NAME "OpenCV Input plugin"
static char plugin_name[] = INPUT_PLUGIN_NAME;

static void null_filter(void* filter_ctx, Mat &src, Mat &dst) {
    dst = src;
}



pthread_t   workerKinect;

freenect_context *f_ctx;
freenect_device *f_dev;
void *depth_stored;
int bytecount = 1;
Mat videomat, temp_1c, temp_3c;

pthread_cond_t video_cv;
pthread_mutex_t video_mtx;


void depth_callback(freenect_device *dev, void *depth, uint32_t timestamp) {
    depth_stored = depth;
}


void rgb_callback(freenect_device *dev, void *video, uint32_t timestamp) {
    process_kinect(video);//, depth_stored);
}


Mat video_wait() { 
    pthread_cond_wait(&video_cv, &video_mtx);
    return videomat;
}


void process_kinect(void *video) {//, void *depth) {
if (bytecount == 1) {
        memcpy(temp_1c.data, video, 640*480*bytecount);
        cvtColor(temp_1c, videomat, CV_GRAY2RGB);
    } else {
        memcpy(videomat.data, video, 640*480*bytecount);
        cvtColor(videomat, videomat, CV_BGR2RGB);
    }

    
    pthread_mutex_lock(&video_mtx);

    pthread_cond_broadcast(&video_cv);      // Broadcast to Threaded Listeners (e.g. Driver Station Sender)
    pthread_mutex_unlock(&video_mtx);
}

void kinect_video_rgb() {
    bytecount = 3;
    freenect_stop_video(f_dev);
    freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
    freenect_start_video(f_dev);
}


void kinect_video_ir() {
    bytecount = 1;
    freenect_stop_video(f_dev);
    freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT));
    freenect_start_video(f_dev);
}




void init_cv() {
    temp_1c = Mat(480, 640, CV_8UC1);
    temp_3c = Mat(480, 640, CV_8UC3);
    videomat = Mat(480, 640, CV_8UC3);
    
    //hsl_low = Scalar(0, 16, 0);
    //hsl_high = Scalar(255, 255, 255);
    
    //blur_size = Size(1, 1);
    
    pthread_cond_init(&video_cv, NULL);
    pthread_mutex_init(&video_mtx, NULL);
}

int init_kinect() {

    if (freenect_init(&f_ctx, NULL) < 0) {
        printf("Freenect Framework Initialization Failed!\n");
        return 1;
    }
    freenect_select_subdevices(f_ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
    if (freenect_num_devices(f_ctx) < 1) {
        printf("No Kinect Sensors were detected! %i\n", freenect_num_devices(f_ctx));
        freenect_shutdown(f_ctx);
        return 1;
    }
    if (freenect_open_device(f_ctx, &f_dev, 0) < 0) {
        printf("Could not open Kinect Device\n");
        freenect_shutdown(f_ctx);
        return 1;
    }
    freenect_set_tilt_degs(f_dev, 0);
    
    //freenect_set_depth_callback(f_dev, depth_callback);
    freenect_set_video_callback(f_dev, rgb_callback);
    
    //freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));
    freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT));

    //freenect_start_depth(f_dev);
    freenect_start_video(f_dev);

    return 0;
}

void *workerKinect_thread(void *arg)
{
    while(freenect_process_events(f_ctx) >= 0);
}







static void help() {
    
    fprintf(stderr,
    " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-r | --resolution ]...: the resolution of the video device,\n" \
    "                          can be one of the following strings:\n" \
    "                          ");
    
    resolutions_help("                          ");
    
    fprintf(stderr,
    " [-q | --quality ] .....: set quality of JPEG encoding\n" \
    " ---------------------------------------------------------------\n" \
    " Optional filter plugin:\n" \
    " [ -filter ]............: filter plugin .so\n" \
    " [ -fargs ].............: filter plugin arguments\n" \
    " ---------------------------------------------------------------\n\n"\
    );
}

static context_settings* init_settings() {
    context_settings *settings;
    
    settings = (context_settings*)calloc(1, sizeof(context_settings));
    if (settings == NULL) {
        IPRINT("error allocating context");
        exit(EXIT_FAILURE);
    }
    
    settings->quality = 80;
    return settings;
}

/*** plugin interface functions ***/

/******************************************************************************
Description.: parse input parameters
Input Value.: param contains the command line string and a pointer to globals
Return Value: 0 if everything is ok
******************************************************************************/


int input_init(input_parameter *param, int plugin_no)
{
    const char * device = "default";
    const char *filter = NULL, *filter_args = "";
    int width = 640, height = 480, i, device_idx;
    



    input * in;
    context *pctx;
    context_settings *settings;
    
    pctx = new context();
    
    settings = pctx->init_settings = init_settings();
    pglobal = param->global;
    in = &pglobal->in[plugin_no];
    in->context = pctx;

    param->argv[0] = plugin_name;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    /* parse the parameters */
    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"resolution", required_argument, 0, 0},
            {"q", required_argument, 0, 0},
            {"quality", required_argument, 0, 0},
            {"filter", required_argument, 0, 0},
            {"fargs", required_argument, 0, 0},
            {0, 0, 0, 0}
        };
    
        /* parsing all parameters according to the list above is sufficent */
        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        /* dispatch the given options */
        switch(option_index) {
        /* h, help */
        case 0:
        case 1:
            help();
            return 1;
        /* r, resolution */
        case 2:
        case 3:
            DBG("case 2,3\n");
            parse_resolution_opt(optarg, &width, &height);
            break;
        
        /* q, quality */
        case 4:
            OPTION_INT(5, quality)
            settings->quality = MIN(MAX(settings->quality, 0), 100);
            break;
        /* filter */
        case 6:
            filter = optarg;
            break;
            
        /* fargs */
        case 7:
            filter_args = optarg;
            break;
            
        default:
            help();
            return 1;
        }
    }

    IPRINT("Desired Resolution: %i x %i\n", width, height);




    //init_cv();

    //init_kinect();






    /* filter stuff goes here */
    if (filter != NULL) {
        
        IPRINT("filter........... : %s\n", filter);
        IPRINT("filter args ..... : %s\n", filter_args);
        
        pctx->filter_handle = dlopen(filter, RTLD_LAZY);
        if(!pctx->filter_handle) {
            LOG("ERROR: could not find input plugin\n");
            LOG("       Perhaps you want to adjust the search path with:\n");
            LOG("       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
            LOG("       dlopen: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_init = (filter_init_fn)dlsym(pctx->filter_handle, "filter_init");
        if (pctx->filter_init == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_process = (filter_process_fn)dlsym(pctx->filter_handle, "filter_process");
        if (pctx->filter_process == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_free = (filter_free_fn)dlsym(pctx->filter_handle, "filter_free");
        if (pctx->filter_free == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        // optional functions
        pctx->filter_init_frame = (filter_init_frame_fn)dlsym(pctx->filter_handle, "filter_init_frame");
        
        // initialize it
        if (!pctx->filter_init(filter_args, &pctx->filter_ctx)) {
            goto fatal_error;
        }
        
    } else {
        pctx->filter_handle = NULL;
        pctx->filter_ctx = NULL;
        pctx->filter_process = null_filter;
        pctx->filter_free = NULL;
    }



    init_cv();
    if(init_kinect()  == 1) {
        printf("ERROR with INIT KINECT");
        return 1;
    } 
    kinect_video_ir();


    return 0;
    
fatal_error:
    worker_cleanup(in);
    closelog();
    exit(EXIT_FAILURE);



    
}

/******************************************************************************
Description.: stops the execution of the worker thread
Input Value.: -
Return Value: 0
******************************************************************************/
int input_stop(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    if (pctx != NULL) {
        DBG("will cancel input thread\n");
        pthread_cancel(pctx->worker);
        pthread_cancel(workerKinect);
    }
    return 0;
}

/******************************************************************************
Description.: starts the worker thread and allocates memory
Input Value.: -
Return Value: 0
******************************************************************************/
int input_run(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    in->buf = NULL;
    in->size = 0;
    
    if(pthread_create(&pctx->worker, 0, worker_thread, in) != 0) {
        worker_cleanup(in);
        fprintf(stderr, "could not start worker thread\n");
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&workerKinect, 0, workerKinect_thread, in) != 0) {
        //workerKinect_cleanup(in);
        IPRINT("could not start workerKinect thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(pctx->worker);
    pthread_detach(workerKinect);
    
    return 0;
}

void *worker_thread(void *arg)
{
    input * in = (input*)arg;
    context *pctx = (context*)in->context;
    context_settings *settings = (context_settings*)pctx->init_settings;
    
    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, arg);

    
    /* setup imencode options */
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    compression_params.push_back(settings->quality); // 1-100
    
    free(settings);
    pctx->init_settings = NULL;
    settings = NULL;
    
    Mat src, dst;
    vector<uchar> jpeg_buffer;
    
    // this exists so that the numpy allocator can assign a custom allocator to
    // the mat, so that it doesn't need to copy the data each time
    if (pctx->filter_init_frame != NULL)
        src = pctx->filter_init_frame(pctx->filter_ctx);
    //////////////////////////////////////////////////
    //src = videomat;
    
    while (!pglobal->stop) {

        src = video_wait();

        // call the filter function
        pctx->filter_process(pctx->filter_ctx, src, dst);
        

        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&in->db);
        
        // take whatever Mat it returns, and write it to jpeg buffer
        imencode(".jpg", dst, jpeg_buffer, compression_params);
        
        // TODO: what to do if imencode returns an error?
        
        // std::vector is guaranteed to be contiguous
        in->buf = &jpeg_buffer[0];
        in->size = jpeg_buffer.size();
        
        /* signal fresh_frame */
        pthread_cond_broadcast(&in->db_update);
        pthread_mutex_unlock(&in->db);



    }
    
    IPRINT("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
Description.: this functions cleans up allocated resources
Input Value.: arg is unused
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    input * in = (input*)arg;
    if (in->context != NULL) {
        context *pctx = (context*)in->context;
        
        if (pctx->filter_free != NULL && pctx->filter_ctx != NULL) {
            pctx->filter_free(pctx->filter_ctx);
            pctx->filter_free = NULL;
        }
        
        if (pctx->filter_handle != NULL) {
            dlclose(pctx->filter_handle);
            pctx->filter_handle = NULL;
        }
        
        delete pctx;
        in->context = NULL;
    }
}
