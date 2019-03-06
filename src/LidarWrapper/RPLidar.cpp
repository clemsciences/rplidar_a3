#include <cmath>
#include <iostream>
#include <iomanip>
#include "LidarWrapper/ReturnDataWrappers.hpp"
#include "LidarWrapper/RPLidar.hpp"

using namespace rp_values;
using namespace data_wrappers;

/**
 * Gives the current epoch, in milliseconds
 * @return guess what
 */
double msecs()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (double) tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

RPLidar::RPLidar(const char *serial_path) : port(serial_path, B115200, 0){

}

void RPLidar::print_status(){
	using namespace std;
	//Get data
	InfoData infoData;
	get_info(&infoData);
	HealthData healthData;
	get_health(&healthData);
	SampleRateData sampleRateData;
	get_samplerate(&sampleRateData);

	cout<<"##############################################"<<endl;
	cout<<left<<setw(3)<<"#"<<left<<setw(27)<<"RPLidar Model:"<<setw(15)<<"A"+to_string(infoData.model>>4)+"M"+to_string(infoData.model&0x0F);
	cout<<right<<"#"<<endl;
	cout<<left<<setw(3)<<"#"<<left<<setw(27)<<"Firmware version: "<<setw(15)<<to_string(infoData.firmware_major)+to_string(infoData.firmware_minor);
	cout<<right<<"#"<<endl;
	cout<<left<<setw(3)<<"#"<<left<<setw(27)<<"Lidar Health: "<<setw(15)<<string(healthData.status==rp_values::LidarStatus::LIDAR_OK?"OK":(healthData.status==rp_values::LidarStatus::LIDAR_WARNING?"WARNING":"ERROR"));
	cout<<right<<"#"<<endl;
	cout<<left<<setw(3)<<"#"<<left<<setw(27)<<("Scan sampling period:")<<setw(15)<<to_string(sampleRateData.scan_sample_rate)+"us";
	cout<<right<<"#"<<endl;
	cout<<left<<setw(3)<<"#"<<left<<setw(27)<<("Express sampling period:")<<setw(15)<<to_string(sampleRateData.express_sample_rate)+"us";
	cout<<right<<"#"<<endl;
	cout<<"##############################################"<<endl;

	//Warnings
	if(healthData.status<0){
		cout<<"Warning/Error code:"<<healthData.error_code<<endl;
	}
	if(healthData.status==rp_values::LidarStatus::LIDAR_WARNING){
		cout<<"WARNING: LiDAR in warning state, may deteriorate"<<endl;
	}
	if(healthData.status==rp_values::LidarStatus::LIDAR_ERROR){
		cout<<"ERROR: LiDAR in critical state"<<endl;
	}
}


/**
 * Sends a packet to the LiDAR
 * @param order that is requested to the LiDAR
 * @param payload of the order, in little endian format
 * @return result of the communication
 */
ComResult RPLidar::send_packet(OrderByte order, const std::vector<uint8_t> &payload) {
	RequestPacket requestPacket(order);
	for(uint8_t data : payload){
		requestPacket.add_payload(data);
	}
	uint8_t timeout=0;
	ComResult result;
	do{
		result=port.send_packet(requestPacket);
		if(timeout>0){
			printf("Error: packet send incomplete, try %d/5\n", ++timeout);
		}
	}while(result==STATUS_ERROR && timeout<5);
	if(timeout>=5){
		printf("Error: could not send packet for order %d", order);
		exit(STATUS_ERROR);
	}
	return STATUS_OK;
}


/**
 * Fills a health data struct with required information from LiDAR
 * @param health_data
 * @return
 */
ComResult RPLidar::get_health(HealthData *health_data) {
	ComResult status=send_packet(GET_HEALTH);
	uint32_t data_len= port.read_descriptor();
	uint8_t* health_array=port.read_data(data_len);
	*health_data=HealthData(health_array);
	delete[] health_array;
	return status;
}


/**
 * Fills a LiDAR information data struct with required information from LiDAR
 * @param info_data
 * @return
 */
rp_values::ComResult RPLidar::get_info(data_wrappers::InfoData *info_data) {
	ComResult status=send_packet(GET_INFO);
	uint32_t data_len= port.read_descriptor();
	uint8_t* raw_info=port.read_data(data_len);
	*info_data=InfoData(raw_info);
	delete[] raw_info;
	return status;
}


