#ifndef ONFI_CHANNEL_BASE_H
#define ONFI_CHANNEL_BASE_H

#include "../nvm_chip/flash_memory/Flash_Chip.h"
#include "NVM_Channel_Base.h"

namespace SSD_Components
{
	enum class ONFI_Protocol {NVDDR2};
	class ONFI_Channel_Base : public NVM_Channel_Base
	{
	public:
		ONFI_Channel_Base(flash_channel_ID_type channelID, unsigned int chipCount, NVM::FlashMemory::Flash_Chip** flashChips, ONFI_Protocol type);
		flash_channel_ID_type ChannelID;
		NVM::FlashMemory::Flash_Chip** Chips;
		ONFI_Protocol Type;

		BusChannelStatus GetStatus()
		{
			return status;
		}

		void SetStatus(BusChannelStatus new_status, NVM::FlashMemory::Flash_Chip* target_chip)
		{
			if (((status == BusChannelStatus::IDLE && new_status == BusChannelStatus::IDLE)
				|| (status == BusChannelStatus::BUSY && new_status == BusChannelStatus::BUSY) 
				|| (status == BusChannelStatus::BUSY_IN && new_status == BusChannelStatus::BUSY_IN)
				|| (status == BusChannelStatus::BUSY_OUT && new_status == BusChannelStatus::BUSY_OUT)
				|| (status == BusChannelStatus::BUSY_IN_AND_OUT && new_status == BusChannelStatus::BUSY_IN_AND_OUT))
				&& (current_active_chip != target_chip)) {
				//PRINT_ERROR("Bus " << ChannelID << ": illegal bus status transition!")
			}

			//std::cout<<"set status: "<<(int)status<<" -> "<<(int)new_status<<std::endl;

			status = new_status;
			if (status == BusChannelStatus::BUSY || status == BusChannelStatus::BUSY_IN || status == BusChannelStatus::BUSY_IN_AND_OUT || status == BusChannelStatus::BUSY_OUT) {
				current_active_chip = target_chip;
			} else {
				current_active_chip = NULL;
			}
		}


		std::string ChannelStatusToString(BusChannelStatus status)
		{
			switch (status) {
			case BusChannelStatus::BUSY:
				return "BUSY";
			case BusChannelStatus::BUSY_IN:
				return "BUSY_IN";
			case BusChannelStatus::BUSY_OUT:
				return "BUSY_OUT";
			case BusChannelStatus::BUSY_IN_AND_OUT:
				return "BUSY_IN_AND_OUT";
			case BusChannelStatus::IDLE:
				return "IDLE";
			default:
				return "UNKNOWN";
			}
		}


	private:
		BusChannelStatus status;
		NVM::FlashMemory::Flash_Chip* current_active_chip;
	};
}

#endif // !CHANNEL_H
