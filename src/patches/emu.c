#include "emu.h"

uint32_t EMU_reg[0x100];
uint32_t EMU_ram[0x88000];
uint32_t EMU_scratch[0x400];

uint32_t (*EMU_codemap[EMU_RAMWORDS])(uint32_t address);

uint32_t EMU_ExecuteUnrecognizedCode(uint32_t pc)
{
	printf("Unrecognized code at PC := %X\n",pc);
	abort();
}

uint32_t EMU_ExecuteBadEntryPointCode(uint32_t pc)
{
	printf("Bad entry point for native code at PC := %X\n",pc);
	abort();
}

uint32_t EMU_DummyMethod(uint32_t pc)
{
	return RA;
}

void EMU_Init(void)
{
	// extern void ZZ_Init(void);
	// PCSX_Init();
	// EMU_codemap[0x28] = BIOS_Execute;
	// EMU_codemap[0x2C] = BIOS_Execute;
	// EMU_codemap[0x30] = BIOS_Execute;
	// EMU_codemap[0x221] = EMU_DummyMethod;
	// for (int i = 0;i < EMU_RAMWORDS;i++)
	// {
	//	if (!EMU_codemap[i])
	//		EMU_codemap[i] = EMU_ExecuteUnrecognizedCode;
	//}
	// ZZ_Init();
}

#define CASE_SPU_IO_PORT(offset) \
	case 0x1F801C00 | (offset): \
	case 0x1F801C10 | (offset): \
	case 0x1F801C20 | (offset): \
	case 0x1F801C30 | (offset): \
	case 0x1F801C40 | (offset): \
	case 0x1F801C50 | (offset): \
	case 0x1F801C60 | (offset): \
	case 0x1F801C70 | (offset): \
	case 0x1F801C80 | (offset): \
	case 0x1F801C90 | (offset): \
	case 0x1F801CA0 | (offset): \
	case 0x1F801CB0 | (offset): \
	case 0x1F801CC0 | (offset): \
	case 0x1F801CD0 | (offset): \
	case 0x1F801CE0 | (offset): \
	case 0x1F801CF0 | (offset): \
	case 0x1F801D00 | (offset): \
	case 0x1F801D10 | (offset): \
	case 0x1F801D20 | (offset): \
	case 0x1F801D30 | (offset): \
	case 0x1F801D40 | (offset): \
	case 0x1F801D50 | (offset): \
	case 0x1F801D60 | (offset): \
	case 0x1F801D70 | (offset)

int8_t EMU_ReadS8(uint32_t address)
{
	return EMU_ReadU8(address);
}

int16_t EMU_ReadS16(uint32_t address)
{
	return EMU_ReadU16(address);
}

uint8_t EMU_ReadU8(uint32_t address)
{
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Reading code as data\n");
			}
		}
		return EMU_ram[(address & 0x1FFFFF) >> 2] >> (address & 3) * 8;
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		return EMU_scratch[(address & 0x3FF) >> 2] >> (address & 3) * 8;
	}
	else switch (address)
	{
    /*
	case 0x1F801040:
		return PAD_Read();
	case 0x1F801800:
		return CDR_GetStatus();
	case 0x1F801801:
		return CDR_ReadResponse();
	case 0x1F801802:
		return CDR_Read();
	case 0x1F801803:
		return CDR_Read3();
    */
	default:
		printf("Unsupported 8-bit read address %.8X\n",address);
		return -1; /*PCSX_Read8(address);*/
	}
}

