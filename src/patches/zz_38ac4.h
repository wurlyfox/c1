#ifdef ZZ_INCLUDE_CODE
ZZ_38AC4:
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
	V0 = 0x1F800000;
	V0 |= 0xEC;
	EMU_Write32(V0,A0);
	V1 = EMU_ReadU32(SP + 16); //+ 0x10
	T0 = EMU_ReadU32(SP + 20); //+ 0x14
	V0 = EMU_ReadU32(SP + 24); //+ 0x18
	T1 = EMU_ReadU32(SP + 28); //+ 0x1C
	T2 = A1;
	SP = EMU_ReadU32(A2);
	FP = EMU_ReadU32(A2 + 4); //+ 0x4
	RA = EMU_ReadU32(A2 + 8); //+ 0x8
	T6 = EMU_ReadU32(A3);
	T7 = EMU_ReadU32(A3 + 4); //+ 0x4
	S0 = EMU_ReadU32(A3 + 8); //+ 0x8
	S1 = EMU_ReadU32(A3 + 12); //+ 0xC
	S2 = EMU_ReadU32(A3 + 16); //+ 0x10
	S3 = EMU_ReadU32(A3 + 20); //+ 0x14
	AT = 0x1F800000;
	AT |= 0xE0;
	EMU_Write32(AT,V0);
	EMU_Write32(AT + 4,T1); //+ 0x4
	EMU_Write32(AT + 8,T0); //+ 0x8
	A3 = R0;
	A2 = R0;
	AT = R0;
	T1 = R0;
	ZZ_CLOCKCYCLES(43,0x80038B70);
ZZ_38AC4_AC:
	A0 = EMU_ReadU32(T2);
	V0 = EMU_ReadU32(T2 + 4); //+ 0x4
	T0 = 0xFFFF;
	A1 = A0 & T0;
	if (A1 == T0)
	{
		ZZ_CLOCKCYCLES(6,0x80038D6C);
		goto ZZ_38AC4_2A8;
	}
	if (A1)
	{
		ZZ_CLOCKCYCLES(8,0x80038BC4);
		goto ZZ_38AC4_100;
	}
	S4 = A0 >> 16;
	S4 <<= 8;
	S5 = V0 & 0xFFFF;
	S5 <<= 8;
	S6 = V0 >> 16;
	S6 <<= 8;
	A0 = EMU_ReadU32(T2 + 8); //+ 0x8
	V0 = EMU_ReadU32(T2 + 12); //+ 0xC
	S7 = A0 >> 16;
	T8 = V0 & 0xFFFF;
	T9 = V0 >> 16;
	T2 += 16;
	ZZ_CLOCKCYCLES(21,0x80038B70);
	goto ZZ_38AC4_AC;
