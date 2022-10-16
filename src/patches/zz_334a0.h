#ifdef ZZ_INCLUDE_CODE
ZZ_334A0:
	AT = 0x1F800000;
	EMU_Write32(AT,S0);
	EMU_Write32(AT + 4,S1); //+ 0x4
	EMU_Write32(AT + 8,S2); //+ 0x8
	EMU_Write32(AT + 12,S3); //+ 0xC
	EMU_Write32(AT + 16,S4); //+ 0x10
	EMU_Write32(AT + 20,S5); //+ 0x14
	EMU_Write32(AT + 24,S6); //+ 0x18
	EMU_Write32(AT + 28,S7); //+ 0x1C
	EMU_Write32(AT + 32,T8); //+ 0x20
	EMU_Write32(AT + 36,T9); //+ 0x24
	EMU_Write32(AT + 40,K0); //+ 0x28
	EMU_Write32(AT + 44,K1); //+ 0x2C
	EMU_Write32(AT + 48,GP); //+ 0x30
	EMU_Write32(AT + 52,SP); //+ 0x34
	EMU_Write32(AT + 56,FP); //+ 0x38
	EMU_Write32(AT + 60,RA); //+ 0x3C
	S6 = 0xFFFF;
	RA = 0x80030000;
	RA += 14152;
	V0 = R0;
	T5 = R0;
	S0 = A0 + 8;
	AT = EMU_ReadU16(A0 + 4); //+ 0x4
	AT <<= 1;
	AT += 4;
	SP = EMU_CheckedAdd(A0,AT);
	T7 = SP;
	AT = EMU_ReadU16(A0 + 6); //+ 0x6
	AT <<= 1;
	AT += 4;
	FP = EMU_CheckedAdd(A0,AT);
	T6 = A1 + 4;
	GP = EMU_ReadU16(A1);
	S1 = A2 + 4;
	AT = EMU_ReadU16(S0);
	S0 += 2;
	T9 = (int32_t)S0 < (int32_t)SP;
	if (!T9)
	{
		T3 = R0;
		ZZ_CLOCKCYCLES(42,0x80033564);
		goto ZZ_334A0_C4;
	}
	T3 = R0;
	T8 = AT;
	if (T8 == S6)
	{
		T1 = T8 & 0xFFF;
		ZZ_CLOCKCYCLES(45,0x80033564);
		goto ZZ_334A0_C4;
	}
	T1 = T8 & 0xFFF;
	T3 = T8 & 0xF000;
	T3 >>= 12;
	T3 += 1;
	ZZ_CLOCKCYCLES(49,0x80033568);
	goto ZZ_334A0_C8;
ZZ_334A0_C4:
	T1 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033568);
ZZ_334A0_C8:
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033578);
		goto ZZ_334A0_D8;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	ZZ_CLOCKCYCLES(4,0x80033578);
ZZ_334A0_D8:
	AT = EMU_ReadU16(T7);
	T7 += 2;
	T9 = (int32_t)T7 < (int32_t)FP;
	if (!T9)
	{
		T2 = R0;
		ZZ_CLOCKCYCLES(5,0x800335A8);
		goto ZZ_334A0_108;
	}
	T2 = R0;
	S7 = AT;
	if (S7 == S6)
	{
		T0 = S7 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x800335A8);
		goto ZZ_334A0_108;
	}
	T0 = S7 & 0xFFF;
	T2 = S7 & 0xF000;
	T2 >>= 12;
	T2 += 1;
	ZZ_CLOCKCYCLES(12,0x800335AC);
	goto ZZ_334A0_10C;
ZZ_334A0_108:
	T0 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x800335AC);
ZZ_334A0_10C:
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x800335BC);
		goto ZZ_334A0_11C;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	ZZ_CLOCKCYCLES(4,0x800335BC);
ZZ_334A0_11C:
	V1 = R0;
	ZZ_CLOCKCYCLES(1,0x800335C0);
ZZ_334A0_120:
	AT = (int32_t)V1 < (int32_t)GP;
	A3 = (int32_t)R0 < (int32_t)T3;
	AT &= A3;
	if (!AT)
	{
		AT = EMU_CheckedAdd(T1,-1);
		ZZ_CLOCKCYCLES(5,0x80033658);
		goto ZZ_334A0_1B8;
	}
	AT = EMU_CheckedAdd(T1,-1);
	AT = EMU_CheckedSubtract(AT,V1);
	if (AT)
	{
		ZZ_CLOCKCYCLES(8,0x80033658);
		goto ZZ_334A0_1B8;
	}
	T3 = EMU_CheckedAdd(T3,-1);
	T5 += 1;
	V1 += 1;
	T6 += 2;
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(14,0x8003360C);
		goto ZZ_334A0_16C;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	AT = T8 >> 15;
	T1 = EMU_CheckedAdd(T1,AT);
	ZZ_CLOCKCYCLES(19,0x80033748);
	goto ZZ_334A0_2A8;