uint16_t EMU_ReadU16(uint32_t address)
{
	if (address & 1)
		printf("Unaligned halfword load\n");
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Reading code as data\n");
			}
		}
		return EMU_ram[(address & 0x1FFFFF) >> 2] >> (address & 2) * 8;
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		return EMU_scratch[(address & 0x3FF) >> 2] >> (address & 2) * 8;
	}
	else switch (address)
	{
    /*
	case 0x1F801070:
		return IRQ_GetStatus();
	case 0x1F801074:
		return IRQ_GetMask();
	CASE_SPU_IO_PORT(0x0):
		return SPU_Voice_GetVolumeLeft((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0x2):
		return SPU_Voice_GetVolumeRight((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0x4):
		return SPU_Voice_GetSampleRate((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0x6):
		return SPU_Voice_GetStartAddress((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0x8):
		return SPU_Voice_GetADSRLow((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0xA):
		return SPU_Voice_GetADSRHigh((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0xC):
		return SPU_Voice_GetADSRVolume((address & 0x1F0) >> 4);
	CASE_SPU_IO_PORT(0xE):
		return SPU_Voice_GetLoopAddress((address & 0x1F0) >> 4);
	case 0x1F801D98:
		return SPU_GetReverbLow();
	case 0x1F801D9A:
		return SPU_GetReverbHigh();
	case 0x1F801DA6:
		return SPU_GetTransferAddress();
	case 0x1F801DAA:
		return SPU_ReadControlRegister();
	case 0x1F801DAE:
		return SPU_ReadStatusRegister();
	case 0x1F801DB8:
		return SPU_GetCurrentVolumeLeft();
	case 0x1F801DBA:
		return SPU_GetCurrentVolumeRight();
	case 0x1F801E00:
	case 0x1F801E04:
	case 0x1F801E08:
	case 0x1F801E0C:
	case 0x1F801E10:
	case 0x1F801E14:
	case 0x1F801E18:
	case 0x1F801E1C:
	case 0x1F801E20:
	case 0x1F801E24:
	case 0x1F801E28:
	case 0x1F801E2C:
	case 0x1F801E30:
	case 0x1F801E34:
	case 0x1F801E38:
	case 0x1F801E3C:
	case 0x1F801E40:
	case 0x1F801E44:
	case 0x1F801E48:
	case 0x1F801E4C:
	case 0x1F801E50:
	case 0x1F801E54:
	case 0x1F801E58:
	case 0x1F801E5C:
		return SPU_Voice_GetCurrentVolumeLeft((address & 0x7C) >> 2);
	case 0x1F801E02:
	case 0x1F801E06:
	case 0x1F801E0A:
	case 0x1F801E0E:
	case 0x1F801E12:
	case 0x1F801E16:
	case 0x1F801E1A:
	case 0x1F801E1E:
	case 0x1F801E22:
	case 0x1F801E26:
	case 0x1F801E2A:
	case 0x1F801E2E:
	case 0x1F801E32:
	case 0x1F801E36:
	case 0x1F801E3A:
	case 0x1F801E3E:
	case 0x1F801E42:
	case 0x1F801E46:
	case 0x1F801E4A:
	case 0x1F801E4E:
	case 0x1F801E52:
	case 0x1F801E56:
	case 0x1F801E5A:
	case 0x1F801E5E:
		return SPU_Voice_GetCurrentVolumeRight((address & 0x7C) >> 2);
    */
	default:
		printf("Unsupported 16-bit read address %.8X\n",address);
		return -1;
		// return PCSX_Read16(address);
	}
}

uint32_t EMU_ReadU32(uint32_t address)
{
	if (address & 3)
		printf("Unaligned word load\n");
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Reading code as data\n");
			}
		}
		return EMU_ram[(address & 0x1FFFFF) >> 2];
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		return EMU_scratch[(address & 0x3FF) >> 2];
	}
	else switch (address)
	{
    /*
	case 0x1F801014:
		// Unused waitstate configuration
		return PCSX_Read16(address);
	case 0x1F801074:
		return IRQ_GetMask();
	case 0x1F8010A8:
		return DMA_GetMode(DMA_GPU);
	case 0x1F8010B8:
		return DMA_GetMode(DMA_CDR);
	case 0x1F8010F0:
		return DMA_GetControlRegister();
	case 0x1F8010F4:
		return DMA_GetInterruptRegister();
	case 0x1F801110:
		return RCNT_GetValue(RCNT_HBLANK);
	case 0x1F801810:
		return GPU_Read();
	case 0x1F801814:
		return GPU_GetStatus();
	*/
	default:
		printf("Unsupported 32-bit read address %.8X\n",address);
		// return PCSX_Read32(address);
        return -1;
	}
}

