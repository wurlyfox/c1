/**
 * GOOL disassembler.
 * (influenced by pcsx's r3000a disassembler.)
 */
#include "disgool.h"

char ostr[256];

#define scatf(s,...) sprintf(s+strlen(s),__VA_ARGS__)

// Type definition of functions
typedef char* (*TdisGOOLF)(uint32_t ins, uint32_t pc);

// Macros used to assemble the disassembler functions
#define MakeDisFg(func, b) char* func(uint32_t ins, uint32_t pc) { b; return ostr; }
#define MakeDisF(func, b) \
  static char* func(uint32_t ins, uint32_t pc) { \
    sprintf(ostr, "0x%-4.4X %8.8X:", pc, ins); \
    b; ostr[(strlen(ostr) - 1)] = 0; return ostr; \
  }

#define gName(i) scatf(ostr, " %5s  ", i)
#define gRefI(i) GoolFormatRef(ostr,i,0)
#define gRefO(i) GoolFormatRef(ostr,i,1)
#define gRefX(i) GoolFormatRef(ostr,i,2)
#define gIm(i)   scatf(ostr, " 0x%x,", i)
#define gReg(i)  scatf(ostr, " self[0x%x],", 0x60+(i*4));
#define gLnk(i)  GoolFormatLnk(ostr,i)
#define gVec(i)  GoolFormatVec(ostr,i)
#define gCol(i)  GoolFormatCol(ostr,i)
#define gCofs(i) scatf(ostr, " code[0x%4.4X],", i)

/*********************************************************
* add, mult, and non-commutative arithmetic              *
* Format:  OP gopr, gopl                                 *
*********************************************************/
MakeDisF(gADD,  gName("ADD");  gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gSUB,  gName("SUB");  gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gMULT, gName("MULT"); gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gDIV,  gName("DIV");  gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gMOD,  gName("MOD");  gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gSHA,  gName("SHA");  gRefI(_GopR_); gRefI(_GopL_);)

/*********************************************************
* commutative arithmetic                                 *
* Format:  OP gopa, gopb                                 *
*********************************************************/
MakeDisF(gCEQ,  gName("CEQ");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gANDL, gName("ANDL"); gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gANDB, gName("ANDB"); gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gORL,  gName("ORL");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gORB,  gName("ORB");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gSLT,  gName("SLT");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gSLE,  gName("SLE");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gSGT,  gName("SGT");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gSGE,  gName("SGE");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gXOR,  gName("XOR");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gTST,  gName("TST");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gNOTB, gName("NOTB"); gRefI(_GopA_); gRefI(_GopB_);)

/*********************************************************
* unary arithmetic                                       *
* Format:  OP gops, gopd                                 *
*********************************************************/
MakeDisF(gNOTL, gName("NOTL"); gRefI(_GopS_); gRefO(_GopD_);)  // src val, dst ref
MakeDisF(gABS,  gName("ABS");  gRefI(_GopS_); gRefO(_GopD_);)

/*********************************************************
* misc arithmetic functions                              *
* Format:  OP gopa, gopb                                 *
*********************************************************/
MakeDisF(gRND,  gName("RND");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gTRI,  gName("TRI");  gRefI(_GopA_); gRefX(_GopB_);)
MakeDisF(gSPD,  gName("SPD");  gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gPRS,  gName("PRS");  gRefI(_GopA_); gRefI(_GopB_);)  // period = GopA/2, phase = GopB
MakeDisF(gSSAW, gName("SSAW"); gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gANGD, gName("ANGD"); gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gAPCH, gName("APCH"); gRefI(_GopR_); gRefI(_GopL_);)
MakeDisF(gROT,  gName("ROT");  gRefI(_GopR_); gRefI(_GopL_);)

