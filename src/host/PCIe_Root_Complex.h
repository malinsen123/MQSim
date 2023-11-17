#ifndef PCIE_ROOT_COMPLEX_H
#define PCIE_ROOT_COMPLEX_H

#include "../ssd/Host_Interface_Defs.h"
#include "Host_Defs.h"
#include "PCIe_Message.h"
#include "PCIe_Link.h"
#include "Host_IO_Request.h"
#include "IO_Flow_Base.h"
#include "SATA_HBA.h"


namespace Host_Components
{
	class PCIe_Link;
	class IO_Flow_Base;
	class SATA_HBA;
	class PCIe_Root_Complex
	{
	public:
		PCIe_Root_Complex(PCIe_Link* pcie_link, HostInterface_Types SSD_device_type, SATA_HBA* sata_hba, std::vector<Host_Components::IO_Flow_Base*>* IO_flows);
		
		void Consume_pcie_message(PCIe_Message* messages)//Modern processors support DDIO, where all writes to memory are going through LLC
		{

			//std::cout<<"PCIe_Root_Complex::Consume_pcie_message"<<std::endl;

			switch (messages->Type)
			{
			case PCIe_Message_Type::READ_REQ:
				Read_from_memory(messages->Address, messages->Queue_id,  (unsigned int)(intptr_t)messages->Payload);
				break;
			case PCIe_Message_Type::WRITE_REQ:
				Write_to_memory(messages->Address, messages->Queue_id,  messages->Payload);
				break;
			default:
				break;
			}
			delete messages;
		}
		
		void Write_to_device(uint64_t address, uint16_t queue_id, uint16_t write_value);
		void Write_to_device_nvme(uint64_t address, uint16_t queue_id, uint16_t write_value, uint16_t command_id); //LM
		void Set_io_flows(std::vector<Host_Components::IO_Flow_Base*>* IO_flows);
	private:
		PCIe_Link* pcie_link;
		HostInterface_Types SSD_device_type;
		SATA_HBA * sata_hba;
		std::vector<Host_Components::IO_Flow_Base*>* IO_flows;
		
		void Write_to_memory(const uint64_t address, uint16_t queue_id, const void* payload);
		void Read_from_memory(const uint64_t address, const uint16_t queue_id,  const unsigned int size);
	};
}

#endif //!PCIE_ROOT_COMPLEX_H
