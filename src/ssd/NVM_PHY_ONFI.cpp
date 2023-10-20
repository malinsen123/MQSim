#include "NVM_PHY_ONFI.h"

namespace SSD_Components {
	void NVM_PHY_ONFI::ConnectToTransactionServicedSignal(TransactionServicedHandlerType function)
	{
		connectedTransactionServicedHandlers.push_back(function);
	}

	/*
	* Different FTL components maybe waiting for a transaction to be finished:
	* HostInterface: For user reads and writes
	* Address_Mapping_Unit: For mapping reads and writes
	* TSU: For the reads that must be finished for partial writes (first read non updated parts of page data and then merge and write them into the new page)
	* GarbageCollector: For gc reads, writes, and erases
	*/
	void NVM_PHY_ONFI::broadcastTransactionServicedSignal(NVM_Transaction_Flash* transaction)
	{
		for (std::vector<TransactionServicedHandlerType>::iterator it = connectedTransactionServicedHandlers.begin();
			it != connectedTransactionServicedHandlers.end(); it++) {
			(*it)(transaction);
		}
		delete transaction;//This transaction has been consumed and no more needed
	}

	void NVM_PHY_ONFI::ConnectToChannelIdleSignal(ChannelIdleHandlerType function)
	{
		connectedChannelIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChannelIdleSignal(flash_channel_ID_type channelID)
	{
		for (std::vector<ChannelIdleHandlerType>::iterator it = connectedChannelIdleHandlers.begin();
			it != connectedChannelIdleHandlers.end(); it++) {
			(*it)(channelID);
		}
	}

	void NVM_PHY_ONFI::ConnectToChipIdleSignal(ChipIdleHandlerType function)
	{
		connectedChipIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChipIdleSignal(NVM::FlashMemory::Flash_Chip* chip)
	{
		for (std::vector<ChipIdleHandlerType>::iterator it = connectedChipIdleHandlers.begin();
			it != connectedChipIdleHandlers.end(); it++) {
			(*it)(chip);
		}
	}


	std::string NVM_PHY_ONFI::ChipStatusToString(ChipStatus status)
	{
		switch(status){
			case ChipStatus::IDLE:
				return "IDLE";
			case ChipStatus::CMD_IN:
				return "CMD_IN";
			case ChipStatus::CMD_DATA_IN:
				return "CMD_DATA_IN";
			case ChipStatus::DATA_OUT:
				return "DATA_OUT";
			case ChipStatus::READING:
				return "READING";
			case ChipStatus::WRITING:
				return "WRITING";
			case ChipStatus::ERASING:
				return "ERASING";
			case ChipStatus::WAIT_FOR_DATA_OUT:
				return "WAIT_FOR_DATA_OUT";
			case ChipStatus::WAIT_FOR_COPYBACK_CMD:
				return "WAIT_FOR_COPYBACK_CMD";
			case ChipStatus::READING_AND_DATA_OUT:
				return "READING_AND_DATA_OUT";
			case ChipStatus::READING_AND_WAIT_DATA_OUT:
				return "READING_AND_WAIT_DATA_OUT";
			default:
				return "UNKNOWN";

		}
	}

}