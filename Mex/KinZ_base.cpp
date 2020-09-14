///////////////////////////////////////////////////////////////////////////
///		KinZ_base.cpp
///
///		Description: 
///			KinZ class implementations.
///			Define methods to initialize, and get images from sensor.
///			It uses Kinect for Azure SDK from Microsoft.
///
///		Usage:
///			Run demos
///
///		Authors: 
///			Juan R. Terven
///         Diana M. Cordova
///
///     Citation:
///		
///		Creation Date: March/21/2020
///     Modifications: 
///         Mar/21/2020: Start the project
///////////////////////////////////////////////////////////////////////////
#include "KinZ.h"
#include "mex.h"
#include "class_handle.hpp"
#include <vector>
#include <memory>

 // Constructor
KinZ::KinZ(uint16_t sources)
{
    m_flags = (kz::Flags)sources;
    
    // Initialize Kinect
    init();
} // end constructor
        
// Destructor. Release all buffers
KinZ::~KinZ()
{    
    if (m_device != NULL) {
        k4a_device_close(m_device);
        m_device = NULL;
    }
    if (m_image_c != NULL) {
        k4a_image_release(m_image_c);
        m_image_c = NULL;
    }
    if (m_image_d != NULL) {
        k4a_image_release(m_image_d);
        m_image_d = NULL;
    }
    if (m_capture != NULL) {
        k4a_capture_release(m_capture);
        m_capture = NULL;
    }    

    mexPrintf("Kinect Object destroyed\n");

} // end destructor

///////// Function: init ///////////////////////////////////////////
// Initialize Kinect2 and frame reader
//////////////////////////////////////////////////////////////////////////
void KinZ::init()
{
    mexPrintf("Flags inside init: %d", m_flags);

    uint32_t m_device_count = k4a_device_get_installed_count();
    if (m_device_count == 0) {
        mexPrintf("No K4A m_devices found\n");
        return;
    }

    if (K4A_RESULT_SUCCEEDED != k4a_device_open(0, &m_device)) {
        mexPrintf("Failed to open m_device\n");
        if (m_device != NULL) {
            k4a_device_close(m_device);
            m_device = NULL;
            return;
        }
    }

    // Get serial number
    //size_t serial_size = 0;
    //k4a_device_get_serialnum(m_device, NULL, &serial_size);

    // Allocate memory for the serial, then acquire it
    //char *serial = (char*)(malloc(serial_size));
    //k4a_device_get_serialnum(m_device,serial,&serial_size);
    // printf("Opened device SN: %s\n",serial);
    //this->m_serial_number = serial;
    //free(serial);

    k4a_fps_t kin_fps = K4A_FRAMES_PER_SECOND_30;
    k4a_device_configuration_t m_config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    m_config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    m_config.synchronized_images_only = true;
    m_config.camera_fps = kin_fps;

    if (m_flags & kz::C720)
        m_config.color_resolution = K4A_COLOR_RESOLUTION_720P;
    else if (m_flags & kz::C1080)
        m_config.color_resolution = K4A_COLOR_RESOLUTION_1080P;
    else if (m_flags & kz::C1440)
        m_config.color_resolution = K4A_COLOR_RESOLUTION_1440P;
    else if (m_flags & kz::C1536)
        m_config.color_resolution = K4A_COLOR_RESOLUTION_1536P;
    else if (m_flags & kz::C2160)
        m_config.color_resolution = K4A_COLOR_RESOLUTION_2160P;
    else if (m_flags & kz::C3072) {
        m_config.color_resolution = K4A_COLOR_RESOLUTION_3072P;
        m_config.camera_fps = K4A_FRAMES_PER_SECOND_15;
    }

    bool wide_fov = false;
    bool binned = false;

    if (m_flags & kz::D_BINNED)
        binned = true;

    if (m_flags & kz::D_WFOV)
        wide_fov = true;
    
    if(wide_fov && binned) {
        m_config.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
        mexPrintf("K4A_DEPTH_MODE_WFOV_2X2BINNED\n");
    }
    if(wide_fov && !binned) {
        m_config.depth_mode = K4A_DEPTH_MODE_WFOV_UNBINNED;
        m_config.camera_fps = K4A_FRAMES_PER_SECOND_5;
        mexPrintf("K4A_DEPTH_MODE_WFOV_UNBINNED\n");
    }
    if(!wide_fov && binned) {
        m_config.depth_mode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
        mexPrintf("K4A_DEPTH_MODE_NFOV_2X2BINNED\n");
    }
    if(!wide_fov && !binned) {
        m_config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        mexPrintf("K4A_DEPTH_MODE_NFOV_UNBINNED\n");
    }

    // Get calibration
    if (K4A_RESULT_SUCCEEDED !=
        k4a_device_get_calibration(m_device, m_config.depth_mode, m_config.color_resolution, &m_calibration)) {
        printf("Failed to get calibration\n");
        if (m_device) {
            k4a_device_close(m_device);
            m_device = NULL;
            return;
        }
    }

    // get transformation to map from depth to color
    m_transformation = k4a_transformation_create(&m_calibration);

    if (K4A_RESULT_SUCCEEDED != k4a_device_start_cameras(m_device, &m_config)) {
        mexPrintf("Failed to start m_device\n");
        if (m_device) {
            k4a_device_close(m_device);
            m_device = NULL;
            return;
        }
    }
    else
        mexPrintf("Kinect for Azure started successfully!!\n");

    // Activate IMU sensors
    m_imu_sensors_available = false;
    if (m_flags & kz::IMU_ON) {
        if(k4a_device_start_imu(m_device) == K4A_RESULT_SUCCEEDED) {
            printf("IMU sensors started succesfully.");
            m_imu_sensors_available = true;
        }
        else {
            printf("IMU SENSORES FAILED INITIALIZATION");
            m_imu_sensors_available = false;
        }
    }
    else {
        mexPrintf("NOT IMU ON!");
    }
} // end init