/*********************************************************
* memory operations                                      *
* Format:  OP gops, gopd                                 *
*          OP gopa, gopb                                 *
*          OP im14, ri                                   *
*********************************************************/
MakeDisF(gMOVE, gName("MOVE"); gRefI(_GopS_); gRefO(_GopD_);)
MakeDisF(gLEA,  gName("LEA");  gRefI(_GopS_); gRefO(_GopD_);)
MakeDisF(gPSHV, gName("PSHV"); gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gPSHP, gName("PSHP"); gRefI(_GopA_); gRefI(_GopB_);)
MakeDisF(gMOVC, gName("MOVC"); gCofs(_Im14_); gReg(_Ri_);)

/*********************************************************
* control flow                                           *
* Format: 0x82 type, cond, ri, **                        *
* BRA =      .    0, cond, ri, vs, im10                  *
* CST =      .    1, cond, ri, im14                      *
* RET =      .    2, cond, ri                            *
* JAL =   0x86             vs, im14                      *
* RST =   0x88 type, cond, ri, **                        *
* RSF =   0x89    .     .   .                            *
**********************************************************/
MakeDisF(gBRA,  gName("BRA");  gIm(_Im10_); gIm(_Vs_);)
MakeDisF(gBNEZ, gName("BNEZ"); gReg(_Ri_);  gIm(_Im10_); gIm(_Vs_);)
MakeDisF(gBEQZ, gName("BEQZ"); gReg(_Ri_);  gIm(_Im10_); gIm(_Vs_);)
MakeDisF(gBRAC, gName("BRAC"); gIm(_Im10_); gIm(_Vs_);)
MakeDisF(gCST,  gName("CST");  gIm(_Im14_);)
MakeDisF(gCSNZ, gName("CSNZ"); gReg(_Ri_); gIm(_Im14_);)
MakeDisF(gCSEZ, gName("CSEZ"); gReg(_Ri_); gIm(_Im14_);)
MakeDisF(gCSTC, gName("CSTC"); gIm(_Im14_);)
MakeDisF(gRET,  gName("RET");)
MakeDisF(gRTNZ, gName("RTNZ"); gReg(_Ri_);)
MakeDisF(gRTEZ, gName("RTEZ"); gReg(_Ri_);)
MakeDisF(gRETC, gName("RETC");)
MakeDisF(gSCND, gName("SCND");)
MakeDisF(gSCNZ, gName("SCNZ"); gReg(_Ri_);)
MakeDisF(gSCEZ, gName("SCEZ"); gReg(_Ri_);)
MakeDisF(gCCND, gName("CCND");)

MakeDisF(gJAL,  gName("JAL"); gIm(_Im14_); gIm(JAL_Vs);)