void EMU_Write8(uint32_t address,uint8_t value)
{
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Writing over code\n");
			}
		}
		uint32_t *word = &EMU_ram[(address & 0x1FFFFF) >> 2];
		*word &= ~(0xFF << (address & 3) * 8);
		*word |= value << (address & 3) * 8;
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		uint32_t *word = &EMU_scratch[(address & 0x3FF) >> 2];
		*word &= ~(0xFF << (address & 3) * 8);
		*word |= value << (address & 3) * 8;
	}
	else switch (address)
	{
    /*
	case 0x1F801040:
		PAD_Write(value);
		break;
	case 0x1F801800:
		CDR_SetMode(value);
		break;
	case 0x1F801801:
		CDR_Write1(value);
		break;
	case 0x1F801802:
		CDR_Write2(value);
		break;
	case 0x1F801803:
		CDR_Write3(value);
		break;
	*/
	default:
		printf("Unsupported 8-bit write address %.8X\n",address);
		// PCSX_Write8(address,value);
		break;
	}
}

void EMU_Write16(uint32_t address,uint16_t value)
{
	if (address & 1)
		printf("Unaligned halfword store\n");
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Writing over code\n");
			}
		}
		uint32_t *word = &EMU_ram[(address & 0x1FFFFF) >> 2];
		*word &= ~(0xFFFF << (address & 2) * 8);
		*word |= value << (address & 2) * 8;
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		uint32_t *word = &EMU_scratch[(address & 0x3FF) >> 2];
		*word &= ~(0xFFFF << (address & 2) * 8);
		*word |= value << (address & 2) * 8;
	}
	else switch (address)
	{
    /*
	case 0x1F80104A:
		PAD_SetControl(value);
		break;
	case 0x1F801070:
		IRQ_AcknowledgeBits(value);
		break;
	case 0x1F801074:
		IRQ_SetMask(value);
		break;
	case 0x1F801124:
		RCNT_SetMode(RCNT_SYSDIV8,value);
		break;
	case 0x1F801128:
		RCNT_SetTarget(RCNT_SYSDIV8,value);
		break;
	CASE_SPU_IO_PORT(0x0):
		SPU_Voice_SetVolumeLeft((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0x2):
		SPU_Voice_SetVolumeRight((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0x4):
		SPU_Voice_SetSampleRate((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0x6):
		SPU_Voice_SetStartAddress((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0x8):
		SPU_Voice_SetADSRLow((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0xA):
		SPU_Voice_SetADSRHigh((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0xC):
		SPU_Voice_SetADSRVolume((address & 0x1F0) >> 4,value);
		break;
	CASE_SPU_IO_PORT(0xE):
		SPU_Voice_SetLoopAddress((address & 0x1F0) >> 4,value);
		break;
	case 0x1F801D80:
		SPU_SetVolumeLeft(value);
		break;
	case 0x1F801D82:
		SPU_SetVolumeRight(value);
		break;
	case 0x1F801D84:
		SPU_SetReverbVolumeLeft(value);
		break;
	case 0x1F801D86:
		SPU_SetReverbVolumeRight(value);
		break;
	case 0x1F801D88:
		SPU_NoteOnLow(value);
		break;
	case 0x1F801D8A:
		SPU_NoteOnHigh(value);
		break;
	case 0x1F801D8C:
		SPU_NoteOffLow(value);
		break;
	case 0x1F801D8E:
		SPU_NoteOffHigh(value);
		break;
	case 0x1F801D90:
		SPU_SetFMLow(value);
		break;
	case 0x1F801D92:
		SPU_SetFMHigh(value);
		break;
	case 0x1F801D94:
		SPU_SetNoiseLow(value);
		break;
	case 0x1F801D96:
		SPU_SetNoiseHigh(value);
		break;
	case 0x1F801D98:
		SPU_SetReverbLow(value);
		break;
	case 0x1F801D9A:
		SPU_SetReverbHigh(value);
		break;
	case 0x1F801D9C:
	case 0x1F801D9E:
		// nonsense write to read-only registers
		PCSX_Write16(address,value);
		break;
	case 0x1F801DA2:
		SPU_SetReverbStartAddress(value);
		break;
	case 0x1F801DA6:
		SPU_SetTransferAddress(value);
		break;
	case 0x1F801DA8:
		SPU_Write(value);
		break;
	case 0x1F801DAA:
		SPU_WriteControlRegister(value);
		break;
	case 0x1F801DAC:
		SPU_SetTransferMode(value);
		break;
	case 0x1F801DB0:
		SPU_SetCDVolumeLeft(value);
		break;
	case 0x1F801DB2:
		SPU_SetCDVolumeRight(value);
		break;
	case 0x1F801DB4:
		SPU_SetExternVolumeLeft(value);
		break;
	case 0x1F801DB6:
		SPU_SetExternVolumeRight(value);
		break;
	case 0x1F801DC0:
	case 0x1F801DC2:
	case 0x1F801DC4:
	case 0x1F801DC6:
	case 0x1F801DC8:
	case 0x1F801DCA:
	case 0x1F801DCC:
	case 0x1F801DCE:
	case 0x1F801DD0:
	case 0x1F801DD2:
	case 0x1F801DD4:
	case 0x1F801DD6:
	case 0x1F801DD8:
	case 0x1F801DDA:
	case 0x1F801DDC:
	case 0x1F801DDE:
	case 0x1F801DE0:
	case 0x1F801DE2:
	case 0x1F801DE4:
	case 0x1F801DE6:
	case 0x1F801DE8:
	case 0x1F801DEA:
	case 0x1F801DEC:
	case 0x1F801DEE:
	case 0x1F801DF0:
	case 0x1F801DF2:
	case 0x1F801DF4:
	case 0x1F801DF6:
	case 0x1F801DF8:
	case 0x1F801DFA:
	case 0x1F801DFC:
	case 0x1F801DFE:
		SPU_ConfigureReverb((address - 0x1F801DC0) / 2,value);
		break;
	*/
	default:
		printf("Unsupported 16-bit write address %.8X\n",address);
		// PCSX_Write16(address,value);
		break;
	}
}