///////// Function: updateData ///////////////////////////////////////////
// Get current data from Kinect and save it in the member variables
//////////////////////////////////////////////////////////////////////////
void KinZ::updateData(uint16_t capture_flags, uint8_t valid[])
{
    // Release images before next acquisition
    if (m_capture) {
        k4a_capture_release(m_capture);
        m_capture = NULL;
    }
    if (m_image_c) {
        k4a_image_release(m_image_c);
        m_image_c = NULL;
    }
    if (m_image_d) {
        k4a_image_release(m_image_d);
        m_image_d = NULL;
    }
    if (m_image_ir) {
        k4a_image_release(m_image_ir);
        m_image_ir = NULL;
    }
    
    // Get a m_capture
    bool newCapture;
    switch (k4a_device_get_capture(m_device, &m_capture, TIMEOUT_IN_MS)) {
        case K4A_WAIT_RESULT_SUCCEEDED:
            newCapture = true;
            break;
        case K4A_WAIT_RESULT_TIMEOUT:
            mexPrintf("Timed out waiting for a m_capture\n");
            newCapture = false;
            break;
        case K4A_WAIT_RESULT_FAILED:
            mexPrintf("Failed to read a m_capture\n");
            newCapture = false;
            // mexPrintf("Restarting streaming ...");
            // k4a_device_stop_cameras	(m_device);	
            // if(K4A_RESULT_SUCCEEDED != k4a_device_start_cameras(m_device, &m_config)) {
            //     mexPrintf("Failed to restart streaming\n");
            //     k4a_device_stop_cameras	(m_device);	
            // }
            break;
    }

    bool newDepthData = false;
    bool newColorData = false;
    bool newInfraredData = false;

    if(newCapture) {
        // Get Depth frame
        newDepthData = true;
        if (capture_flags & kz::DEPTH) {
            m_image_d = k4a_capture_get_depth_image(m_capture);
            if (m_image_d == NULL) {
                newDepthData = false;
                mexPrintf("Could not read depth image\n");
            }
        }

        // Get Color frame
        newColorData = true;
        if (capture_flags & kz::COLOR) {
            m_image_c = k4a_capture_get_color_image(m_capture);
            if (m_image_c == NULL) {
                newColorData = false;
                mexPrintf("Could not read color image\n");
            }
        }
        
        // Get IR image
        newInfraredData = true;
        if (capture_flags & kz::INFRARED) {
            m_image_ir = k4a_capture_get_ir_image(m_capture);
            if (m_image_ir == NULL) {
                newInfraredData = false;
                mexPrintf("Could not read IR image");
            }
        }
    }

    if((capture_flags & kz::IMU_ON) && m_imu_sensors_available) {
        //mexPrintf("Updating sensor data");
        k4a_imu_sample_t imu_sample;

        // Capture a imu sample
        k4a_wait_result_t imu_status;
        imu_status = k4a_device_get_imu_sample(m_device, &imu_sample, TIMEOUT_IN_MS);
        switch (imu_status)
        {
        case K4A_WAIT_RESULT_SUCCEEDED:
            break;
        case K4A_WAIT_RESULT_TIMEOUT:
            printf("Timed out waiting for a imu sample\n");
            break;
        case K4A_WAIT_RESULT_FAILED:
            printf("Failed to read a imu sample\n");
            break;
        }

        // Access the accelerometer readings
        if (imu_status == K4A_WAIT_RESULT_SUCCEEDED)
        {
            m_imu_data.temperature = imu_sample.temperature;
            m_imu_data.acc_x = imu_sample.acc_sample.xyz.x;
            m_imu_data.acc_y = imu_sample.acc_sample.xyz.y;
            m_imu_data.acc_z = imu_sample.acc_sample.xyz.z;
            m_imu_data.acc_timestamp_usec = imu_sample.acc_timestamp_usec;
            m_imu_data.gyro_x = imu_sample.gyro_sample.xyz.x;
            m_imu_data.gyro_y = imu_sample.gyro_sample.xyz.y;
            m_imu_data.gyro_z = imu_sample.gyro_sample.xyz.z;
            m_imu_data.gyro_timestamp_usec = imu_sample.gyro_timestamp_usec;
        }
    }
    /*else {
        mexPrintf("NOT Updating sensor data");
        mexPrintf("%d", capture_flags);
    }*/
    
    if (newDepthData && newColorData && newInfraredData)
        valid[0] = 1;
    else
        valid[0] = 0;
} // end updateData