MakeDisF(gGBNT, gName("GBNT"); gReg(_Ri_); gIm(_Im10_); gIm(_Vs_);) // change _guard (or otherwise _branch when cond == 0) if cond (_not eq.) != 0 with guard = _true
MakeDisF(gGBZT, gName("GBZT"); gReg(_Ri_); gIm(_Im10_); gIm(_Vs_);) // change _guard (or otherwise _branch when cond != 0) if cond == 0 (_zero) with guard = _true
MakeDisF(gGBNF, gName("GBNF"); gReg(_Ri_); gIm(_Im10_); gIm(_Vs_);) // change _guard (or otherwise _branch when cond == 0) if cond (_not eq.) != 0 with guard = _true
MakeDisF(gGBZF, gName("GBZF"); gReg(_Ri_); gIm(_Im10_); gIm(_Vs_);) // change _guard (or otherwise _branch when cond != 0) if cond == 0 (_zero) with guard = _true
MakeDisF(gRST,  gName("RST");  gIm(_Im14_);)                        // _return _state transition with guard = _true
MakeDisF(gRSNT, gName("RSNT"); gReg(_Ri_); gIm(_Im14_);)            // _return _state transition if cond (_not eq.) != 0 with guard = _true
MakeDisF(gRSZT, gName("RSZT"); gReg(_Ri_); gIm(_Im14_);)            // _return _state transition if cond == 0 (_zero) with guard = _true
MakeDisF(gRSCT, gName("RSCT"); gIm(_Im14_);)                        // _return _state transition based on prev _cond with guard = _true
MakeDisF(gRSF,  gName("RSF");  gIm(_Im14_);)                        // _return _state transition with guard = _true
MakeDisF(gRSNF, gName("RSNF"); gReg(_Ri_); gIm(_Im14_);)            // _return _state transition if cond (_not eq.) != 0 with guard = _true
MakeDisF(gRSZF, gName("RSZF"); gReg(_Ri_); gIm(_Im14_);)            // _return _state transition if cond == 0 (_zero) with guard = _true
MakeDisF(gRSCF, gName("RSCF"); gIm(_Im14_);)                        // _return _state transition based on prev _cond with guard = _true
MakeDisF(gRNT,  gName("RNT");)                                      // _return _null state transition with guard = _true
MakeDisF(gRNNT, gName("RNNT"); gReg(_Ri_);)                         // _return _null state transition if cond (_not eq.) != 0 with guard = _true
MakeDisF(gRNZT, gName("RNZT"); gReg(_Ri_);)                         // _return _null state transition if cond == 0 (_zero) with guard = _true
MakeDisF(gRNCT, gName("RNCT");)                                     // _return _null state transition based on prev _cond with guard = _true
MakeDisF(gRNF,  gName("RNF");)                                      // _return _null state transition with guard = _true
MakeDisF(gRNNF, gName("RNNF"); gReg(_Ri_);)                         // _return _null state transition if cond (_not eq.) != 0 with guard = _true
MakeDisF(gRNZF, gName("RNZF"); gReg(_Ri_);)                         // _return _null state transition if cond == 0 (_zero) with guard = _true
MakeDisF(gRNCF, gName("RNCF");)                                     // _return _null state transition based on prev _cond with guard = _true
MakeDisF(gEBT,  gName("EBT");)                                      // _event [service] '_begin' and set guard to _true
MakeDisF(gEBNT, gName("EBNT"); gReg(_Ri_);)                         // _event [service] '_begin' if cond (_not eq.) != 0 and set guard to _true
MakeDisF(gEBZT, gName("EBZT"); gReg(_Ri_);)                         // _event [service] '_begin' if cond == 0 (_zero) and set guard to _true
MakeDisF(gEBCT, gName("EBCT");)                                     // _event [service] '_begin' based on prev _cond and set guard to _true
MakeDisF(gEBF,  gName("EBF");)                                      // _event [service] '_begin' and set guard to _true
MakeDisF(gEBNF, gName("EBNF"); gReg(_Ri_);)                         // _event [service] '_begin' if cond (_not eq.) != 0 and set guard to _true
MakeDisF(gEBZF, gName("EBZF"); gReg(_Ri_);)                         // _event [service] '_begin' if cond == 0 (_zero) and set guard to _true
MakeDisF(gEBCF, gName("EBCF");)                                     // _event [service] '_begin' based on prev _cond and set guard to _true

