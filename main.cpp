#include <unistd.h>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <csignal>
#include "rplidar.h"
#include "Com/DataSocket.hpp"

#define DEBUG true

#ifndef _countof
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

#ifdef _WIN32
#include <Windows.h>
#define delay(x)   ::Sleep(x)
#else
#include <unistd.h>
static inline void delay(_word_size_t ms){
    while (ms>=1000){
        usleep(1000*1000);
        ms-=1000;
    };
    if (ms!=0)
        usleep(ms*1000);
}
#endif

using namespace rp::standalone::rplidar;

static const char* SERVER_ADDRESS=	"127.0.0.1";
static const uint16_t SERVER_PORT=		17685;
using namespace data_wrappers;
using namespace std;

bool running=true; //Variable to manage stops when signals are received
void signal_handler(int signo){
	if(signo==SIGTERM || signo==SIGINT)
		running = false;
}

bool checkRPLIDARHealth(RPlidarDriver * drv)
{
    u_result     op_result;
    rplidar_response_device_health_t healthinfo;


    op_result = drv->getHealth(healthinfo);
    if (IS_OK(op_result)) { // the macro IS_OK is the preperred way to judge whether the operation is succeed.
        printf("RPLidar health status : %d\n", healthinfo.status);
        if (healthinfo.status == RPLIDAR_STATUS_ERROR) {
            fprintf(stderr, "Error, rplidar internal error detected. Please reboot the device to retry.\n");
            // enable the following code if you want rplidar to be reboot by software
            // drv->reset();
            return false;
        } else {
            return true;
        }

    } else {
        fprintf(stderr, "Error, cannot retrieve the lidar health code: %x\n", op_result);
        return false;
    }
}


#include <signal.h>
bool ctrl_c_pressed;
void ctrlc(int)
{
    ctrl_c_pressed = true;
}

int main(int argc, char** argv){
	/* ************************************
	*    SETUP LIDAR & CHECK STATUS  *
	**************************************/
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	running=true;

	const char * opt_com_path = NULL;
	opt_com_path = "/dev/ttyUSB0";
	_u32         baudrateArray[2] = {115200, 256000};
	_u32         opt_com_baudrate = 0;
	u_result     op_result;


	rplidar_response_device_info_t devinfo;
	bool connectSuccess = false;
	// make connection...

	DataSocket HL(SERVER_ADDRESS, SERVER_PORT); //Connection to the client
	//RPLidar lidar(argc>1?argv[argc - 1]:"/dev/ttyUSB0"); //Connects to lidar
	RPlidarDriver * drv = RPlidarDriver::CreateDriver(DRIVER_TYPE_SERIALPORT);
	if (!drv) {
		fprintf(stderr, "insufficent memory, exit\n");
		exit(-2);
	}
size_t baudRateArraySize = (sizeof(baudrateArray))/ (sizeof(baudrateArray[0]));
        for(size_t i = 0; i < baudRateArraySize; ++i)
        {
            if(!drv)
                drv = RPlidarDriver::CreateDriver(DRIVER_TYPE_SERIALPORT);
            if(IS_OK(drv->connect(opt_com_path, baudrateArray[i])))
            {
                op_result = drv->getDeviceInfo(devinfo);

                if (IS_OK(op_result)) 
                {
                    connectSuccess = true;
                    break;
                }
                else
                {
                    delete drv;
                    drv = NULL;
                }
            }
        }
	if (!connectSuccess) {
		fprintf(stderr, "Error, cannot bind to the specified serial port %s.\n"
		, opt_com_path);
		goto on_finished;
	}

// print out the device serial number, firmware and hardware version number..
    printf("RPLIDAR S/N: ");
    for (int pos = 0; pos < 16 ;++pos) {
        printf("%02X", devinfo.serialnum[pos]);
    }

    printf("\n"
            "Firmware Ver: %d.%02d\n"
            "Hardware Rev: %d\n"
            , devinfo.firmware_version>>8
            , devinfo.firmware_version & 0xFF
            , (int)devinfo.hardware_version);



	    // check health...
	    if (!checkRPLIDARHealth(drv)) {
		goto on_finished;
	}

	signal(SIGINT, ctrlc);
	data_wrappers::FullScan current_scan;
	//lidar.print_status(); // Print model, health, sampling rates
	//lidar.stop_motor();
	/* ************************************
 	*                   TEST MAIN LOOP             *
 	**************************************/
	while(running) {
		/* ************************************
		 *                   START SCAN                    *
		 **************************************/
		std::cout<<"Waiting for client..."<<std::endl;
		while(!HL.accept_client() && running);
		if(!running) continue;

		std::cout<<" Connected !"<<std::endl;
		drv->stop();
		drv->stopMotor();
		std::cout<<"Motor stopped!"<<std::endl;
		drv->startMotor();
		std::cout<<"Motor started!"<<std::endl;

		//sleep(1);	//Let motor spin
		std::cout<<"Un petit sleep!"<<std::endl;
		// start scan...
		drv->startScan(0, 1);

		std::cout<<"start express scan ok!"<<std::endl;
		int result;
		do{
			rplidar_response_measurement_node_t nodes[8192];
			size_t   count = _countof(nodes);

			//Update current scan (one turn of measurements)
			op_result = drv->grabScanData(nodes, count);
			if (IS_OK(op_result)) {
			    drv->ascendScanData(nodes, count);
			    for (int pos = 0; pos < (int)count ; ++pos) {
				printf("%s theta: %03.2f Dist: %08.2f Q: %d \n", 
				    (nodes[pos].sync_quality & RPLIDAR_RESP_MEASUREMENT_SYNCBIT) ?"S ":"  ", 
				    (nodes[pos].angle_q6_checkbit >> RPLIDAR_RESP_MEASUREMENT_ANGLE_SHIFT)/64.0f,
				    nodes[pos].distance_q2/4.0f,
				    nodes[pos].sync_quality >> RPLIDAR_RESP_MEASUREMENT_QUALITY_SHIFT);
			    }
			}

			// Data processing: obstacle extraction and Kalman(?)

			//TODO

			//Send the data to client, TODO send obstacle data(not full scan)
			result=HL.send_scan(op_result);
			//	std::cout<<result<<std::endl;
		}while(result>=0 && running);
		drv->stop();
		drv->stopMotor(); //Stop motor if already running
	}
	/* ***********************************
	 *                       STOP ALL                    *
	 *************************************/
	drv->stop();
	drv->stopMotor(); /
	return 0;
}