///////// Function: getColor ///////////////////////////////////////////
// Copy color frame to Matlab matrix
// You must call updateData first
//////////////////////////////////////////////////////////////////////////
void KinZ::getColor(uint8_t rgbImage[], uint64_t& time, bool& validColor)
{
    if(m_image_c) {
        int w = k4a_image_get_width_pixels(m_image_c);
        int h = k4a_image_get_height_pixels(m_image_c);
        int stride = k4a_image_get_stride_bytes(m_image_c);
        uint8_t* dataBuffer = k4a_image_get_buffer(m_image_c);
        int numColorPix = w*h;

        // copy color buffer to Matlab output
        // sweep the entire matrix copying data to output matrix
        int colSize = 4*w;
        for (int x=0, k=0; x < w*4; x+=4)
            for (int y=0; y <h; y++,k++)
            {
                int idx = y * colSize + x;
                rgbImage[k] = dataBuffer[idx+2];
                rgbImage[numColorPix + k] = dataBuffer[idx+1];
                rgbImage[numColorPix*2 + k] = dataBuffer[idx];                   
            }
        validColor = true;
        time = k4a_image_get_system_timestamp_nsec(m_image_c);
    }
    else
        validColor = false;

} // end getColor

///////// Function: getDepth ///////////////////////////////////////////
// Copy depth frame to Matlab matrix
// You must call updateData first
//////////////////////////////////////////////////////////////////////////
void KinZ::getDepth(uint16_t depth[], uint64_t& time, bool& validDepth)
{
    if(m_image_d) {
        int w = k4a_image_get_width_pixels(m_image_d);
        int h = k4a_image_get_height_pixels(m_image_d);
        int stride = k4a_image_get_stride_bytes(m_image_d);
        uint8_t* dataBuffer = k4a_image_get_buffer(m_image_d);

        // Copy Depth frame to output matrix
        int colSize = 2*w;
        for (int x=0, k=0;x<w*2;x+=2)
            for (int y=0;y<h;y++,k++) {
                int idx = y * colSize + x;
                uint16_t lsb, msb;

                lsb = dataBuffer[idx];
                msb = dataBuffer[idx+1];
                depth[k] = msb * 256 + lsb;
            }

        validDepth = true;
        time = k4a_image_get_system_timestamp_nsec(m_image_d);
    }
    else 
        validDepth = false;
} // end getDepth