void EMU_Write32(uint32_t address,uint32_t value)
{
	if (address & 3)
		printf("Unaligned word store\n");
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
normal:
		{
			uint32_t (*codehandler)(uint32_t) = EMU_codemap[(address & 0x1FFFFF) >> 2];
			if (codehandler != EMU_ExecuteUnrecognizedCode)
			{
				//printf("Writing over code\n");
			}
		}
		EMU_ram[(address & 0x1FFFFF) >> 2] = value;
	}
	else if (address >= 0x0 && address <= 0x1FFFFF)
	{
		goto normal;
	}
	else if (address >= 0xA0000000 && address <= 0xA01FFFFF)
	{
		goto normal;
	}
	else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		EMU_scratch[(address & 0x3FF) >> 2] = value;
	}
	else switch (address)
	{
    /*
	case 0x1F801014:
	case 0x1F801018:
	case 0x1F801020:
		// Unused waitstate configuration
		PCSX_Write32(address,value);
		break;
	case 0x1F801074:
		IRQ_SetMask(value);
		break;
	case 0x1F8010A0:
		DMA_SetAddress(DMA_GPU,value);
		break;
	case 0x1F8010A4:
		DMA_SetBlockData(DMA_GPU,value);
		break;
	case 0x1F8010A8:
		DMA_SetMode(DMA_GPU,value);
		break;
	case 0x1F8010B0:
		DMA_SetAddress(DMA_CDR,value);
		break;
	case 0x1F8010B4:
		DMA_SetBlockData(DMA_CDR,value);
		break;
	case 0x1F8010B8:
		DMA_SetMode(DMA_CDR,value);
		break;
	case 0x1F8010C0:
		DMA_SetAddress(DMA_SPU,value);
		break;
	case 0x1F8010C4:
		DMA_SetBlockData(DMA_SPU,value);
		break;
	case 0x1F8010C8:
		DMA_SetMode(DMA_SPU,value);
		break;
	case 0x1F8010F0:
		DMA_SetControlRegister(value);
		break;
	case 0x1F8010F4:
		DMA_SetInterruptRegister(value);
		break;
	case 0x1F801114:
		RCNT_SetMode(RCNT_HBLANK,value);
		break;
	case 0x1F801810:
		GPU_Write(value);
		break;
	case 0x1F801814:
		GPU_WriteAlt(value);
		break;
    */
	default:
		printf("Unsupported 32-bit write address %.8X",address);
		// PCSX_Write32(address,value);
		break;
	}
}