/*********************************************************
* vector operations                                      *
* Format: 0x85 VEC_ViC,VEC_Pso,VEC_ViB,VEC_ViA,VEC_Gop   *
*         0x8E ...                                       *
*                                                        *
* OP = 0x85                                              *
* PATH: VEC_Pso = 0 | VEC_Gop = path prog                *
*                     VEC_ViA = trans vec in/out         *
* Display Format: PATH [path prog], [trans vec in/out]   *
*                                                        *
* PROJ: VEC_Pso = 1 | VEC_Gop = out screen coord 2d vec  *
*                     VEC_ViA = in 3d vec                *
*                     VEC_ViB = out 3d orthographic vec  *
* Display Format: PROJ [out 2d], [in 3d], [out 3d orth]  *
*                                                        *
* APCR: VEC_Pso = 2 | VEC_Gop = rot speed                *
*                     VEC_ViA = dst rot angle            *
* Display Format: APCR [rot speed], [dst rot angle]      *
*                                                        *
* MATT: VEC_Pso = 4 | VEC_Gop = in vector z value        *
* MTRT: VEC_Pso = 5 | stack = ..., in vec x, in vec y    *
*                     VEC_ViA = trans vec in (for trans) *
*                     VEC_ViB = transformed vec out      *
* Display Format: MATT [in vec z], [tr vec in],[vec out] *
*                 MTRT ...                               *
*                                                        *
* TVTX: VEC_Pso = 6 | VEC_Gop = vertex index             *
*                     VEC_ViA = transformed pt (vec) out *
*                     VEC_ViD = link index (has model)   *
* Display Format TVTX [lnk indx], [vrt indx], [vec out]  *
*                                                        *
* OP = 0x8E                                              *
* RBND: VEC_Pso = 0 | VEC_Gop = dir vec out              *
*                     VEC_ViA = dir vec in               *
*                     VEC_ViB = trans vec out            *
* Display Format RBND [vec out], [vec in], [trans out]   *
*********************************************************/
MakeDisF(gPATH, gName("PATH"); gRefI(VEC_Gop); gVec(VEC_ViA);)
MakeDisF(gPROJ, gName("PROJ"); gRefI(VEC_Gop); gVec(VEC_ViA); gVec(VEC_ViB);)
MakeDisF(gAPCR, gName("APCR"); gRefI(VEC_Gop); gVec(VEC_ViA);)                // approach obj target rot
MakeDisF(gMATT, gName("MATT"); gRefI(VEC_Gop); gVec(VEC_ViA); gVec(VEC_ViB);) // linear/matrix transformation
MakeDisF(gMTRT, gName("MTRT"); gRefI(VEC_Gop); gVec(VEC_ViA); gVec(VEC_ViB);) // linear/matrix transformation using target rot as basis
MakeDisF(gTVTX, gName("TVTX"); gLnk(VEC_ViC); gRefI(VEC_Gop); gVec(VEC_ViA);) // linear/matrix transformation on svtx vertex
MakeDisF(gSNMT, gName("SNMT"); gRefI(VEC_Gop); gVec(VEC_ViA); gVec(VEC_ViB);) // matrix transform for sounds

MakeDisF(gRBND, gName("RBND"); gRefI(VEC_Gop); gVec(VEC_ViA); gVec(VEC_ViB);) // rebound vector from solid surface
MakeDisF(gVCU1, gName("VCU1"); gRefI(VEC_Gop); gVec(VEC_ViB);)
MakeDisF(gVCU2, gName("VCU2"); gRefI(VEC_Gop); gVec(VEC_ViB);)
MakeDisF(gVCU3, gName("VCU3"); gRefI(VEC_Gop); gVec(VEC_ViB);)
MakeDisF(gVCU4, gName("VCU4"); gRefI(VEC_Gop); gVec(VEC_ViB);)
MakeDisF(gVCU5, gName("VCU5");)