ZZ_334A0_16C:
	AT = EMU_ReadU16(S0);
	S0 += 2;
	T9 = (int32_t)S0 < (int32_t)SP;
	if (!T9)
	{
		T3 = R0;
		ZZ_CLOCKCYCLES(5,0x8003363C);
		goto ZZ_334A0_19C;
	}
	T3 = R0;
	T8 = AT;
	if (T8 == S6)
	{
		T1 = T8 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x8003363C);
		goto ZZ_334A0_19C;
	}
	T1 = T8 & 0xFFF;
	T3 = T8 & 0xF000;
	T3 >>= 12;
	T3 += 1;
	ZZ_CLOCKCYCLES(12,0x80033640);
	goto ZZ_334A0_1A0;
ZZ_334A0_19C:
	T1 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033640);
ZZ_334A0_1A0:
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033748);
		goto ZZ_334A0_2A8;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	ZZ_CLOCKCYCLES(6,0x80033748);
	goto ZZ_334A0_2A8;
ZZ_334A0_1B8:
	if ((int32_t)T2 <= 0)
	{
		AT = EMU_CheckedAdd(T0,T5);
		ZZ_CLOCKCYCLES(2,0x800336E4);
		goto ZZ_334A0_244;
	}
	AT = EMU_CheckedAdd(T0,T5);
	AT = EMU_CheckedSubtract(AT,V1);
	if (AT)
	{
		AT = S7 & 0x7FFF;
		ZZ_CLOCKCYCLES(5,0x800336E4);
		goto ZZ_334A0_244;
	}
	AT = S7 & 0x7FFF;
	EMU_Write16(S1,AT);
	S1 += 2;
	V0 += 1;
	T2 = EMU_CheckedAdd(T2,-1);
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(11,0x80033698);
		goto ZZ_334A0_1F8;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	AT = S7 >> 15;
	T0 = EMU_CheckedAdd(T0,AT);
	ZZ_CLOCKCYCLES(16,0x80033748);
	goto ZZ_334A0_2A8;
ZZ_334A0_1F8:
	AT = EMU_ReadU16(T7);
	T7 += 2;
	T9 = (int32_t)T7 < (int32_t)FP;
	if (!T9)
	{
		T2 = R0;
		ZZ_CLOCKCYCLES(5,0x800336C8);
		goto ZZ_334A0_228;
	}
	T2 = R0;
	S7 = AT;
	if (S7 == S6)
	{
		T0 = S7 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x800336C8);
		goto ZZ_334A0_228;
	}
	T0 = S7 & 0xFFF;
	T2 = S7 & 0xF000;
	T2 >>= 12;
	T2 += 1;
	ZZ_CLOCKCYCLES(12,0x800336CC);
	goto ZZ_334A0_22C;
ZZ_334A0_228:
	T0 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x800336CC);
ZZ_334A0_22C:
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033748);
		goto ZZ_334A0_2A8;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	ZZ_CLOCKCYCLES(6,0x80033748);
	goto ZZ_334A0_2A8;
ZZ_334A0_244:
	AT = (int32_t)V1 < (int32_t)GP;
	if (!AT)
	{
		AT = EMU_CheckedSubtract(T1,V1);
		ZZ_CLOCKCYCLES(3,0x80033748);
		goto ZZ_334A0_2A8;
	}
	AT = EMU_CheckedSubtract(T1,V1);
	AT = EMU_CheckedAdd(AT,-1);
	A3 = EMU_CheckedAdd(T0,T5);
	A3 = EMU_CheckedSubtract(A3,V1);
	T9 = (int32_t)AT < (int32_t)A3;
	if (T9)
	{
		ZZ_CLOCKCYCLES(9,0x8003370C);
		goto ZZ_334A0_26C;
	}
	AT = A3;
	ZZ_CLOCKCYCLES(10,0x8003370C);
ZZ_334A0_26C:
	A3 = EMU_CheckedSubtract(GP,V1);
	A3 = EMU_CheckedAdd(A3,-1);
	T9 = (int32_t)AT < (int32_t)A3;
	if (T9)
	{
		ZZ_CLOCKCYCLES(5,0x80033724);
		goto ZZ_334A0_284;
	}
	AT = A3;
	ZZ_CLOCKCYCLES(6,0x80033724);