/**
 * Fills a sample rate data struct with required information from LiDAR
 * @param sample_rate for express scans (it is actually the period un us)
 * @return result of the communication
 */
ComResult RPLidar::get_samplerate(SampleRateData *sample_rate) {
	send_packet(GET_SAMPLERATE);
	uint32_t data_len= port.read_descriptor();
	uint8_t* sampe_rate_data=port.read_data(data_len);
	*sample_rate=SampleRateData(sampe_rate_data);
	delete[] sampe_rate_data;
	return STATUS_OK;
}


/**
 * Requests the start of the motor to the control module
 * As we do not have any way of knowing if it worked, we have to spam a bit.
 * @return result of the communication
 */
ComResult RPLidar::start_motor() {
	for(int i=0;i<NUMBER_PWM_TRIES-1;i++) {
		set_pwm(DEFAULT_MOTOR_PWM);
	}
	return STATUS_OK;
}


/**
 * Requests a stop of the motor to the control module
 * As we do not have any way of knowing if it worked, we have to spam a bit.
 * @return result of the communication
 */
ComResult RPLidar::stop_motor() {
	ComResult status=STATUS_OK;
	for(int i=0;i<NUMBER_PWM_TRIES;i++) {
		status=set_pwm(0)==STATUS_OK?STATUS_OK:STATUS_ERROR;
	}
	return status;
}


/**
 * Send a PWM motor command for speed control
 * @param pwm between 0 and 1023 (inclusive)
 * @return result of the communication
 */
ComResult RPLidar::set_pwm(uint16_t pwm) {
	if(pwm>1023){
		pwm=1023;
	}
	uint8_t bytes[2]={(uint8_t)pwm, (uint8_t)(pwm>>8)}; //Little endian
	return send_packet(SET_PWM, {bytes[0],bytes[1]});
}


/**
 * Requests the start of an express scan.
 * A response descriptor should then be read for data packets length,
 * then the data should be read
 * @return result of the communication
 */
ComResult RPLidar::start_express_scan() {
	send_packet(EXPRESS_SCAN, {0,0,0,0,0});
	uint32_t data_size = port.read_descriptor();
	return data_size==DATA_SIZE_EXPRESS_SCAN?ComResult::STATUS_OK:ComResult::STATUS_ERROR;
}


/**
 * Reads express scan packet into a vector<uint8_t>
 * @param output_data : vector to fill
 * @param size : number of bytes to read
 * @param to_sync : in case of loss of synchronization
 * @return result of the communication
 */
rp_values::ComResult RPLidar::read_scan_data(std::vector<uint8_t> &output_data, uint8_t size, bool to_sync) {
	output_data.clear();
	uint8_t* read_data= nullptr;
	uint8_t n_bytes_to_read=size;
	if(to_sync) {
		read_data = port.read_data(1);
		while (((read_data[0] >> 4) != 0xA)) {
			delete[] read_data;
			read_data = port.read_data(1);
			if (read_data == nullptr) {
				return ComResult::STATUS_ERROR;
			}
		}
		output_data.push_back(read_data[0]);
		delete[] read_data;
		n_bytes_to_read--;
	}
	read_data = port.read_data(n_bytes_to_read);
	if(read_data==nullptr){
		return ComResult::STATUS_ERROR;
	}
	for(uint32_t i=0;i<n_bytes_to_read;i++){
		output_data.push_back(read_data[i]);
	}
	delete[] read_data;
	return STATUS_OK;
}


/**
 * Requests a stop of any scan active
 * @return result of the communication
 */
rp_values::ComResult RPLidar::stop_scan() {
	return send_packet(STOP);
}

/**
 * Function used to compute angle difference (according to doc)
 * @param angle1 : current packet angle
 * @param angle2 : next packet angle
 * @return result of the communication
 */
float AngleDiff(float angle1, float angle2){
	if(angle2>=angle1){
		return angle2-angle1;
	}
	return 360+angle2-angle1;
}