/*********************************************************
* misc operations                                        *
* Format:                                                *
*********************************************************/
MakeDisF(gEARG, gName("EARG"); gRefI(MSC_Gop); gIm(MSC_Sso);)                // _event _a_r_gument
MakeDisF(gDIST, gName("DIST"); gLnk(MSC_Idx);  gIm(MSC_Sso);)                // _d_i_s_tance between obj trans and link trans(flags in Sso)
MakeDisF(gVDST, gName("VDST"); gRefI(MSC_Gop); gLnk(MSC_Idx); gIm(MSC_Sso);) // _d_i_s_tance between vect and link trans(flags in Sso)
MakeDisF(gAGDP, gName("AGDP"); gRefI(MSC_Gop); gLnk(MSC_Idx);)               // _an_gular _dis_placement between ang and obj link ang
MakeDisF(gLR,   gName("LR");   gRefI(MSC_Gop); gLnk(MSC_Idx);)               // _load _register [from link]
MakeDisF(gSR,   gName("SR");   gRefI(MSC_Gop); gLnk(MSC_Idx);)               // _set _register [of link]
MakeDisF(gLXAG, gName("LXAG"); gLnk(MSC_Idx);)                               // [retrieve] _loo_k _an_gle between obj. and link in XZ plane
MakeDisF(gFIND, gName("FIND"); gRefI(MSC_Gop);)                              // _f_i_n_d object with id
MakeDisF(gSZON, gName("SZON"); gLnk(MSC_Idx);  gRefI(MSC_Gop);)              // _set current _z_o_ne of link to zone containing vector
MakeDisF(gSNRS, gName("SNRS"); gRefI(MSC_Gop); gIm(MSC_Idx);)                // _set (or clear) _no _re_spawn bit of obj w/id
MakeDisF(gTNRS, gName("TNRS"); gRefI(MSC_Gop);)                              // _test _no _re_spawn bit of obj w/id
MakeDisF(gSXB,  gName("SXB");  gRefI(MSC_Gop);)                              // _set _exists/spawned _bit
MakeDisF(gCXB,  gName("CXB");  gRefI(MSC_Gop);)                              // _clear _exists/spawned _bit
//MakeDisF(gTXB,  gName("TXB");  gRefI(MSC_Gop);)                              // _test _exists/spawned _bit
MakeDisF(gSB3,  gName("SB3");  gRefI(MSC_Gop);)                              // _set spawn status _bit _3
MakeDisF(gCB3,  gName("CB3");  gRefI(MSC_Gop);)                              // _clear spawn status _bit _3
MakeDisF(gTB3,  gName("TB3");  gRefI(MSC_Gop);)                              // _test spawn status _bit _3
MakeDisF(gPSSB, gName("PSSB"); gRefI(MSC_Gop);)                              // _preserve _spawn _status _bits
MakeDisF(gRSSB, gName("RSSB"); gRefI(MSC_Gop);)                              // _release _spawn _status _bits
MakeDisF(gSPTB, gName("SPTB"); gRefI(MSC_Gop);)                              // _set _preserve spawn status at _transition between levels _bit
MakeDisF(gCPTB, gName("CPTB"); gRefI(MSC_Gop);)                              // _clear _preserve spawn status at _transition between levels _bit
MakeDisF(gTPTB, gName("TPTB"); gRefI(MSC_Gop);)                              // _test _preserve spawn status at _transition between levels _bit
MakeDisF(gSAVE, gName("SAVE");)                                              // _s_a_v_e level state
MakeDisF(gLOAD, gName("LOAD");)                                              // [re]_l_o_a_d level state
MakeDisF(gSPAR, gName("SPAR"); gRefI(MSC_Gop);)                              // _set new _p_a_rent
MakeDisF(gUNKB, gName("UNKB"); gRefI(MSC_Gop);)
MakeDisF(gZONE, gName("ZONE"); gLnk(MSC_Idx);  gRefI(MSC_Gop);)              // _set object _li_n_k
MakeDisF(gSNDU, gName("SNDU");)
MakeDisF(gSNDC, gName("SNDC"); gRefI(MSC_Gop);)
MakeDisF(gTRMO, gName("TRMO");)                                              // _te_r_minate all _objects in current zone and its neighbors
MakeDisF(gLYAG, gName("LYAG"); gRefI(MSC_Gop); gLnk(MSC_Idx);)               // [retrieve] _loo_k _an_gle between vector and link in _Y -axis-
MakeDisF(gLLEV, gName("LLEV"); gRefI(MSC_Gop);)                              // _load a new _l_e_vel
MakeDisF(gSNSD, gName("SNSD"); gRefI(MSC_Gop);)                              // _seek disc to location of _n_s_d file
MakeDisF(gGLBI, gName("GLBI");)                                              // _g_lo_bal _initialize ([re]init all globals)
MakeDisF(gBACE, gName("BACE"); gLnk(MSC_Idx); gRefI(MSC_Gop); gLnk(MSC_Pso);)// _broadcast _asynchronously handled collision event
MakeDisF(gTCOL, gName("TCOL"); gLnk(MSC_Idx); gRefI(MSC_Gop);)               // _test point for _c_o_llision with object
MakeDisF(gCARD, gName("CARD"); gIm(MSC_Pso);  gRefI(MSC_Gop);)               // memory _c_a_r_d routine