ZZ_334A0_284:
	A3 = (int32_t)AT < 2;
	if (!A3)
	{
		ZZ_CLOCKCYCLES(3,0x80033C94);
		goto ZZ_33878_41C;
	}
	else {
	AT = EMU_ReadU16(T6);
	T6 += 2;
	EMU_Write16(S1,AT);
	S1 += 2;
	V0 += 1;
	V1 += 1;
	ZZ_CLOCKCYCLES(9,0x80033748);
	}
ZZ_334A0_2A8:
	AT = (int32_t)V1 < (int32_t)GP;
	A3 = (int32_t)R0 < (int32_t)T2;
	AT |= A3;
	A3 = (int32_t)R0 < (int32_t)T3;
	AT |= A3;
	if (AT)
	{
		ZZ_CLOCKCYCLES(7,0x800335C0);
		goto ZZ_334A0_120;
	}
	AT = EMU_ReadU16(A0 + 6); //+ 0x6
	AT <<= 1;
	AT += 4;
	T6 = EMU_CheckedAdd(A0,AT);
	AT = EMU_ReadU16(A0);
	AT <<= 1;
	AT += 4;
	T7 = EMU_CheckedAdd(A0,AT);
	ZZ_CLOCKCYCLES(17,0x8003378C);
ZZ_334A0_2EC:
	A3 = (int32_t)T6 < (int32_t)T7;
	if (!A3)
	{
		ZZ_CLOCKCYCLES(3,0x80033828);
		goto ZZ_334A0_388;
	}
	AT = EMU_ReadU16(T6);
	T6 += 2;
	if (AT == S6)
	{
		A3 = AT & 0x8000;
		ZZ_CLOCKCYCLES(7,0x80033828);
		goto ZZ_334A0_388;
	}
	A3 = AT & 0x8000;
	if (!A3)
	{
		S3 = AT & 0x7800;
		ZZ_CLOCKCYCLES(9,0x800337BC);
		goto ZZ_334A0_31C;
	}
	S3 = AT & 0x7800;
	S3 >>= 11;
	S4 = AT & 0x7FF;
	ZZ_CLOCKCYCLES(12,0x800337F0);
	goto ZZ_334A0_350;
ZZ_334A0_31C:
	A3 = AT & 0xC000;
	T9 = 16384;
	if (A3 != T9)
	{
		S3 = AT & 0x1F;
		ZZ_CLOCKCYCLES(4,0x800337E0);
		goto ZZ_334A0_340;
	}
	S3 = AT & 0x1F;
	S3 += 16;
	A3 = AT & 0x3FE0;
	A3 >>= 5;
	S4 += A3;
	ZZ_CLOCKCYCLES(9,0x800337F0);
	goto ZZ_334A0_350;
ZZ_334A0_340:
	S4 = AT & 0xFFF;
	AT = EMU_ReadU16(T6);
	T6 += 2;
	S3 = AT & 0xFFF;
	ZZ_CLOCKCYCLES(4,0x800337F0);
ZZ_334A0_350:
	T9 = S4;
	S3 += 1;
	S3 += S4;
	S3 <<= 1;
	S4 <<= 1;
	AT = A2 + 4;
	S4 += AT;
	S3 += AT;
	AT = EMU_ReadU16(S4);
	A3 = EMU_ReadU16(S3);
	EMU_Write16(S3,AT);
	EMU_Write16(S4,A3);
	S4 = T9;
	ZZ_CLOCKCYCLES(14,0x8003378C);
	goto ZZ_334A0_2EC;
ZZ_334A0_388:
	EMU_Write16(A2,V0);
	AT = 0x1F800000;
	S0 = EMU_ReadU32(AT);
	S1 = EMU_ReadU32(AT + 4); //+ 0x4
	S2 = EMU_ReadU32(AT + 8); //+ 0x8
	S3 = EMU_ReadU32(AT + 12); //+ 0xC
	S4 = EMU_ReadU32(AT + 16); //+ 0x10
	S5 = EMU_ReadU32(AT + 20); //+ 0x14
	S6 = EMU_ReadU32(AT + 24); //+ 0x18
	S7 = EMU_ReadU32(AT + 28); //+ 0x1C
	T8 = EMU_ReadU32(AT + 32); //+ 0x20
	T9 = EMU_ReadU32(AT + 36); //+ 0x24
	K0 = EMU_ReadU32(AT + 40); //+ 0x28
	K1 = EMU_ReadU32(AT + 44); //+ 0x2C
	GP = EMU_ReadU32(AT + 48); //+ 0x30
	SP = EMU_ReadU32(AT + 52); //+ 0x34
	RA = EMU_ReadU32(AT + 60); //+ 0x3C
	FP = EMU_ReadU32(AT + 56); //+ 0x38
#endif