/**
 * Gets the "measurement_id"th measurement in the current packet, needs the next packet also
 * @param scan_packet : current express packet
 * @param next_angle : start angle of next express packet
 * @param measurement_id : id of the measurement needed
 * @return Measurement struct (includes distance and angle of the measurement)
 */
data_wrappers::Measurement RPLidar::get_next_measurement(data_wrappers::ExpressScanPacket &scan_packet, float next_angle, uint8_t measurement_id) {
	uint16_t distance=scan_packet.distances[measurement_id-1];
	float angle=fmodf(scan_packet.start_angle+(AngleDiff(scan_packet.start_angle, next_angle)/32.0f)*measurement_id-scan_packet.d_angles[measurement_id-1], 360.0f);
	return {distance, angle};
}


int8_t RPLidar::check_new_turn(float next_angle, data_wrappers::FullScan &current_scan) {
	return ((next_angle<5) && (current_scan[current_scan.size()-1].angle>355)?1:-1); // Petite marge
}


/**
 * Main Loop for processing express scan data
 */
bool RPLidar::process_express_scans(FullScan &current_scan) {
	bool error_handling=true;
	bool scan_finished=false;
	bool express_packet_ongoing=true;
	current_scan.clear();										//Reinitialize scan
	bool wrong_flag=false;									//For resynchronization when wrong flags
	std::vector<uint8_t> read_buffer;					//input buffer

	double start_time = msecs();
	for(int i=0;i<10;i++){
		current_scan.measurement_id=32;
		uint8_t last_id=32;
		while (express_packet_ongoing) {
			if (current_scan.measurement_id == 32) {    //If we need to restart a new express packet decoding
				if (last_id == 31) {    //Si on a parcouru une fois le packet actuel, fin
					break;
				}
				current_scan.measurement_id = 0;
				if (current_scan.next_packet.distances.empty()) {    //If there is no next packet (begginning, we only have one packet)
					read_scan_data(read_buffer, DATA_SIZE_EXPRESS_SCAN, wrong_flag);
					rp_values::ComResult result = current_scan.next_packet.decode_packet_bytes(read_buffer);
					if (error_handling) {
						//Error handling
						if (result == rp_values::ComResult::STATUS_WRONG_FLAG) {
							printf("WRONG FLAGS, WILL SYNC BACK\n");
							wrong_flag = true;
							continue; //Couldn't decode an Express packet (wrong flags or checksum)
						} else if (result != rp_values::ComResult::STATUS_OK) {
							printf("WRONG CHECKSUM OR COM ERROR, IGNORE PACKET\n");
							continue;
						}
					}
				}
				current_scan.current_packet = current_scan.next_packet;    //We finished a packet, we go to the next
				read_scan_data(read_buffer, DATA_SIZE_EXPRESS_SCAN, wrong_flag);
				rp_values::ComResult result = current_scan.next_packet.decode_packet_bytes(read_buffer);
				if (error_handling) {
					//Error handling
					if (result == rp_values::ComResult::STATUS_WRONG_FLAG) {
						printf("WRONG FLAGS, WILL SYNC BACK\n");
						wrong_flag = true;
						continue; //Couldn't decode an Express packet (wrong flags or checksum)
					} else if (result != rp_values::ComResult::STATUS_OK) {
						printf("WRONG CHECKSUM OR COM ERROR, IGNORE PACKET\n");
						continue;
					}
					wrong_flag = false;
				}
			}
			//Read the current measurement to process
			last_id = current_scan.measurement_id;
			current_scan.measurement_id++;
			Measurement measurement = get_next_measurement(current_scan.current_packet,
														   current_scan.next_packet.start_angle,
														   current_scan.measurement_id);
			//	Logs
			//	printf("d %u ang %f\n", measurement.distance, measurement.angle);
			current_scan.add_measurement(measurement);
		}
	}
	double duration= msecs()-start_time;
	std::cout<<"Delta_t ="<<duration<<"ms"<<std::endl;
	return true;
}

void RPLidar::print_scan(data_wrappers::FullScan scan) {
	std::cout<<"SCAN: "<<scan.size()<<" values. Content: ";
	for(Measurement& m:scan){
		std::cout<<"[Angle= "<<m.angle<<" Dist= "<<m.distance<<"mm] ";
	}
	std::cout<<std::endl;
}