MakeDisF(gGLBR, gName("GLBR"); gRefI(_GopS_);)
MakeDisF(gGLBW, gName("GLBW"); gRefI(_GopS_);  gRefI(_GopD_);)
MakeDisF(gCVMR, gName("CVMR"); gLnk(CVM_Lnk);  gCol(CVM_Cdi);)
MakeDisF(gCVMW, gName("CVMW"); gLnk(CVM_Lnk);  gCol(CVM_Cdi); gRefI(CVM_Gop);)
MakeDisF(gANID, gName("ANID"); gRefI(_GopS_);  gRefO(_GopD_);)
MakeDisF(gANIS, gName("ANIS"); gIm(ANI_FrI);   gIm(ANI_Seq);  gIm(ANI_Wt);  gIm(ANI_Flp);)
MakeDisF(gANIF, gName("ANIF"); gRefI(ANI_FrG); gIm(ANI_Wt);   gIm(ANI_Flp);)
MakeDisF(gEVNT, gName("EVNT"); gRefI(EVT_Gop); gLnk(EVT_Rec); gIm(EVT_ArC); gReg(EVT_Cnd);)
MakeDisF(gEVNB, gName("EVNB"); gRefI(EVT_Gop); gLnk(EVT_Rec); gIm(EVT_ArC); gReg(EVT_Cnd);)
MakeDisF(gEVNU, gName("EVNU"); gRefI(EVT_Gop); gLnk(EVT_Rec); gIm(EVT_ArC); gReg(EVT_Cnd);)
MakeDisF(gCHLD, gName("CHLD"); gIm(CHD_SpC);   gIm(CHD_Typ);  gIm(CHD_Sub); gIm(CHD_ArC);)
MakeDisF(gCHLF, gName("CHLF"); gIm(CHD_SpC);   gIm(CHD_Typ);  gIm(CHD_Sub); gIm(CHD_ArC);)
MakeDisF(gNTRY, gName("NTRY"); gRefI(_GopB_);  gRefI(_GopA_);) // TODO: expand NTRY
MakeDisF(gSNDA, gName("SNDA"); gRefI(_GopA_);  gRefI(_GopB_);)
MakeDisF(gSNDB, gName("SNDB"); gRefI(SND_Gop); gReg(SND_Ri);  gIm(SND_Vol);)
MakeDisF(gPAD,  gName("PAD");  gRefI(PAD_Gop); gIm(PAD_Pct);  gIm(PAD_Sct); gIm(PAD_Dir); gIm(PAD_Flg);)

MakeDisF(gNOP,  gName("NOP");)
MakeDisF(gDBG,  gName("GDBG");) // for now
MakeDisF(gUNK,  gName("UNK");)
MakeDisF(gILL,  gName("ILL");)

TdisGOOLF disGOOL_CFL[][4] = {
{ gBRA  , gBNEZ , gBEQZ , gBRAC },
{ gCST  , gCSNZ , gCSEZ , gCSTC },
{ gRET  , gRTNZ , gRTEZ , gRETC },
{ gSCND , gSCNZ , gSCEZ , gCCND } };

MakeDisFg(gCFL,    disGOOL_CFL[_Os_][_Cond_](ins, pc);)

TdisGOOLF disGOOL_RSTT[][4] = {
{ gNOP  , gGBNT , gGBZT , gNOP  },
{ gRST  , gRSNT , gRSZT , gRSCT },
{ gRNT  , gRNNT , gRNZT , gRNCT },
{ gEBT  , gEBNT , gEBZT , gEBCT } };

