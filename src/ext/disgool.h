#ifndef _DISGOOL_H_
#define _DISGOOL_H_

#include "common.h"

#define _GopA_  ((ins >> 12) & 0xFFF)  // GOOL operand A
#define _GopB_  ((ins      ) & 0xFFF)  // GOOL operand B
#define _GopL_ _GopB_
#define _GopR_ _GopA_
#define _GopS_ _GopA_
#define _GopD_ _GopB_

#define _Im14_  ((ins      ) & 0x3FFF) // 14 bit immediate code offset or state id
#define _Im10_  ((ins      ) & 0x3FF)  // 10 bit immediate branch offset
#define _Os_    ((ins >> 22) & 3)      // 3 bit control flow operation subtype
#define _Cond_  ((ins >> 20) & 3)      // 3 bit operation condition mode
#define _Ri_    ((ins >> 14) & 0x3F)   // 6 bit 'register' index = object field index
#define _Vs_    ((ins >> 10) & 0xF)    // 4 bit variable skip count

#define PAD_Gop ((ins      ) & 0xFFF)
#define PAD_Pct ((ins >> 12) & 3)
#define PAD_Sct ((ins >> 14) & 3)
#define PAD_Dir ((ins >> 16) & 0xF)
#define PAD_Flg ((ins >> 20) & 1)

#define MSC_Gop ((ins      ) & 0xFFF)  // argument
#define MSC_Idx ((ins >> 12) & 0x7)    // data index
#define MSC_Pso ((ins >> 15) & 0x1F)   // primary sub-operation
#define MSC_Sso ((ins >> 20) & 0xF)    // secondary sub-operation

#define CVM_Gop ((ins      ) & 0xFFF)  
#define CVM_Lnk ((ins >> 12) & 0x7)    // link index
#define CVM_Cdi ((ins >> 15) & 0x3F)   // color data index

#define ANI_FrG ((ins      ) & 0xFFF)  // anim frame gool operand
#define ANI_FrI ((ins      ) & 0x7F)   // anim frame immediate operand
#define ANI_Seq ((ins >>  7) & 0x1FF)  // anim sequence index
#define ANI_Wt  ((ins >> 16) & 0x3F)   // anim wait time
#define ANI_Flp ((ins >> 22) & 3)      // anim flip mode

#define VEC_Gop ((ins      ) & 0xFFF)  // vector gool operand input 
#define VEC_ViA ((ins >> 12) & 7)      // vector index A
#define VEC_ViB ((ins >> 15) & 7)      // vector index B
#define VEC_Pso ((ins >> 18) & 7)      // primary [vector] sub-operation
#define VEC_ViC ((ins >> 21) & 7)      // vector index C

#define JAL_Vs  ((ins >> 20) & 0xF)    // JAL variable skip count 

#define EVT_Gop ((ins      ) & 0xFFF)  // event gool operand input (event)
#define EVT_Cnd ((ins >> 12) & 0x3F)   // event condition = register index
#define EVT_ArC ((ins >> 18) & 7)      // event num args sent to recipient
#define EVT_Rec ((ins >> 21) & 7)      // event recipient = link index

#define CHD_SpC ((ins      ) & 0x3F)   // children spawn count
#define CHD_Sub ((ins >>  6) & 0x3F)   // children objects' subtype
#define CHD_Typ ((ins >> 12) & 0xFF)   // children objects' type
#define CHD_ArC ((ins >> 20) & 0xF)    // children init state arg count

#define SND_Gop ((ins      ) & 0xFFF)
#define SND_Ri  ((ins >> 12) & 0x3F)   // sound flags = 6 bit 'register' index = obj field index
#define SND_Vol ((ins >> 20) & 0xF)    // sound volume?

extern char *GoolDisassemble(uint32_t ins, uint32_t pc);
extern char *GoolFormatRef(char *str, uint32_t ref, int mode);
extern char *GoolFormatLnk(char *str, uint32_t lnk);
extern char *GoolFormatVec(char *str, uint32_t vec);
extern char *GoolFormatCol(char *str, uint32_t col);

#endif /* _DISGOOL_H_ */