///////// Function: getDepthAligned ///////////////////////////////////////////
// Copy depth aligned to color frame to Matlab matrix
// You must call updateData first
//////////////////////////////////////////////////////////////////////////
void KinZ::getDepthAligned(uint16_t depth[], uint64_t& time, bool& validDepth)
{
    if(m_image_d) {
        k4a_image_t image_dc;
        if(align_depth_to_color(k4a_image_get_width_pixels(m_image_c),
            k4a_image_get_height_pixels(m_image_c), image_dc)) {
        }
        else
            mexPrintf("Failed to align depth to color\n");


        int w = k4a_image_get_width_pixels(image_dc);
        int h = k4a_image_get_height_pixels(image_dc);
        int stride = k4a_image_get_stride_bytes(image_dc);
        uint8_t* dataBuffer = k4a_image_get_buffer(image_dc);

        // Copy Depth frame to output matrix
        int colSize = 2*w;
        for (int x=0, k=0;x<w*2;x+=2)
            for (int y=0;y<h;y++,k++) {
                int idx = y * colSize + x;
                uint16_t lsb, msb;

                lsb = dataBuffer[idx];
                msb = dataBuffer[idx+1];
                depth[k] = msb * 256 + lsb;
            }

        validDepth = true;
        time = time = k4a_image_get_system_timestamp_nsec(m_image_c);
    }
    else 
        validDepth = false;
} // end getDepthAligned


///////// Function: getColorAligned ///////////////////////////////////////////
// Copy color aligned to depth frame to Matlab matrix
// You must call updateData first
//////////////////////////////////////////////////////////////////////////
void KinZ::getColorAligned(uint8_t color[], uint64_t& time, bool& valid)
{
    if(m_image_d && m_image_c) {
        k4a_image_t image_cd;
        if(align_color_to_depth(k4a_image_get_width_pixels(m_image_d),
            k4a_image_get_height_pixels(m_image_d), image_cd)) {
        }
        else
            mexPrintf("Failed to align color to depth\n");


        int w = k4a_image_get_width_pixels(image_cd);
        int h = k4a_image_get_height_pixels(image_cd);
        int stride = k4a_image_get_stride_bytes(image_cd);
        uint8_t* dataBuffer = k4a_image_get_buffer(image_cd);
        int numColorPix = w*h;

        // Copy frame to output matrix
        int colSize = 4*w;
        for (int x=0, k=0; x < w*4; x+=4)
            for (int y=0; y <h; y++,k++) {
                int idx = y * colSize + x;
                color[k] = dataBuffer[idx+2];
                color[numColorPix + k] = dataBuffer[idx+1];
                color[numColorPix*2 + k] = dataBuffer[idx];                   
            }

        valid = true;
        time = k4a_image_get_system_timestamp_nsec(m_image_d);
    }
    else 
        valid = false;
} // end getColorAligned

///////// Function: getInfrared ///////////////////////////////////////////
// Copy infrared frame to Matlab matrix
// You must call updateData first
///////////////////////////////////////////////////////////////////////////
void KinZ::getInfrared(uint16_t infrared[], uint64_t& time, bool& validInfrared)
{
    if(m_image_ir) {
        int w = k4a_image_get_width_pixels(m_image_ir);
        int h = k4a_image_get_height_pixels(m_image_ir);
        int stride = k4a_image_get_stride_bytes(m_image_ir);
        uint8_t* dataBuffer = k4a_image_get_buffer(m_image_ir);

        // copy dataBuffer to output matrix
        int colSize = 2*w;
        for (int x=0, k=0;x<w*2;x+=2)
            for (int y=0;y<h;y++,k++) {
                int idx = y * colSize + x;
                uint16_t lsb, msb;

                lsb = dataBuffer[idx];
                msb = dataBuffer[idx+1];        
                infrared[k] = msb * 256 + lsb;
            }
        
        validInfrared = true;
        time = k4a_image_get_system_timestamp_nsec(m_image_ir);
    }
    else
        validInfrared = false;
} // end getInfrared

bool KinZ::align_depth_to_color(int width, int height, k4a_image_t &transformed_depth_image){
    if (K4A_RESULT_SUCCEEDED != k4a_image_create(K4A_IMAGE_FORMAT_DEPTH16,
                                                width, height, width * (int)sizeof(uint16_t),
                                                &transformed_depth_image)) {
        mexPrintf("Failed to create aligned depth to color image\n");
        return false;
    }

    if (K4A_RESULT_SUCCEEDED != k4a_transformation_depth_image_to_color_camera(m_transformation,
                                                                            m_image_d,
                                                                            transformed_depth_image)) {
        mexPrintf("Failed to compute aligned depth to color image\n");
        return false;
    }

    return true;
}