MakeDisFg(gRSTT,   disGOOL_RSTT[_Os_][_Cond_](ins, pc);)

TdisGOOLF disGOOL_RSTF[][4] = {
{ gNOP  , gGBNF , gGBZF , gNOP  },
{ gRSF  , gRSNF , gRSZF , gRSCF },
{ gRNF  , gRNNF , gRNZF , gRNCF },
{ gEBF  , gEBNF , gEBZF , gEBCF } };

MakeDisFg(gRSTF,   disGOOL_RSTF[_Os_][_Cond_](ins, pc);)

TdisGOOLF disGOOL_VECA[] = {
  gPATH , gPROJ , gAPCR , gNOP  , gMATT , gMTRT , gTVTX , gSNMT };

MakeDisFg(gVECA,   disGOOL_VECA[VEC_Pso](ins, pc);)

TdisGOOLF disGOOL_VECB[] = {
  gRBND , gVCU1 , gVCU2 , gVCU3 , gVCU4 , gVCU5 , gILL  , gILL  };

MakeDisFg(gVECB,   disGOOL_VECB[VEC_Pso](ins, pc);)

TdisGOOLF disGOOL_MSC_SBW[] = {
  gSB3  , gCB3  , gSPTB , gCPTB , gPSSB , gRSSB , gNOP  , gNOP  ,
  gSXB  , gCXB  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  };

MakeDisFg(gSBW,    disGOOL_MSC_SBW[MSC_Sso](ins, pc);)

TdisGOOLF disGOOL_MSC_SBR[] = {
  gNOP  , gTNRS , gTB3  , gTPTB , gILL  , gILL  , gILL  , gILL  };

MakeDisFg(gSBR,    disGOOL_MSC_SBR[MSC_Sso](ins, pc);)

TdisGOOLF disGOOL_MSC_MSCB[] = {
  gSAVE , gLOAD , gSPAR , gUNKB , gZONE , gSNDU , gSNDC , gTRMO ,
  gLYAG , gLLEV , gSNSD , gGLBI , gILL  , gILL  , gILL  , gILL  };

MakeDisFg(gMSCB,   disGOOL_MSC_MSCB[MSC_Sso](ins, pc);)

TdisGOOLF disGOOL_MSC[] = {
  gEARG , gDIST , gAGDP , gLR   , gSR   , gLXAG , gVDST , gFIND ,
  gSNRS , gSZON , gSBW  , gSBR  , gMSCB , gBACE , gTCOL , gCARD };

MakeDisFg(gMSC,    disGOOL_MSC[MSC_Pso](ins, pc);)

TdisGOOLF disGOOL[] = {
  gADD  , gSUB  , gMULT , gDIV  , gCEQ  , gANDL , gORL  , gANDB ,
  gORB  , gSLT  , gSLE  , gSGT  , gSGE  , gMOD  , gXOR  , gTST  ,
  gRND  , gMOVE , gNOTL , gTRI  , gLEA  , gSHA  , gPSHV , gNOTB ,
  gMOVC , gABS  , gPAD  , gSPD  , gMSC  , gPRS  , gSSAW , gGLBR ,
  gGLBW , gANGD , gAPCH , gCVMR , gCVMW , gROT  , gPSHP , gANID ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  ,
  gDBG  , gUNK  , gCFL  , gANIS , gANIF , gVECA , gJAL  , gEVNT ,
  gRSTT , gRSTF , gCHLD , gNTRY , gSNDA , gSNDB , gVECB , gEVNB ,
  gEVNU , gCHLF , gILL  , gILL  , gILL  , gILL  , gILL  , gILL  };

MakeDisFg(GoolDisassemble, disGOOL[ins >> 24](ins, pc);)

