#ifdef ZZ_INCLUDE_CODE
ZZ_33878:
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
	RA += 15400;
	AT = EMU_ReadU16(A0);
	AT = EMU_CheckedAdd(AT,-1);
	AT <<= 1;
	AT += 4;
	T6 = EMU_CheckedAdd(A0,AT);
	AT = EMU_ReadU16(A0 + 6); //+ 0x6
	AT <<= 1;
	AT += 4;
	T7 = EMU_CheckedAdd(A0,AT);
	ZZ_CLOCKCYCLES(31,0x800338F4);
ZZ_33878_7C:
	A3 = (int32_t)T6 < (int32_t)T7;
	if (A3)
	{
		ZZ_CLOCKCYCLES(3,0x800339B8);
		goto ZZ_33878_140;
	}
	AT = EMU_ReadU16(T6);
	T6 = EMU_CheckedAdd(T6,-2);
	if (AT == S6)
	{
		A3 = AT & 0x8000;
		ZZ_CLOCKCYCLES(7,0x800338F4);
		goto ZZ_33878_7C;
	}
	A3 = AT & 0x8000;
	if (!A3)
	{
		S3 = AT & 0x7800;
		ZZ_CLOCKCYCLES(9,0x80033924);
		goto ZZ_33878_AC;
	}
	S3 = AT & 0x7800;
	S3 >>= 11;
	S4 = AT & 0x7FF;
	ZZ_CLOCKCYCLES(12,0x80033984);
	goto ZZ_33878_10C;
ZZ_33878_AC:
	A3 = AT & 0xC000;
	T9 = 16384;
	if (A3 != T9)
	{
		A3 = EMU_ReadU16(T6);
		ZZ_CLOCKCYCLES(4,0x80033974);
		goto ZZ_33878_FC;
	}
	A3 = EMU_ReadU16(T6);
	T9 = A3 & 0x8000;
	if (!T9)
	{
		ZZ_CLOCKCYCLES(8,0x80033950);
		goto ZZ_33878_D8;
	}
	S4 = A3 & 0x7FF;
	ZZ_CLOCKCYCLES(11,0x8003395C);
	goto ZZ_33878_E4;
ZZ_33878_D8:
	A3 = EMU_ReadU16(T6 - 2); //+ 0xFFFFFFFE
	S4 = A3 & 0xFFF;
	ZZ_CLOCKCYCLES(3,0x8003395C);
ZZ_33878_E4:
	S3 = AT & 0x1F;
	S3 += 16;
	A3 = AT & 0x3FE0;
	A3 >>= 5;
	S4 += A3;
	ZZ_CLOCKCYCLES(6,0x80033984);
	goto ZZ_33878_10C;
ZZ_33878_FC:
	S3 = AT & 0xFFF;
	AT = EMU_ReadU16(T6);
	T6 = EMU_CheckedAdd(T6,-2);
	S4 = AT & 0xFFF;
	ZZ_CLOCKCYCLES(4,0x80033984);
ZZ_33878_10C:
	S3 += 1;
	S3 += S4;
	S3 <<= 1;
	S4 <<= 1;
	AT = A1 + 4;
	S4 += AT;
	S3 += AT;
	AT = EMU_ReadU16(S4);
	A3 = EMU_ReadU16(S3);
	EMU_Write16(S3,AT);
	EMU_Write16(S4,A3);
	ZZ_CLOCKCYCLES(13,0x800338F4);
	goto ZZ_33878_7C;
ZZ_33878_140:
	V0 = R0;
	T5 = R0;
	T4 = R0;
	T7 = A0 + 8;
	AT = EMU_ReadU16(A0 + 4); //+ 0x4
	AT <<= 1;
	AT += 4;
	FP = EMU_CheckedAdd(A0,AT);
	S0 = FP;
	AT = EMU_ReadU16(A0 + 6); //+ 0x6
	AT <<= 1;
	AT += 4;
	SP = EMU_CheckedAdd(A0,AT);
	T6 = A1 + 4;
	GP = EMU_ReadU16(A1);
	S1 = A2 + 4;
	AT = EMU_ReadU16(S0);
	S0 += 2;
	T9 = (int32_t)S0 < (int32_t)SP;
	if (!T9)
	{
		T3 = R0;
		ZZ_CLOCKCYCLES(23,0x80033A30);
		goto ZZ_33878_1B8;
	}
	T3 = R0;
	T8 = AT;
	if (T8 == S6)
	{
		T1 = T8 & 0xFFF;
		ZZ_CLOCKCYCLES(26,0x80033A30);
		goto ZZ_33878_1B8;
	}
	T1 = T8 & 0xFFF;
	T3 = T8 & 0xF000;
	T3 >>= 12;
	T3 += 1;
	ZZ_CLOCKCYCLES(30,0x80033A34);
	goto ZZ_33878_1BC;
