#include <stdexcept>
#include "../sim/Engine.h"
#include "Host_Interface_NVMe.h"
#include "NVM_Transaction_Flash_RD.h"
#include "NVM_Transaction_Flash_WR.h"

namespace SSD_Components
{
Input_Stream_NVMe::~Input_Stream_NVMe()
{
	for (auto &user_request : Waiting_user_requests)
		delete user_request;
	for (auto &user_request : Completed_user_requests)
		delete user_request;
}

Input_Stream_Manager_NVMe::Input_Stream_Manager_NVMe(Host_Interface_Base *host_interface, uint16_t queue_fetch_szie) : Input_Stream_Manager_Base(host_interface), Queue_fetch_size(queue_fetch_szie)
{
}

stream_id_type Input_Stream_Manager_NVMe::Create_new_stream(IO_Flow_Priority_Class::Priority priority_class,
															LHA_type start_logical_sector_address,
															LHA_type end_logical_sector_address,
															uint64_t submission_queue_base_address,
															uint16_t submission_queue_size,
															uint64_t completion_queue_base_address,
															uint16_t completion_queue_size,
															uint32_t queue_numbers_of_the_flow)
{
	if (end_logical_sector_address < start_logical_sector_address)
	{
		PRINT_ERROR("Error in allocating address range to a stream in host interface: the start address should be smaller than the end address.")
	}
	Input_Stream_NVMe *input_stream = new Input_Stream_NVMe(priority_class, start_logical_sector_address, end_logical_sector_address,
															submission_queue_base_address, submission_queue_size, completion_queue_base_address, completion_queue_size, queue_numbers_of_the_flow);

	for(uint32_t i = 0; i < queue_numbers_of_the_flow; i++)
	{
		NVMe_Queue_Pair nvme_queue_pair_;
		nvme_queue_pair_.Submission_queue_base_address = submission_queue_base_address;
		nvme_queue_pair_.Submission_queue_size = submission_queue_size;
		nvme_queue_pair_.Completion_queue_base_address = completion_queue_base_address;
		nvme_queue_pair_.Completion_queue_size = completion_queue_size;
		nvme_queue_pair_.Submission_head = 0;
		nvme_queue_pair_.Submission_head_informed_to_host = 0;
		nvme_queue_pair_.Submission_tail = 0;
		nvme_queue_pair_.Completion_head = 0;
		nvme_queue_pair_.Completion_tail = 0;
		nvme_queue_pair_.Queue_id = i;
		nvme_queue_pair_.Queue_on_the_fly_requests = 0;
		input_stream->queue_pairs.push_back(nvme_queue_pair_);
	}

	for(uint32_t i = 0; i < queue_numbers_of_the_flow; i++)
	{
		std::cout<<"new_stream: with queue  "<<input_stream->queue_pairs[i].Queue_id<<std::endl;
		std::cout<<"new_stream: with queue  "<<input_stream->queue_pairs[i].Submission_queue_base_address<<std::endl;

	}


	this->input_streams.push_back(input_stream);

	std::cout<<"Create_new_stream: "<<input_streams.size()<<std::endl;
	for(uint32_t i = 0; i < queue_numbers_of_the_flow; i++){
		std::cout<<"new_stream: with queue  "<<input_stream->queue_pairs[i].Queue_id<<std::endl;

	}


	return (stream_id_type)(this->input_streams.size() - 1);
}
//LM
inline void Input_Stream_Manager_NVMe::Submission_queue_tail_pointer_update(stream_id_type stream_id , uint16_t queue_id, uint16_t tail_pointer_value)
{
	((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_tail = tail_pointer_value; 

	std::cout<<"Submission_queue_tail_pointer_update: "<<tail_pointer_value<<std::endl;
	std::cout<<"Submission_queue_tail_pointer_update: "<<((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_tail<<std::endl;
	std::cout<<"Queue on the fly requests: "<<((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Queue_on_the_fly_requests<<std::endl;

	if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Queue_on_the_fly_requests < Queue_fetch_size) //8191 for Queue_fetch_size
	{
		((Host_Interface_NVMe *)host_interface)->request_fetch_unit->Fetch_next_request(stream_id, queue_id);
		((Input_Stream_NVMe *)input_streams[stream_id])->Flow_On_the_fly_requests++;
		((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Queue_on_the_fly_requests++;
		((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_head++; //Update submission queue head after starting fetch request
		if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_head == ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_queue_size)
		{ //Circular queue implementation
			((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Submission_head = 0;
		}
	}
}

inline void Input_Stream_Manager_NVMe::Completion_queue_head_pointer_update(stream_id_type stream_id, uint16_t queue_id, uint16_t head_pointer_value)
{
	((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[queue_id].Completion_head = head_pointer_value;

	//If this check is true, then the host interface couldn't send the completion queue entry, since the completion queue was full
	if (((Input_Stream_NVMe *)input_streams[stream_id])->Completed_user_requests.size() > 0)
	{
		User_Request *request = ((Input_Stream_NVMe *)input_streams[stream_id])->Completed_user_requests.front();
		((Input_Stream_NVMe *)input_streams[stream_id])->Completed_user_requests.pop_front();
		inform_host_request_completed(stream_id, request);
	}
}
//LM
inline void Input_Stream_Manager_NVMe::Handle_new_arrived_request(User_Request *request)
{


	((Input_Stream_NVMe *)input_streams[request->Stream_id])->queue_pairs[request->Queue_id].Submission_head_informed_to_host++;
	if (((Input_Stream_NVMe *)input_streams[request->Stream_id])->queue_pairs[request->Queue_id].Submission_head_informed_to_host == ((Input_Stream_NVMe *)input_streams[request->Stream_id])->queue_pairs[request->Queue_id].Submission_queue_size)
	{ //Circular queue implementation
		((Input_Stream_NVMe *)input_streams[request->Stream_id])->queue_pairs[request->Queue_id].Submission_head_informed_to_host = 0;
	}
	if (request->Type == UserRequestType::READ)
	{
		((Input_Stream_NVMe *)input_streams[request->Stream_id])->Waiting_user_requests.push_back(request);
		((Input_Stream_NVMe *)input_streams[request->Stream_id])->STAT_number_of_read_requests++;
		segment_user_request(request);

		((Host_Interface_NVMe *)host_interface)->broadcast_user_request_arrival_signal(request);
	}
	else
	{ //This is a write request
		((Input_Stream_NVMe *)input_streams[request->Stream_id])->Waiting_user_requests.push_back(request);
		((Input_Stream_NVMe *)input_streams[request->Stream_id])->STAT_number_of_write_requests++;
		((Host_Interface_NVMe *)host_interface)->request_fetch_unit->Fetch_write_data(request);
	}
}

inline void Input_Stream_Manager_NVMe::Handle_arrived_write_data(User_Request *request)
{
	segment_user_request(request);
	((Host_Interface_NVMe *)host_interface)->broadcast_user_request_arrival_signal(request);
}

inline void Input_Stream_Manager_NVMe::Handle_serviced_request(User_Request *request)
{

	//LM
	std::cout<<"Host_Interface_NVME::Handle_serviced_request"<<std::endl;
	std::cout<<"The request start LBA is "<<request->Start_LBA<<std::endl;
	std::cout<<"Current time is "<<Simulator->Time()<<std::endl;
	std::cout<<"The stream id is "<<request->Stream_id<<std::endl;
	std::cout<<"The queue id is "<<request->Queue_id<<std::endl;

	stream_id_type stream_id = request->Stream_id;
	uint16_t queue_id = request->Queue_id;
	((Input_Stream_NVMe *)input_streams[request->Stream_id])->Waiting_user_requests.remove(request);
	((Input_Stream_NVMe *)input_streams[stream_id])->Flow_On_the_fly_requests--;
	((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Queue_on_the_fly_requests--;

	std::cout<<"The flow on the fly requests is "<<((Input_Stream_NVMe *)input_streams[stream_id])->Flow_On_the_fly_requests<<std::endl;
	std::cout<<"The queue on the fly requests is "<<((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Queue_on_the_fly_requests<<std::endl;

	DEBUG("** Host Interface: Request #" << request->ID << " from stream #" << request->Stream_id << " is finished")

	//If this is a read request, then the read data should be written to host memory
	if (request->Type == UserRequestType::READ)
	{
		((Host_Interface_NVMe *)host_interface)->request_fetch_unit->Send_read_data(request);
	}

	//there are waiting requests in the submission queue but have not been fetched, due to Queue_fetch_size limit
	if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_head != ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_tail)
	{
		((Host_Interface_NVMe *)host_interface)->request_fetch_unit->Fetch_next_request(stream_id, queue_id);
		std::cout<<"come here 2!!!!!!!!!!!"<<std::endl;
		((Input_Stream_NVMe *)input_streams[stream_id])->Flow_On_the_fly_requests++;
		((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_head++; //Update submission queue head after starting fetch request
		if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_head == ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_queue_size)
		{ //Circular queue implementation
			((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_head = 0;
		}
	}

	//Check if completion queue is full
	if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_head > ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail)
	{
		//completion queue is full
		if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail + 1 == ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_head)
		{
			((Input_Stream_NVMe *)input_streams[stream_id])->Completed_user_requests.push_back(request); //Wait while the completion queue is full
			return;
		}
	}
	else if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail - ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_head == ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_queue_size - 1)
	{
		((Input_Stream_NVMe *)input_streams[stream_id])->Completed_user_requests.push_back(request); //Wait while the completion queue is full
		return;
	}

	inform_host_request_completed(stream_id, request); //Completion queue is not full, so the device can DMA the completion queue entry to the host
	DELETE_REQUEST_NVME(request);
}

uint16_t Input_Stream_Manager_NVMe::Get_submission_queue_depth(stream_id_type stream_id, uint16_t queue_id)
{
	return ((Input_Stream_NVMe *)this->input_streams[stream_id])->queue_pairs[queue_id].Submission_queue_size;
}

uint16_t Input_Stream_Manager_NVMe::Get_completion_queue_depth(stream_id_type stream_id, uint16_t queue_id)
{
	return ((Input_Stream_NVMe *)this->input_streams[stream_id])->queue_pairs[queue_id].Completion_queue_size;
}

IO_Flow_Priority_Class::Priority Input_Stream_Manager_NVMe::Get_priority_class(stream_id_type stream_id)
{
	return ((Input_Stream_NVMe *)this->input_streams[stream_id])->Priority_class;
}

inline void Input_Stream_Manager_NVMe::inform_host_request_completed(stream_id_type stream_id, User_Request *request)
{
	((Request_Fetch_Unit_NVMe *)((Host_Interface_NVMe *)host_interface)->request_fetch_unit)->Send_completion_queue_element(request, ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Submission_head_informed_to_host);
	((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail++; //Next free slot in the completion queue
	//Circular queue implementation
	if (((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail == ((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_queue_size)
	{
		((Input_Stream_NVMe *)input_streams[stream_id])->queue_pairs[request->Queue_id].Completion_tail = 0;
	}
}

void Input_Stream_Manager_NVMe::segment_user_request(User_Request *user_request)
{
	LHA_type lsa = user_request->Start_LBA;
	LHA_type lsa2 = user_request->Start_LBA;
	unsigned int req_size = user_request->SizeInSectors;

	page_status_type access_status_bitmap = 0;
	unsigned int handled_sectors_count = 0;
	unsigned int transaction_size = 0;
	while (handled_sectors_count < req_size)
	{
		//Check if LSA is in the correct range allocted to the stream
		if (lsa < ((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->Start_logical_sector_address || lsa > ((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->End_logical_sector_address)
		{
			lsa = ((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->Start_logical_sector_address + (lsa % (((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->End_logical_sector_address - (((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->Start_logical_sector_address)));
		}
		LHA_type internal_lsa = lsa - ((Input_Stream_NVMe *)input_streams[user_request->Stream_id])->Start_logical_sector_address; //For each flow, all lsa's should be translated into a range starting from zero

		transaction_size = host_interface->sectors_per_page - (unsigned int)(lsa % host_interface->sectors_per_page);
		if (handled_sectors_count + transaction_size >= req_size)
		{
			transaction_size = req_size - handled_sectors_count;
		}
		LPA_type lpa = internal_lsa / host_interface->sectors_per_page;

		page_status_type temp = ~(0xffffffffffffffff << (int)transaction_size);
		access_status_bitmap = temp << (int)(internal_lsa % host_interface->sectors_per_page);

		if (user_request->Type == UserRequestType::READ)
		{
			NVM_Transaction_Flash_RD *transaction = new NVM_Transaction_Flash_RD(Transaction_Source_Type::USERIO, user_request->Stream_id,
																				 transaction_size * SECTOR_SIZE_IN_BYTE, lpa, NO_PPA, user_request, user_request->Priority_class, 0, access_status_bitmap, CurrentTimeStamp);
			user_request->Transaction_list.push_back(transaction);
			input_streams[user_request->Stream_id]->STAT_number_of_read_transactions++;
		}
		else
		{ //user_request->Type == UserRequestType::WRITE
			NVM_Transaction_Flash_WR *transaction = new NVM_Transaction_Flash_WR(Transaction_Source_Type::USERIO, user_request->Stream_id,
																				 transaction_size * SECTOR_SIZE_IN_BYTE, lpa, user_request, user_request->Priority_class, 0, access_status_bitmap, CurrentTimeStamp);
			user_request->Transaction_list.push_back(transaction);
			input_streams[user_request->Stream_id]->STAT_number_of_write_transactions++;
		}

		lsa = lsa + transaction_size;
		handled_sectors_count += transaction_size;
	}
}

Request_Fetch_Unit_NVMe::Request_Fetch_Unit_NVMe(Host_Interface_Base *host_interface) : Request_Fetch_Unit_Base(host_interface), current_phase(0xffff), number_of_sent_cqe(0) {

}


/*
//LM
//request_fetch_unit->Process_pcie_write_message(message->Address, message->Payload, message->Payload_size);
void Request_Fetch_Unit_NVMe::Process_pcie_write_message(uint64_t address, void *payload, unsigned int payload_size)
{

	std::cout<<"Process_pcie_write_message: "<<address<<std::endl;

	Host_Interface_NVMe *hi = (Host_Interface_NVMe *)host_interface;
	uint64_t val = (uint64_t)payload;
	switch (address)
	{
	case SUBMISSION_QUEUE_REGISTER_1:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(0, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_1:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(0, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_2:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(1, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_2:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(1, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_3:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(2, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_3:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(2, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_4:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(3, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_4:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(3, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_5:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(4, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_5:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(4, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_6:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(5, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_6:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(5, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_7:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(6, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_7:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(6, (uint16_t)val);
		break;
	case SUBMISSION_QUEUE_REGISTER_8:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(7, (uint16_t)val);
		break;
	case COMPLETION_QUEUE_REGISTER_8:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(7, (uint16_t)val);
		break;
	default:
		throw std::invalid_argument("Unknown register is written!");
	}
}
*/

//LM new
//request_fetch_unit->Process_pcie_write_message(message->Address, message->Payload, message->Payload_size);
void Request_Fetch_Unit_NVMe::Process_pcie_write_message(uint64_t address, uint16_t queue_id,  void *payload, unsigned int payload_size)
{

	std::cout<<"Process_pcie_write_message: "<<address<<std::endl;
	std::cout<<"the queue id is "<<queue_id<<std::endl;

	Host_Interface_NVMe *hi = (Host_Interface_NVMe *)host_interface;
	uint64_t val = (uint64_t)payload;
	switch (address)
	{
	case FLOW_1_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(0, queue_id, (uint16_t)val);
		break;
	case FLOW_1_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(0,  queue_id, (uint16_t)val);
		break;
	case FLOW_2_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(1,  queue_id, (uint16_t)val);
		break;
	case FLOW_2_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(1,  queue_id, (uint16_t)val);
		break;
	case FLOW_3_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(2,  queue_id, (uint16_t)val);
		break;
	case FLOW_3_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(2,  queue_id, (uint16_t)val);
		break;
	case FLOW_4_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(3,  queue_id, (uint16_t)val);
		break;
	case FLOW_4_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(3,  queue_id, (uint16_t)val);
		break;
	case FLOW_5_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(4,  queue_id,  (uint16_t)val);
		break;
	case FLOW_5_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(4,  queue_id, (uint16_t)val);
		break;
	case FLOW_6_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(5,  queue_id, (uint16_t)val);
		break;
	case FLOW_6_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(5,  queue_id, (uint16_t)val);
		break;
	case FLOW_7_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(6,  queue_id, (uint16_t)val);
		break;
	case FLOW_7_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(6,  queue_id, (uint16_t)val);
		break;
	case FLOW_8_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(7,  queue_id, (uint16_t)val);
		break;
	case FLOW_8_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(7,  queue_id, (uint16_t)val);
		break;
	case FLOW_9_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(8,  queue_id, (uint16_t)val);
		break;
	case FLOW_9_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(8,  queue_id, (uint16_t)val);
		break;
	case FLOW_10_SUBMISSION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Submission_queue_tail_pointer_update(9,  queue_id, (uint16_t)val);
		break;
	case FLOW_10_COMPLETION_QUEUE_REGISTER_BASE:
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Completion_queue_head_pointer_update(9,  queue_id, (uint16_t)val);
		break;
	default:
		throw std::invalid_argument("Unknown register is written!");
	}
}






//LM process_pcie_read_message
void Request_Fetch_Unit_NVMe::Process_pcie_read_message(uint64_t address, uint16_t queue_id,  void *payload, unsigned int payload_size)
{

	std::cout<<"Process_pcie_read_message: "<<address<<std::endl;
	std::cout<<"the queue id is "<<queue_id<<std::endl;
	//sleep(1);

	Host_Interface_NVMe *hi = (Host_Interface_NVMe *)host_interface;
	DMA_Req_Item *dma_req_item = dma_list.front();


	dma_list.pop_front();

	std::cout<<"dma_list.size(): "<<dma_list.size()<<std::endl;
	std::cout<<"current time: "<<Simulator->Time()<<std::endl;


	switch (dma_req_item->Type)
	{
	case DMA_Req_Type::REQUEST_INFO:
	{
		User_Request *new_request = new User_Request;
		new_request->IO_command_info = payload;
		new_request->Stream_id = (stream_id_type)((uint64_t)(dma_req_item->object));
		new_request->Priority_class = ((Input_Stream_Manager_NVMe *)host_interface->input_stream_manager)->Get_priority_class(new_request->Stream_id);
		new_request->STAT_InitiationTime = Simulator->Time();
		new_request->Queue_id = queue_id;
		Submission_Queue_Entry *sqe = (Submission_Queue_Entry *)payload;
		sqe->SQ_ID = queue_id;
		switch (sqe->Opcode)
		{
		case NVME_READ_OPCODE:
			new_request->Type = UserRequestType::READ;
			new_request->Start_LBA = ((LHA_type)sqe->Command_specific[1]) << 31 | (LHA_type)sqe->Command_specific[0]; //Command Dword 10 and Command Dword 11
			new_request->SizeInSectors = sqe->Command_specific[2] & (LHA_type)(0x0000ffff);
			new_request->Size_in_byte = new_request->SizeInSectors * SECTOR_SIZE_IN_BYTE;
			std::cout<<"come here"<<std::endl;
			std::cout<<"The request start LBA is "<<new_request->Start_LBA<<std::endl;

			break;
		case NVME_WRITE_OPCODE:
			new_request->Type = UserRequestType::WRITE;
			new_request->Start_LBA = ((LHA_type)sqe->Command_specific[1]) << 31 | (LHA_type)sqe->Command_specific[0]; //Command Dword 10 and Command Dword 11
			new_request->SizeInSectors = sqe->Command_specific[2] & (LHA_type)(0x0000ffff);
			new_request->Size_in_byte = new_request->SizeInSectors * SECTOR_SIZE_IN_BYTE;
			break;
		default:
			throw std::invalid_argument("NVMe command is not supported!");
		}
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Handle_new_arrived_request(new_request);
		break;
	}
	case DMA_Req_Type::WRITE_DATA:
		COPYDATA(((User_Request *)dma_req_item->object)->Data, payload, payload_size);
		((Input_Stream_Manager_NVMe *)(hi->input_stream_manager))->Handle_arrived_write_data((User_Request *)dma_req_item->object);
		break;
	default:
		break;
	}
	delete dma_req_item;
}
//LM
void Request_Fetch_Unit_NVMe::Fetch_next_request(stream_id_type stream_id, uint16_t queue_id) //LM stream_id corresponding to flow_id
{
	std::cout<<"stream_id: "<<stream_id<<std::endl;
	std::cout<<"fetch next request"<<std::endl;
	std::cout<<"current time: "<<Simulator->Time()<<std::endl;
	std::cout<<"queue_id: "<<queue_id<<std::endl;

	DMA_Req_Item *dma_req_item = new DMA_Req_Item;
	dma_req_item->Type = DMA_Req_Type::REQUEST_INFO;
	dma_req_item->Queue_id = queue_id;
	dma_req_item->object = (void *)(intptr_t)stream_id;
	dma_list.push_back(dma_req_item);

	Host_Interface_NVMe *hi = (Host_Interface_NVMe *)host_interface;
	Input_Stream_NVMe *im = ((Input_Stream_NVMe *)hi->input_stream_manager->input_streams[stream_id]);
	//host_interface->Send_read_message_to_host(im->Submission_queue_base_address + im->Submission_head * sizeof(Submission_Queue_Entry), sizeof(Submission_Queue_Entry));

	host_interface->Send_read_message_to_host(im->queue_pairs[queue_id].Submission_queue_base_address + im->queue_pairs[queue_id].Submission_head * sizeof(Submission_Queue_Entry), queue_id,  sizeof(Submission_Queue_Entry));

}

void Request_Fetch_Unit_NVMe::Fetch_write_data(User_Request *request)
{
	DMA_Req_Item *dma_req_item = new DMA_Req_Item;
	dma_req_item->Type = DMA_Req_Type::WRITE_DATA;
	dma_req_item->object = (void *)request;
	dma_list.push_back(dma_req_item);

	Submission_Queue_Entry *sqe = (Submission_Queue_Entry *)request->IO_command_info;
	host_interface->Send_read_message_to_host((sqe->PRP_entry_2 << 31) | sqe->PRP_entry_1, request->Queue_id, request->Size_in_byte);
}

void Request_Fetch_Unit_NVMe::Send_completion_queue_element(User_Request *request, uint16_t sq_head_value)
{
	Host_Interface_NVMe *hi = (Host_Interface_NVMe *)host_interface;
	Completion_Queue_Entry *cqe = new Completion_Queue_Entry;
	cqe->SQ_Head = sq_head_value;
	//cqe->SQ_ID = FLOW_ID_TO_Q_ID(request->Stream_id);
	cqe->FLOW_ID = FLOW_ID_TO_Q_ID(request->Stream_id); //LM modification for multiqueue flow
	cqe->SQ_ID = request->Queue_id;                     //LM modification for multiqueue flow
	cqe->SF_P = 0x0001 & current_phase;
	cqe->Command_Identifier = ((Submission_Queue_Entry *)request->IO_command_info)->Command_Identifier;
	Input_Stream_NVMe *im = ((Input_Stream_NVMe *)hi->input_stream_manager->input_streams[request->Stream_id]);
	host_interface->Send_write_message_to_host(im->queue_pairs[request->Queue_id].Completion_queue_base_address + im->queue_pairs[request->Queue_id].Completion_tail * sizeof(Completion_Queue_Entry),request->Queue_id,  cqe, sizeof(Completion_Queue_Entry));
	number_of_sent_cqe++;
	if (number_of_sent_cqe % im->queue_pairs[request->Queue_id].Completion_queue_size == 0)
	{
		//According to protocol specification, the value of the Phase Tag is inverted each pass through the Completion Queue
		if (current_phase == 0xffff)
		{
			current_phase = 0xfffe;
		}
		else
		{
			current_phase = 0xffff;
		}
	}
}

void Request_Fetch_Unit_NVMe::Send_read_data(User_Request *request)
{
	Submission_Queue_Entry *sqe = (Submission_Queue_Entry *)request->IO_command_info;
	sqe->SQ_ID = request->Queue_id;

	std::cout<<"Send_read_data: queue ID "<<sqe->SQ_ID<<std::endl;
	std::cout<<"Send_read_data: CMD ID "<<sqe->Command_Identifier<<std::endl;

	host_interface->Send_write_message_to_host(sqe->PRP_entry_1,request->Queue_id, request->Data, request->Size_in_byte);
}

Host_Interface_NVMe::Host_Interface_NVMe(const sim_object_id_type &id,
										 LHA_type max_logical_sector_address, uint16_t submission_queue_depth, uint16_t completion_queue_depth,
										 unsigned int no_of_input_streams, uint16_t queue_fetch_size, unsigned int sectors_per_page, Data_Cache_Manager_Base *cache) : Host_Interface_Base(id, HostInterface_Types::NVME, max_logical_sector_address, sectors_per_page, cache),
																																									   submission_queue_depth(submission_queue_depth), completion_queue_depth(completion_queue_depth), no_of_input_streams(no_of_input_streams)
{
	this->input_stream_manager = new Input_Stream_Manager_NVMe(this, queue_fetch_size);
	this->request_fetch_unit = new Request_Fetch_Unit_NVMe(this);
}

stream_id_type Host_Interface_NVMe::Create_new_stream(IO_Flow_Priority_Class::Priority priority_class, LHA_type start_logical_sector_address, LHA_type end_logical_sector_address,
													  uint64_t submission_queue_base_address, uint64_t completion_queue_base_address, uint32_t queue_numbers_of_the_flow)
{
	return ((Input_Stream_Manager_NVMe *)input_stream_manager)->Create_new_stream(priority_class, start_logical_sector_address, end_logical_sector_address, submission_queue_base_address, submission_queue_depth, completion_queue_base_address, completion_queue_depth, queue_numbers_of_the_flow);
}

void Host_Interface_NVMe::Validate_simulation_config()
{
	Host_Interface_Base::Validate_simulation_config();
	if (this->input_stream_manager == NULL)
	{
		throw std::logic_error("Input stream manager is not set for Host Interface");
	}
	if (this->request_fetch_unit == NULL)
	{
		throw std::logic_error("Request fetch unit is not set for Host Interface");
	}
}

void Host_Interface_NVMe::Start_simulation()
{
}

void Host_Interface_NVMe::Execute_simulator_event(MQSimEngine::Sim_Event *event) {}

uint16_t Host_Interface_NVMe::Get_submission_queue_depth()
{
	return submission_queue_depth;
}

uint16_t Host_Interface_NVMe::Get_completion_queue_depth()
{
	return completion_queue_depth;
}

void Host_Interface_NVMe::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter &xmlwriter)
{
	std::string tmp = name_prefix + ".HostInterface";
	xmlwriter.Write_open_tag(tmp);

	std::string attr = "Name";
	std::string val = ID();
	xmlwriter.Write_attribute_string(attr, val);

	for (unsigned int stream_id = 0; stream_id < no_of_input_streams; stream_id++)
	{
		std::string tmp = name_prefix + ".IO_Stream";
		xmlwriter.Write_open_tag(tmp);

		attr = "Stream_ID";
		val = std::to_string(stream_id);
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Read_Transaction_Turnaround_Time";
		val = std::to_string(input_stream_manager->Get_average_read_transaction_turnaround_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Read_Transaction_Execution_Time";
		val = std::to_string(input_stream_manager->Get_average_read_transaction_execution_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Read_Transaction_Transfer_Time";
		val = std::to_string(input_stream_manager->Get_average_read_transaction_transfer_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Read_Transaction_Waiting_Time";
		val = std::to_string(input_stream_manager->Get_average_read_transaction_waiting_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Write_Transaction_Turnaround_Time";
		val = std::to_string(input_stream_manager->Get_average_write_transaction_turnaround_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Write_Transaction_Execution_Time";
		val = std::to_string(input_stream_manager->Get_average_write_transaction_execution_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Write_Transaction_Transfer_Time";
		val = std::to_string(input_stream_manager->Get_average_write_transaction_transfer_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		attr = "Average_Write_Transaction_Waiting_Time";
		val = std::to_string(input_stream_manager->Get_average_write_transaction_waiting_time(stream_id));
		xmlwriter.Write_attribute_string(attr, val);

		xmlwriter.Write_close_tag();
	}

	xmlwriter.Write_close_tag();
}
} // namespace SSD_Components