char *GoolFormatRef(char *str, uint32_t ref, int mode) {
  uint32_t reg, ireg, lnk;
  uint32_t integer, fraction, sign, offset;

  if ((ref & 0xFFF) == 0xE1F) {
    if (mode == 0)
      scatf(str, " pop(),");
    else if (mode == 1)
      scatf(str, " push(),");
    else if (mode == 2)
      scatf(str, " inout(),");
  }
  else if ((ref & 0xE00) == 0xE00) {
    reg = ref & 0x1FF;
    scatf(str, " obj[0x%x],", (reg*4)+0x60);
  }
  else if ((ref & 0x0800) == 0) {
    ireg = ref & 0x3FF;
    if ((ref & 0x400) == 0)
      scatf(str, " ireg[0x%x],", (ireg*4));
    else
      scatf(str, " pool[0x%x],", (ireg*4));
  }
  else if ((ref & 0x400) == 0) {
    if ((ref & 0x200) == 0) {
      integer = (ref & 0xFF);
      if (integer) {
        sign = (ref & 0x100) >> 8;
        if (sign) {
          integer = 0x100 - integer;
          scatf(str, " -0x%x00,", integer);
        }
        else
          scatf(str, " 0x%x00,", integer);
      }
      else
        scatf(str, " 0,");
    }
    else if ((ref & 0x100) == 0) {
      integer = (ref & 0x70) >> 4;
      fraction = (ref & 0xF);
      if (ref & 0x7F) {
        sign = (ref & 0x80) >> 7;
        if (sign) {
          integer = 8 - integer;
          fraction = 16 - fraction;
          scatf(str, " -%d.%-4.4d,", integer, fraction*625);
        }
        else
          scatf(str, " %d.%-4.4d,", integer, fraction*625);
      }
      else
        scatf(str, " 0,");
    }
    else if ((ref & 0x80) == 0) {
      sign = (ref & 0x40) >> 6;
      offset = (ref & 0x3F);
      if (sign) {
        offset = 0x40 - offset;
        scatf(str, " -0x%x($fp),", offset*4);
      }
      else
        scatf(str, " 0x%x($fp),", offset*4);
    }
    else if (ref == 0xBE0)
      scatf(str, " 0,");
    else if (ref == 0xBF0)
      scatf(str, " pop(), pop(),");
    else
      scatf(str, " INVALID OPERAND,");
  }
  else {
    lnk = (ref & 0x1C0) >> 6;
    GoolFormatLnk(str, lnk);
    str[strlen(str)-1] = 0;
    reg = (ref & 0x3F);
    scatf(str, "[0x%x],", 0x60+(reg*4));
  }
  return str;
}

char *GoolFormatLnk(char *str, uint32_t lnk) {
  static char *disRNameLnk[] = {
    "self"    , "parent"  , "sibling" , "child"   ,
    "creator" , "player"  , "collider", "sender"  };

  scatf(str, " %s,", disRNameLnk[lnk]);
  return str;
}

char *GoolFormatVec(char *str, uint32_t vec) {
  static char *disRNameVec[] = {
    "trans"   , "rot"      , "scale"       ,
    "velocity", "targetrot", "rotvelocity" };

  scatf(str, " %s,", disRNameVec[vec]);
  return str;
}

char *GoolFormatCol(char *str, uint32_t col) {
  static char *disRNameCol[] = {
    "lightmatrix.v1x", "lightmatrix.v1y", "lightmatrix.v1z",
    "lightmatrix.v2x", "lightmatrix.v2y", "lightmatrix.v2z",
    "lightmatrix.v3x", "lightmatrix.v3y", "lightmatrix.v3z",
    "color.r"        , "color.g"        , "color.b"        ,
    "colormatrix.v1x", "colormatrix.v1y", "colormatrix.v1z",
    "colormatrix.v2x", "colormatrix.v2y", "colormatrix.v2z",
    "colormatrix.v3x", "colormatrix.v3y", "colormatrix.v3z",
    "intensity.r"    , "intensity.g"    , "intensity.b"    };
  
  scatf(str, " %s,", disRNameCol[col]);
  return str;
}