ZZ_33878_1B8:
	T1 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033A34);
ZZ_33878_1BC:
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033A44);
		goto ZZ_33878_1CC;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	ZZ_CLOCKCYCLES(4,0x80033A44);
ZZ_33878_1CC:
	AT = EMU_ReadU16(T7);
	T7 += 2;
	T9 = (int32_t)T7 < (int32_t)FP;
	if (!T9)
	{
		T2 = R0;
		ZZ_CLOCKCYCLES(5,0x80033A74);
		goto ZZ_33878_1FC;
	}
	T2 = R0;
	S7 = AT;
	if (S7 == S6)
	{
		T0 = S7 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x80033A74);
		goto ZZ_33878_1FC;
	}
	T0 = S7 & 0xFFF;
	T2 = S7 & 0xF000;
	T2 >>= 12;
	T2 += 1;
	ZZ_CLOCKCYCLES(12,0x80033A78);
	goto ZZ_33878_200;
ZZ_33878_1FC:
	T0 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033A78);
ZZ_33878_200:
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033A88);
		goto ZZ_33878_210;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	ZZ_CLOCKCYCLES(4,0x80033A88);
ZZ_33878_210:
	V1 = R0;
	ZZ_CLOCKCYCLES(1,0x80033A8C);
ZZ_33878_214:
	AT = (int32_t)V1 < (int32_t)GP;
	A3 = (int32_t)R0 < (int32_t)T3;
	AT &= A3;
	if (!AT)
	{
		AT = EMU_CheckedAdd(T1,T5);
		ZZ_CLOCKCYCLES(5,0x80033B24);
		goto ZZ_33878_2AC;
	}
	AT = EMU_CheckedAdd(T1,T5);
	AT = EMU_CheckedSubtract(AT,V1);
	if (AT)
	{
		ZZ_CLOCKCYCLES(8,0x80033B24);
		goto ZZ_33878_2AC;
	}
	T3 = EMU_CheckedAdd(T3,-1);
	T5 += 1;
	V1 += 1;
	T6 += 2;
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(14,0x80033AD8);
		goto ZZ_33878_260;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	AT = T8 >> 15;
	T1 = EMU_CheckedAdd(T1,AT);
	ZZ_CLOCKCYCLES(19,0x80033C28);
	goto ZZ_33878_3B0;
ZZ_33878_260:
	AT = EMU_ReadU16(S0);
	S0 += 2;
	T9 = (int32_t)S0 < (int32_t)SP;
	if (!T9)
	{
		T3 = R0;
		ZZ_CLOCKCYCLES(5,0x80033B08);
		goto ZZ_33878_290;
	}
	T3 = R0;
	T8 = AT;
	if (T8 == S6)
	{
		T1 = T8 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x80033B08);
		goto ZZ_33878_290;
	}
	T1 = T8 & 0xFFF;
	T3 = T8 & 0xF000;
	T3 >>= 12;
	T3 += 1;
	ZZ_CLOCKCYCLES(12,0x80033B0C);
	goto ZZ_33878_294;
ZZ_33878_290:
	T1 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033B0C);
ZZ_33878_294:
	if ((int32_t)T3 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033C28);
		goto ZZ_33878_3B0;
	}
	T8 = EMU_ReadU16(S0);
	S0 += 2;
	ZZ_CLOCKCYCLES(6,0x80033C28);
	goto ZZ_33878_3B0;