#undef CASE_SPU_IO_PORT

void EMU_ReadLeft(uint32_t address,uint32_t *valueref)
{
	static uint32_t mask[4] = {0xFFFFFF,0xFFFF,0xFF,0};
	static int shift[4] = {24,16,8,0};
	uint32_t value = EMU_ReadU32(address & ~3);
	*valueref &= mask[address & 3];
	*valueref |= value << shift[address & 3];
}

void EMU_ReadRight(uint32_t address,uint32_t *valueref)
{
	static uint32_t mask[4] = {0,0xFF000000,0xFFFF0000,0xFFFFFF00};
	static int shift[4] = {0,8,16,24};
	uint32_t value = EMU_ReadU32(address & ~3);
	*valueref &= mask[address & 3];
	*valueref |= value >> shift[address & 3];
}

void EMU_WriteLeft(uint32_t address,uint32_t value)
{
	static uint32_t mask[4] = {0xFFFFFF00,0xFFFF0000,0xFF000000,0};
	static int shift[4] = {24,16,8,0};
	uint32_t memvalue = EMU_ReadU32(address & ~3);
	memvalue &= mask[address & 3];
	memvalue |= value >> shift[address & 3];
	EMU_Write32(address & ~3,memvalue);
}

void EMU_WriteRight(uint32_t address,uint32_t value)
{
	static uint32_t mask[4] = {0,0xFF,0xFFFF,0xFFFFFF};
	static int shift[4] = {0,8,16,24};
	uint32_t memvalue = EMU_ReadU32(address & ~3);
	memvalue &= mask[address & 3];
	memvalue |= value << shift[address & 3];
	EMU_Write32(address & ~3,memvalue);
}

void *EMU_Pointer(uint32_t address)
{
	if (address >= 0x80000000 && address <= 0x801FFFFF)
	{
		return (uint8_t *)&EMU_ram[(address & 0x1FFFFF) >> 2] + (address & 3);
	}
  else if (address >= 0x1F800000 && address <= 0x1F8003FF)
	{
		return (uint8_t *)&EMU_scratch[(address & 0x3FF) >> 2] + (address & 3);
	}
	abort();
}

uint32_t EMU_Address(uint32_t x)
{
  if ((x >= (uint32_t)(EMU_ram)) &&
      (x <= (uint32_t)(EMU_ram+EMU_RAMWORDS)))
  {
    return (x - (uint32_t)(EMU_ram)) | 0x80000000;
  }
  else if ((x >= (uint32_t)(EMU_scratch)) &&
           (x <= (uint32_t)(EMU_scratch+EMU_SCRATCHWORDS)))
  {
    return (x - (uint32_t)(EMU_scratch)) | 0x1F800000;
  }
  abort();
}

