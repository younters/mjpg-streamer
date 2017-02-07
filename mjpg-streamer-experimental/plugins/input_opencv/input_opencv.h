#ifndef INPUT_OPENCV_H_
#define INPUT_OPENCV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../../mjpg_streamer.h"
#include "../../utils.h"

int input_init(input_parameter* param, int id);
int input_stop(int id);
int input_run(int id);
int input_cmd(int plugin, unsigned int control_id, unsigned int typecode, int value);

void process_kinect(void *video, void *depth);
void depth_callback(freenect_device *dev, void *depth, uint32_t timestamp);
void rgb_callback(freenect_device *dev, void *rgb, uint32_t timestamp);


#ifdef __cplusplus
}
#endif

#endif /* INPUT_OPENCV_H_ */