bool KinZ::align_color_to_depth(int width, int height, k4a_image_t &transformed_color_image ){
    //k4a_image_t transformed_color_image = NULL;
    if (K4A_RESULT_SUCCEEDED != k4a_image_create(K4A_IMAGE_FORMAT_COLOR_BGRA32,
                                                 width, height, width * 4 * (int)sizeof(uint8_t),
                                                 &transformed_color_image)) {
        mexPrintf("Failed to create aligned color to depth image\n");
        return false;
    }

    if (K4A_RESULT_SUCCEEDED != k4a_transformation_color_image_to_depth_camera(m_transformation,
                                                                               m_image_d,
                                                                               m_image_c,
                                                                               transformed_color_image)) {
        mexPrintf("Failed to compute color to depth image\n");
        return false;
    }

    return true;
}

void KinZ::getCalibration(k4a_calibration_t &calibration) {
    calibration = m_calibration;
}

/** Transforms the depth image into 3 planar images representing X, Y and Z-coordinates of corresponding 3d points.
* Throws error on failure.
*
* \sa k4a_transformation_depth_image_to_point_cloud
*/
bool KinZ::depth_image_to_point_cloud(int width, int height, k4a_image_t &xyz_image) {
    if (K4A_RESULT_SUCCEEDED != k4a_image_create(K4A_IMAGE_FORMAT_CUSTOM,
                                                 width, height,
                                                 width * 3 * (int)sizeof(int16_t),
                                                 &xyz_image)) {
        printf("Failed to create transformed xyz image\n");
        return false;
    }

    k4a_result_t result =
        k4a_transformation_depth_image_to_point_cloud(m_transformation,
                                                      m_image_d,
                                                      K4A_CALIBRATION_TYPE_DEPTH,
                                                      xyz_image);

    if (K4A_RESULT_SUCCEEDED != result) {
        printf("Failed to transform depth image to point cloud!");
        return false;
    }
    return true;
}

///////// Function: getPointCloud ///////////////////////////////////////////
// Get camera points from depth frame and copy them to Matlab matrix
// You must call updateData first and have depth activated
///////////////////////////////////////////////////////////////////////////
void KinZ::getPointCloud(double pointCloud[], unsigned char colors[], 
                         bool color, bool& validData)
{   
    validData = false; 
    if(m_image_d) {
        k4a_image_t point_cloud_image = NULL;
        k4a_image_t color_image = NULL;
        bool valid_color_transform = false;

        // Get the point cloud
        if(depth_image_to_point_cloud(k4a_image_get_width_pixels(m_image_d),
            k4a_image_get_height_pixels(m_image_d), point_cloud_image)) {

            // if the user want color
            if(color) {
                // get the color image same size as depth image
                int depth_image_width_pixels = k4a_image_get_width_pixels(m_image_d);
                int depth_image_height_pixels = k4a_image_get_height_pixels(m_image_d);
                valid_color_transform = align_color_to_depth(depth_image_width_pixels, 
                                                             depth_image_height_pixels,
                                                             color_image);
            }

            int width = k4a_image_get_width_pixels(point_cloud_image);
            int height = k4a_image_get_height_pixels(point_cloud_image);
            int numDepthPoints = width * height;

            int16_t *point_cloud_image_data = (int16_t *)(void *)k4a_image_get_buffer(point_cloud_image);
            uint8_t *color_image_data = k4a_image_get_buffer(color_image);

            for (int i = 0; i < numDepthPoints; i++) {
                int16_t X, Y, Z;  
                X = point_cloud_image_data[3 * i + 0];
                Y = point_cloud_image_data[3 * i + 1];
                Z = point_cloud_image_data[3 * i + 2];

                pointCloud[i] = X;
                pointCloud[i + numDepthPoints] = Y;
                pointCloud[i + numDepthPoints + numDepthPoints] = Z;

                if(color && valid_color_transform) {
                    uint8_t R, G, B;
                    B = color_image_data[4 * i + 0];
                    G = color_image_data[4 * i + 1];
                    R = color_image_data[4 * i + 2];

                    colors[i] = R;
                    colors[i + numDepthPoints] = G;
                    colors[i + numDepthPoints + numDepthPoints] = B;
                }
            }
            validData = true;
        } 
        else {
            pointCloud[0] = 0;
            pointCloud[1] = 0;
            pointCloud[2] = 0;
            mexPrintf("Error getting Pointcloud\n");
        }
    }
}

void KinZ::getSensorData(Imu_sample &imu_data) {
    imu_data = m_imu_data;
}