ZZ_33878_2AC:
	if ((int32_t)T2 <= 0)
	{
		AT = EMU_CheckedAdd(T0,T5);
		ZZ_CLOCKCYCLES(2,0x80033BBC);
		goto ZZ_33878_344;
	}
	AT = EMU_CheckedAdd(T0,T5);
	AT = EMU_CheckedSubtract(AT,T4);
	AT -= 1;
	AT = EMU_CheckedSubtract(AT,V1);
	if (AT)
	{
		AT = S7 & 0x7FFF;
		ZZ_CLOCKCYCLES(7,0x80033BBC);
		goto ZZ_33878_344;
	}
	AT = S7 & 0x7FFF;
	EMU_Write16(S1,AT);
	S1 += 2;
	V0 += 1;
	T2 = EMU_CheckedAdd(T2,-1);
	T4 += 1;
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(14,0x80033B70);
		goto ZZ_33878_2F8;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	AT = S7 >> 15;
	T0 = EMU_CheckedAdd(T0,AT);
	ZZ_CLOCKCYCLES(19,0x80033C28);
	goto ZZ_33878_3B0;
ZZ_33878_2F8:
	AT = EMU_ReadU16(T7);
	T7 += 2;
	T9 = (int32_t)T7 < (int32_t)FP;
	if (!T9)
	{
		T2 = R0;
		ZZ_CLOCKCYCLES(5,0x80033BA0);
		goto ZZ_33878_328;
	}
	T2 = R0;
	S7 = AT;
	if (S7 == S6)
	{
		T0 = S7 & 0xFFF;
		ZZ_CLOCKCYCLES(8,0x80033BA0);
		goto ZZ_33878_328;
	}
	T0 = S7 & 0xFFF;
	T2 = S7 & 0xF000;
	T2 >>= 12;
	T2 += 1;
	ZZ_CLOCKCYCLES(12,0x80033BA4);
	goto ZZ_33878_32C;
ZZ_33878_328:
	T0 = S6 << 8;
	ZZ_CLOCKCYCLES(1,0x80033BA4);
ZZ_33878_32C:
	if ((int32_t)T2 <= 0)
	{
		ZZ_CLOCKCYCLES(2,0x80033C28);
		goto ZZ_33878_3B0;
	}
	S7 = EMU_ReadU16(T7);
	T7 += 2;
	ZZ_CLOCKCYCLES(6,0x80033C28);
	goto ZZ_33878_3B0;
ZZ_33878_344:
	AT = (int32_t)V1 < (int32_t)GP;
	if (!AT)
	{
		AT = EMU_CheckedAdd(T1,T5);
		ZZ_CLOCKCYCLES(3,0x80033C28);
		goto ZZ_33878_3B0;
	}
	AT = EMU_CheckedAdd(T1,T5);
	AT = EMU_CheckedSubtract(AT,V1);
	A3 = EMU_CheckedAdd(T0,T5);
	A3 = EMU_CheckedSubtract(A3,T4);
	A3 = EMU_CheckedAdd(A3,-1);
	A3 = EMU_CheckedSubtract(A3,V1);
	T9 = (int32_t)AT < (int32_t)A3;
	if (T9)
	{
		ZZ_CLOCKCYCLES(11,0x80033BEC);
		goto ZZ_33878_374;
	}
	AT = A3;
	ZZ_CLOCKCYCLES(12,0x80033BEC);
ZZ_33878_374:
	A3 = EMU_CheckedSubtract(GP,V1);
	A3 = EMU_CheckedAdd(A3,-1);
	T9 = (int32_t)AT < (int32_t)A3;
	if (T9)
	{
		ZZ_CLOCKCYCLES(5,0x80033C04);
		goto ZZ_33878_38C;
	}
	AT = A3;
	ZZ_CLOCKCYCLES(6,0x80033C04);
ZZ_33878_38C:
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
	ZZ_CLOCKCYCLES(9,0x80033C28);
	}
ZZ_33878_3B0:
	AT = (int32_t)V1 < (int32_t)GP;
	A3 = (int32_t)R0 < (int32_t)T2;
	AT |= A3;
	A3 = (int32_t)R0 < (int32_t)T3;
	AT |= A3;
	if (AT)
	{
		ZZ_CLOCKCYCLES(7,0x80033A8C);
		goto ZZ_33878_214;
	}
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