void EMU_SMultiply(int32_t a,int32_t b)
{
	int64_t result = (int64_t)a * (int64_t)b;
	LO = result;
	HI = result >> 32;
}

void EMU_UMultiply(uint32_t a,uint32_t b)
{
	uint64_t result = (uint64_t)a * (uint64_t)b;
	LO = result;
	HI = result >> 32;
}

void EMU_SDivide(int32_t a,int32_t b)
{
	if (b)
	{
		LO = a / b;
		HI = a % b;
	}
	else
	{
		LO = 0xFFFFFFFF;
		HI = a;
	}
}

void EMU_UDivide(uint32_t a,uint32_t b)
{
	if (b)
	{
		LO = a / b;
		HI = a % b;
	}
	else
	{
		LO = 0xFFFFFFFF;
		HI = a;
	}
}

/*
uint32_t EMU_Invoke(uint32_t address,int argc,...)
{
	va_list args;
	uint32_t ra_saved = RA;
	uint32_t pc;
	SP -= argc * 4;
	va_start(args,argc);
	for (int i = 0;i < argc;i++)
	{
		if (i == 0)
			A0 = va_arg(args,uint32_t);
		else if (i == 1)
			A1 = va_arg(args,uint32_t);
		else if (i == 2)
			A2 = va_arg(args,uint32_t);
		else if (i == 3)
			A3 = va_arg(args,uint32_t);
		else
			EMU_Write32(SP + i * 4,va_arg(args,uint32_t));
	}
	va_end(args);
	RA = 0xDEADBEEF;
	pc = address;
	while (pc != 0xDEADBEEF)
	{
		if (pc & 3)
			printf("Program counter is unaligned\n");
		pc = EMU_codemap[(pc & 0x1FFFFF) >> 2](pc);
	}
	SP += argc * 4;
	RA = ra_saved;
	return V0;
}
*/

void EMU_Syscall(uint32_t pc)
{
	// PCSX_Syscall(pc);
    printf("Syscalls unsupported");
}

void EMU_Break(uint32_t id)
{
	// TODO
}

void EMU_NativeCall(void *method)
{
	static uint32_t stackbuffer[16];
	memcpy(stackbuffer,(uint8_t *)EMU_ram + (SP & 0x1FFFFF),sizeof(stackbuffer));
	stackbuffer[0] = A0;
	stackbuffer[1] = A1;
	stackbuffer[2] = A2;
	stackbuffer[3] = A3;
	__asm__
	(
		"movl %2,%%ecx;"
		"subl %%ecx,%%esp;"
		"shr $2,%%ecx;"
		"movl %%esp,%%edi;"
		"rep movsd;"
		"call *%1;"
		"addl %2,%%esp;"
		: "=a"(V0)
		: "m"(method),"i"(sizeof(stackbuffer)),"S"(stackbuffer)
		: "%ecx","%edx","%edi","flags","memory"
	);
}

void EMU_Cycle(int cycles)
{
	// PCSX_Cycle(cycles);
}

void EMU_RunInterrupts(void)
{
	/*uint32_t pc = PCSX_RunInterrupts(0xDEADBEEF);
	while (pc != 0xDEADBEEF)
	{
		if (pc & 3)
			printf("Program counter is unaligned");
		pc = EMU_codemap[(pc & 0x1FFFFF) >> 2](pc);
	}
	*/
	// printf("Run Interrupts not supported");
}

static uint32_t EMU_savedreg[34];

extern void EMU_SaveRegisters(void)
{
	memcpy(EMU_savedreg,EMU_reg,sizeof(EMU_savedreg));
}

extern void EMU_LoadRegisters(void)
{
	memcpy(EMU_reg,EMU_savedreg,sizeof(EMU_savedreg));
}
