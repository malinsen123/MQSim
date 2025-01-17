#include "../sim/Sim_Defs.h"
#include "../sim/Engine.h"
#include "PCIe_Link.h"
#include "PCIe_Message.h"

namespace Host_Components
{
	PCIe_Link::PCIe_Link(const sim_object_id_type& id, PCIe_Root_Complex* root_complex, PCIe_Switch* pcie_switch,
		double lane_bandwidth_GBPs, int lane_count, int tlp_header_size,
		int tlp_max_payload_size, int dllp_ovehread, int ph_overhead) :
		Sim_Object(id), root_complex(root_complex), pcie_switch(pcie_switch),
		lane_bandwidth_GBPs(lane_bandwidth_GBPs), lane_count(lane_count),
		tlp_header_size(tlp_header_size), tlp_max_payload_size(tlp_max_payload_size), dllp_ovehread(dllp_ovehread), ph_overhead(ph_overhead)
	{
		packet_overhead = ph_overhead + dllp_ovehread + tlp_header_size;
	}

	void PCIe_Link::Set_root_complex(PCIe_Root_Complex* root_complex)
	{
		this->root_complex = root_complex;
	}

	void PCIe_Link::Set_pcie_switch(PCIe_Switch* pcie_switch) 
	{
		this->pcie_switch = pcie_switch;
	}

	void PCIe_Link::Deliver(PCIe_Message* message)
	{
		switch (message->Destination) {
			case PCIe_Destination_Type::HOST://Message from SSD device to the host
				Message_buffer_toward_root_complex.push(message);
				if (Message_buffer_toward_root_complex.size() > 1) {//There are active transfers
					return;
				}
				Simulator->Register_sim_event(Simulator->Time() + estimate_transfer_time(message), this, (void*)(intptr_t)PCIe_Destination_Type::HOST, static_cast<int>(PCIe_Link_Event_Type::DELIVER));
				//std::cout<<"register event to deliver message to host"<<std::endl;
				//std::cout<<"estimate_transfer_time(message): "<<estimate_transfer_time(message)<<std::endl;
				//std::cout<<"current time: "<<Simulator->Time()<<std::endl;
				//std::cout<<"estimate_finish_time(message): "<<Simulator->Time() + estimate_transfer_time(message)<<std::endl;
				break;
			case PCIe_Destination_Type::DEVICE://Message from Host to the SSD device
				Message_buffer_toward_ssd_device.push(message);
				if (Message_buffer_toward_ssd_device.size() > 1) {
					//std::cout<<"[PCIe_Link][Deliver] Message_buffer_toward_ssd_device.size() > 1"<<std::endl;
					//std::cout<<"The size of Message_buffer_toward_ssd_device is "<<Message_buffer_toward_ssd_device.size()<<std::endl;
					return;
				}
				//LM This is the actual function delivering the message to device
				Simulator->Register_sim_event(Simulator->Time() + estimate_transfer_time(message), this, (void*)(intptr_t)PCIe_Destination_Type::DEVICE, static_cast<int>(PCIe_Link_Event_Type::DELIVER));
				//std::cout<<"register event to deliver message to device"<<std::endl;
				//std::cout<<"current time: "<<Simulator->Time()<<std::endl;
				//std::cout<<"estimate_transfer_time(message): "<<estimate_transfer_time(message)<<std::endl;
				break;
			default:
				break;
		}
	}

	void PCIe_Link::Start_simulation() {}

	void PCIe_Link::Validate_simulation_config() {}

	void PCIe_Link::Execute_simulator_event(MQSimEngine::Sim_Event* event)
	{

		//std::cout<<"[PCIe_Link][Execute_simulator_event] event->Type: "<<event->Type<<std::endl;
		//std::cout<<"current time: "<<Simulator->Time()<<std::endl;


		PCIe_Message* message = NULL;
		PCIe_Destination_Type destination = (PCIe_Destination_Type)(intptr_t)event->Parameters;
		switch (destination) {
			case PCIe_Destination_Type::HOST:
				//std::cout<<"[PCIe_Link][Execute_simulator_event] PCIe_Destination_Type::HOST"<<std::endl;
				message = Message_buffer_toward_root_complex.front();
				Message_buffer_toward_root_complex.pop();
				root_complex->Consume_pcie_message(message);
				if (Message_buffer_toward_root_complex.size() > 0) {//There are active transfers
					//std::cout<<"[PCIe_Link][Execute_simulator_event] Message_buffer_toward_root_complex.size() > 0"<<std::endl;

					Simulator->Register_sim_event(Simulator->Time() + estimate_transfer_time(Message_buffer_toward_root_complex.front()),
						this, (void*)(intptr_t)PCIe_Destination_Type::HOST, static_cast<int>(PCIe_Link_Event_Type::DELIVER));
					//std::cout<<"1.0 expected finish time: "<<Simulator->Time() + estimate_transfer_time(Message_buffer_toward_root_complex.front())<<std::endl;
				}
				break;
			case PCIe_Destination_Type::DEVICE:
				//std::cout<<"[PCIe_Link][Execute_simulator_event] PCIe_Destination_Type::DEVICE"<<std::endl;
				message = Message_buffer_toward_ssd_device.front();
				Message_buffer_toward_ssd_device.pop();
				pcie_switch->Deliver_to_device(message);
				if (Message_buffer_toward_ssd_device.size() > 0) {
					Simulator->Register_sim_event(Simulator->Time() + estimate_transfer_time(Message_buffer_toward_ssd_device.front()),
						this, (void*)(intptr_t)PCIe_Destination_Type::DEVICE, static_cast<int>(PCIe_Link_Event_Type::DELIVER));
					//std::cout<<"2.0 expected finish time: "<<Simulator->Time() + estimate_transfer_time(Message_buffer_toward_ssd_device.front())<<std::endl;
				
				}
				break;
		}
	}
}