ZZ_38AC4_100:
	A1 = A0 & 0xFFF8;
	A1 >>= 2;
	A1 |= 0x1;
	T0 = A1 & 0xE;
	GP = A0 & 0x7;
	T3 = A0 >> 16;
	T4 = V0 & 0xFFFF;
	T5 = V0 >> 16;
	T3 <<= 16;
	T3 = (int32_t)T3 >> 12;
	T4 <<= 16;
	T4 = (int32_t)T4 >> 12;
	T5 <<= 16;
	T5 = (int32_t)T5 >> 12;
	T3 += SP;
	T4 += FP;
	T5 += RA;
	A0 = (int32_t)S1 < (int32_t)T3;
	if (A0)
	{
		A0 = (int32_t)S2 < (int32_t)T4;
		ZZ_CLOCKCYCLES(20,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = (int32_t)S2 < (int32_t)T4;
	if (A0)
	{
		A0 = (int32_t)S3 < (int32_t)T5;
		ZZ_CLOCKCYCLES(22,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = (int32_t)S3 < (int32_t)T5;
	if (A0)
	{
		A0 = (int32_t)S7 < (int32_t)GP;
		ZZ_CLOCKCYCLES(24,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = (int32_t)S7 < (int32_t)GP;
	if (A0)
	{
		A0 = S4 >> S7;
		ZZ_CLOCKCYCLES(26,0x80038C30);
		goto ZZ_38AC4_16C;
	}
	A0 = S4 >> S7;
	A0 = S4 >> GP;
	ZZ_CLOCKCYCLES(27,0x80038C30);
ZZ_38AC4_16C:
	T3 += A0;
	A0 = (int32_t)T8 < (int32_t)GP;
	if (A0)
	{
		A0 = S5 >> T8;
		ZZ_CLOCKCYCLES(4,0x80038C44);
		goto ZZ_38AC4_180;
	}
	A0 = S5 >> T8;
	A0 = S5 >> GP;
	ZZ_CLOCKCYCLES(5,0x80038C44);
ZZ_38AC4_180:
	T4 += A0;
	A0 = (int32_t)T9 < (int32_t)GP;
	if (A0)
	{
		A0 = S6 >> T9;
		ZZ_CLOCKCYCLES(4,0x80038C58);
		goto ZZ_38AC4_194;
	}
	A0 = S6 >> T9;
	A0 = S6 >> GP;
	ZZ_CLOCKCYCLES(5,0x80038C58);
ZZ_38AC4_194:
	T5 += A0;
	A0 = (int32_t)T3 < (int32_t)T6;
	if (A0)
	{
		A0 = (int32_t)T4 < (int32_t)T7;
		ZZ_CLOCKCYCLES(4,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = (int32_t)T4 < (int32_t)T7;
	if (A0)
	{
		A0 = (int32_t)T5 < (int32_t)S0;
		ZZ_CLOCKCYCLES(6,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = (int32_t)T5 < (int32_t)S0;
	if (A0)
	{
		A0 = 0x6;
		ZZ_CLOCKCYCLES(8,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	A0 = 0x6;
	if (T0 == A0)
	{
		A0 = 0x8;
		ZZ_CLOCKCYCLES(10,0x80038C98);
		goto ZZ_38AC4_1D4;
	}
	A0 = 0x8;
	if (T0 == A0)
	{
		A0 = A1 & 0x3F0;
		ZZ_CLOCKCYCLES(12,0x80038C98);
		goto ZZ_38AC4_1D4;
	}
	A0 = A1 & 0x3F0;
	if (!A0)
	{
		A0 = A0 < 624;
		ZZ_CLOCKCYCLES(14,0x80038D3C);
		goto ZZ_38AC4_278;
	}
	A0 = A0 < 624;
	if (!A0)
	{
		ZZ_CLOCKCYCLES(16,0x80038D3C);
		goto ZZ_38AC4_278;
	}
	ZZ_CLOCKCYCLES(16,0x80038C98);
ZZ_38AC4_1D4:
	A0 = 0x1F800000;
	A0 |= 0xF0;
	EMU_Write32(A0,AT);
	AT = 0x1F800000;
	AT |= 0x70;
	EMU_Write32(AT,V1);
	EMU_Write32(AT + 4,A1); //+ 0x4
	EMU_Write32(AT + 8,A2); //+ 0x8
	EMU_Write32(AT + 12,A3); //+ 0xC
	EMU_Write32(AT + 16,T0); //+ 0x10
	EMU_Write32(AT + 20,T1); //+ 0x14
	EMU_Write32(AT + 24,T2); //+ 0x18
	EMU_Write32(AT + 28,T3); //+ 0x1C
	EMU_Write32(AT + 32,T4); //+ 0x20
	EMU_Write32(AT + 36,T5); //+ 0x24
	EMU_Write32(AT + 40,T6); //+ 0x28
	EMU_Write32(AT + 44,T7); //+ 0x2C
	A0 = 0x1F800000;
	A0 |= 0xE4;
	V0 = EMU_ReadU32(A0);
	A0 = EMU_ReadU32(A0 + 8); //+ 0x8
	ZZ_JUMPREGISTER_BEGIN(V0);
	RA = 0x80038CF4; //ZZ_38AC4_230
	ZZ_CLOCKCYCLES_JR(23);
	ZZ_JUMPREGISTER(0x8002C3B8,ZZ_2C3B8);
	ZZ_JUMPREGISTER_END();
ZZ_38AC4_230:
	AT = 0x1F800000;
	AT |= 0x70;
	V1 = EMU_ReadU32(AT);
	A1 = EMU_ReadU32(AT + 4); //+ 0x4
	A2 = EMU_ReadU32(AT + 8); //+ 0x8
	A3 = EMU_ReadU32(AT + 12); //+ 0xC
	T0 = EMU_ReadU32(AT + 16); //+ 0x10
	T1 = EMU_ReadU32(AT + 20); //+ 0x14
	T2 = EMU_ReadU32(AT + 24); //+ 0x18
	T3 = EMU_ReadU32(AT + 28); //+ 0x1C
	T4 = EMU_ReadU32(AT + 32); //+ 0x20
	T5 = EMU_ReadU32(AT + 36); //+ 0x24
	T6 = EMU_ReadU32(AT + 40); //+ 0x28
	T7 = EMU_ReadU32(AT + 44); //+ 0x2C
	A0 = 0x1F800000;
	A0 |= 0xF0;
	AT = EMU_ReadU32(A0);
	if (V0)
	{
		A0 = (int32_t)V1 < (int32_t)T4;
		ZZ_CLOCKCYCLES(19,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	ZZ_CLOCKCYCLES(18,0x80038D3C);
ZZ_38AC4_278:
	A0 = (int32_t)V1 < (int32_t)T4;
	if (A0)
	{
		ZZ_CLOCKCYCLES(3,0x80038D64);
		goto ZZ_38AC4_2A0;
	}
	if (!T0)
	{
		ZZ_CLOCKCYCLES(5,0x80038D5C);
		goto ZZ_38AC4_298;
	}
	T1 += 1;
	AT += T4;
	ZZ_CLOCKCYCLES(8,0x80038D64);
	goto ZZ_38AC4_2A0;
ZZ_38AC4_298:
	A2 += 1;
	A3 += T4;
	ZZ_CLOCKCYCLES(2,0x80038D64);
ZZ_38AC4_2A0:
	T2 += 8;
	ZZ_CLOCKCYCLES(2,0x80038B70);
	goto ZZ_38AC4_AC;
ZZ_38AC4_2A8:
	T2 = 0x1F800000;
	T2 |= 0xE0;
	T2 = EMU_ReadU32(T2);
	A0 = 0x1F800000;
	A0 |= 0xE8;
	A0 = EMU_ReadU32(A0);
	if (!A2)
	{
		EMU_Write32(A0 + 4,T2); //+ 0x4
		ZZ_CLOCKCYCLES(8,0x80038D98);
		goto ZZ_38AC4_2D4;
	}
	EMU_Write32(A0 + 4,T2); //+ 0x4
	EMU_SDivide(A3,A2);
	V0 = LO;
	EMU_Write32(A0 + 4,V0); //+ 0x4
	ZZ_CLOCKCYCLES(11,0x80038D98);
ZZ_38AC4_2D4:
	if (!T1)
	{
		EMU_Write32(A0 + 8,T2); //+ 0x8
		ZZ_CLOCKCYCLES(2,0x80038DAC);
		goto ZZ_38AC4_2E8;
	}
	EMU_Write32(A0 + 8,T2); //+ 0x8
	EMU_SDivide(AT,T1);
	V0 = LO;
	EMU_Write32(A0 + 8,V0); //+ 0x8
	ZZ_CLOCKCYCLES(5,0x80038DAC);
ZZ_38AC4_2E8:
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
	// ZZ_JUMPREGISTER_BEGIN(RA);
	// ZZ_CLOCKCYCLES_JR(19);
	// ZZ_JUMPREGISTER(0x8002CAD8,ZZ_2C8EC_1EC);
	// ZZ_JUMPREGISTER_END();
#endif
