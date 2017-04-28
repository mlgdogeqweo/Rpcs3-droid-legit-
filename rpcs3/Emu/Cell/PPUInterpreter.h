#pragma once

#include "PPUOpcodes.h"

class ppu_thread;

struct ppu_interpreter
{
	static bool MFVSCR(ppu_thread&, ppu_opcode_t);
	static bool MTVSCR(ppu_thread&, ppu_opcode_t);
	static bool VADDCUW(ppu_thread&, ppu_opcode_t);
	static bool VADDFP(ppu_thread&, ppu_opcode_t);
	static bool VADDUBM(ppu_thread&, ppu_opcode_t);
	static bool VADDUHM(ppu_thread&, ppu_opcode_t);
	static bool VADDUWM(ppu_thread&, ppu_opcode_t);
	static bool VAND(ppu_thread&, ppu_opcode_t);
	static bool VANDC(ppu_thread&, ppu_opcode_t);
	static bool VAVGSB(ppu_thread&, ppu_opcode_t);
	static bool VAVGSH(ppu_thread&, ppu_opcode_t);
	static bool VAVGSW(ppu_thread&, ppu_opcode_t);
	static bool VAVGUB(ppu_thread&, ppu_opcode_t);
	static bool VAVGUH(ppu_thread&, ppu_opcode_t);
	static bool VAVGUW(ppu_thread&, ppu_opcode_t);
	static bool VCFSX(ppu_thread&, ppu_opcode_t);
	static bool VCFUX(ppu_thread&, ppu_opcode_t);
	static bool VCMPBFP(ppu_thread&, ppu_opcode_t);
	static bool VCMPEQFP(ppu_thread&, ppu_opcode_t);
	static bool VCMPEQUB(ppu_thread&, ppu_opcode_t);
	static bool VCMPEQUH(ppu_thread&, ppu_opcode_t);
	static bool VCMPEQUW(ppu_thread&, ppu_opcode_t);
	static bool VCMPGEFP(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTFP(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTSB(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTSH(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTSW(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTUB(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTUH(ppu_thread&, ppu_opcode_t);
	static bool VCMPGTUW(ppu_thread&, ppu_opcode_t);
	static bool VEXPTEFP(ppu_thread&, ppu_opcode_t);
	static bool VLOGEFP(ppu_thread&, ppu_opcode_t);
	static bool VMADDFP(ppu_thread&, ppu_opcode_t);
	static bool VMAXFP(ppu_thread&, ppu_opcode_t);
	static bool VMAXSB(ppu_thread&, ppu_opcode_t);
	static bool VMAXSH(ppu_thread&, ppu_opcode_t);
	static bool VMAXSW(ppu_thread&, ppu_opcode_t);
	static bool VMAXUB(ppu_thread&, ppu_opcode_t);
	static bool VMAXUH(ppu_thread&, ppu_opcode_t);
	static bool VMAXUW(ppu_thread&, ppu_opcode_t);
	static bool VMINFP(ppu_thread&, ppu_opcode_t);
	static bool VMINSB(ppu_thread&, ppu_opcode_t);
	static bool VMINSH(ppu_thread&, ppu_opcode_t);
	static bool VMINSW(ppu_thread&, ppu_opcode_t);
	static bool VMINUB(ppu_thread&, ppu_opcode_t);
	static bool VMINUH(ppu_thread&, ppu_opcode_t);
	static bool VMINUW(ppu_thread&, ppu_opcode_t);
	static bool VMLADDUHM(ppu_thread&, ppu_opcode_t);
	static bool VMRGHB(ppu_thread&, ppu_opcode_t);
	static bool VMRGHH(ppu_thread&, ppu_opcode_t);
	static bool VMRGHW(ppu_thread&, ppu_opcode_t);
	static bool VMRGLB(ppu_thread&, ppu_opcode_t);
	static bool VMRGLH(ppu_thread&, ppu_opcode_t);
	static bool VMRGLW(ppu_thread&, ppu_opcode_t);
	static bool VMSUMMBM(ppu_thread&, ppu_opcode_t);
	static bool VMSUMSHM(ppu_thread&, ppu_opcode_t);
	static bool VMSUMSHS(ppu_thread&, ppu_opcode_t);
	static bool VMSUMUBM(ppu_thread&, ppu_opcode_t);
	static bool VMSUMUHM(ppu_thread&, ppu_opcode_t);
	static bool VMSUMUHS(ppu_thread&, ppu_opcode_t);
	static bool VMULESB(ppu_thread&, ppu_opcode_t);
	static bool VMULESH(ppu_thread&, ppu_opcode_t);
	static bool VMULEUB(ppu_thread&, ppu_opcode_t);
	static bool VMULEUH(ppu_thread&, ppu_opcode_t);
	static bool VMULOSB(ppu_thread&, ppu_opcode_t);
	static bool VMULOSH(ppu_thread&, ppu_opcode_t);
	static bool VMULOUB(ppu_thread&, ppu_opcode_t);
	static bool VMULOUH(ppu_thread&, ppu_opcode_t);
	static bool VNMSUBFP(ppu_thread&, ppu_opcode_t);
	static bool VNOR(ppu_thread&, ppu_opcode_t);
	static bool VOR(ppu_thread&, ppu_opcode_t);
	static bool VPERM(ppu_thread&, ppu_opcode_t);
	static bool VPKPX(ppu_thread&, ppu_opcode_t);
	static bool VPKSWUS(ppu_thread&, ppu_opcode_t);
	static bool VPKUHUM(ppu_thread&, ppu_opcode_t);
	static bool VPKUHUS(ppu_thread&, ppu_opcode_t);
	static bool VPKUWUM(ppu_thread&, ppu_opcode_t);
	static bool VPKUWUS(ppu_thread&, ppu_opcode_t);
	static bool VREFP(ppu_thread&, ppu_opcode_t);
	static bool VRFIM(ppu_thread&, ppu_opcode_t);
	static bool VRFIN(ppu_thread&, ppu_opcode_t);
	static bool VRFIP(ppu_thread&, ppu_opcode_t);
	static bool VRFIZ(ppu_thread&, ppu_opcode_t);
	static bool VRLB(ppu_thread&, ppu_opcode_t);
	static bool VRLH(ppu_thread&, ppu_opcode_t);
	static bool VRLW(ppu_thread&, ppu_opcode_t);
	static bool VRSQRTEFP(ppu_thread&, ppu_opcode_t);
	static bool VSEL(ppu_thread&, ppu_opcode_t);
	static bool VSL(ppu_thread&, ppu_opcode_t);
	static bool VSLB(ppu_thread&, ppu_opcode_t);
	static bool VSLDOI(ppu_thread&, ppu_opcode_t);
	static bool VSLH(ppu_thread&, ppu_opcode_t);
	static bool VSLO(ppu_thread&, ppu_opcode_t);
	static bool VSLW(ppu_thread&, ppu_opcode_t);
	static bool VSPLTB(ppu_thread&, ppu_opcode_t);
	static bool VSPLTH(ppu_thread&, ppu_opcode_t);
	static bool VSPLTISB(ppu_thread&, ppu_opcode_t);
	static bool VSPLTISH(ppu_thread&, ppu_opcode_t);
	static bool VSPLTISW(ppu_thread&, ppu_opcode_t);
	static bool VSPLTW(ppu_thread&, ppu_opcode_t);
	static bool VSR(ppu_thread&, ppu_opcode_t);
	static bool VSRAB(ppu_thread&, ppu_opcode_t);
	static bool VSRAH(ppu_thread&, ppu_opcode_t);
	static bool VSRAW(ppu_thread&, ppu_opcode_t);
	static bool VSRB(ppu_thread&, ppu_opcode_t);
	static bool VSRH(ppu_thread&, ppu_opcode_t);
	static bool VSRO(ppu_thread&, ppu_opcode_t);
	static bool VSRW(ppu_thread&, ppu_opcode_t);
	static bool VSUBCUW(ppu_thread&, ppu_opcode_t);
	static bool VSUBFP(ppu_thread&, ppu_opcode_t);
	static bool VSUBSWS(ppu_thread&, ppu_opcode_t);
	static bool VSUBUBM(ppu_thread&, ppu_opcode_t);
	static bool VSUBUHM(ppu_thread&, ppu_opcode_t);
	static bool VSUBUWM(ppu_thread&, ppu_opcode_t);
	static bool VSUBUWS(ppu_thread&, ppu_opcode_t);
	static bool VSUMSWS(ppu_thread&, ppu_opcode_t);
	static bool VSUM2SWS(ppu_thread&, ppu_opcode_t);
	static bool VSUM4SBS(ppu_thread&, ppu_opcode_t);
	static bool VSUM4SHS(ppu_thread&, ppu_opcode_t);
	static bool VSUM4UBS(ppu_thread&, ppu_opcode_t);
	static bool VUPKHPX(ppu_thread&, ppu_opcode_t);
	static bool VUPKHSB(ppu_thread&, ppu_opcode_t);
	static bool VUPKHSH(ppu_thread&, ppu_opcode_t);
	static bool VUPKLPX(ppu_thread&, ppu_opcode_t);
	static bool VUPKLSB(ppu_thread&, ppu_opcode_t);
	static bool VUPKLSH(ppu_thread&, ppu_opcode_t);
	static bool VXOR(ppu_thread&, ppu_opcode_t);
	static bool TDI(ppu_thread&, ppu_opcode_t);
	static bool TWI(ppu_thread&, ppu_opcode_t);
	static bool MULLI(ppu_thread&, ppu_opcode_t);
	static bool SUBFIC(ppu_thread&, ppu_opcode_t);
	static bool CMPLI(ppu_thread&, ppu_opcode_t);
	static bool CMPI(ppu_thread&, ppu_opcode_t);
	static bool ADDIC(ppu_thread&, ppu_opcode_t);
	static bool ADDI(ppu_thread&, ppu_opcode_t);
	static bool ADDIS(ppu_thread&, ppu_opcode_t);
	static bool BC(ppu_thread&, ppu_opcode_t);
	static bool SC(ppu_thread&, ppu_opcode_t);
	static bool B(ppu_thread&, ppu_opcode_t);
	static bool MCRF(ppu_thread&, ppu_opcode_t);
	static bool BCLR(ppu_thread&, ppu_opcode_t);
	static bool CRNOR(ppu_thread&, ppu_opcode_t);
	static bool CRANDC(ppu_thread&, ppu_opcode_t);
	static bool ISYNC(ppu_thread&, ppu_opcode_t);
	static bool CRXOR(ppu_thread&, ppu_opcode_t);
	static bool CRNAND(ppu_thread&, ppu_opcode_t);
	static bool CRAND(ppu_thread&, ppu_opcode_t);
	static bool CREQV(ppu_thread&, ppu_opcode_t);
	static bool CRORC(ppu_thread&, ppu_opcode_t);
	static bool CROR(ppu_thread&, ppu_opcode_t);
	static bool BCCTR(ppu_thread&, ppu_opcode_t);
	static bool RLWIMI(ppu_thread&, ppu_opcode_t);
	static bool RLWINM(ppu_thread&, ppu_opcode_t);
	static bool RLWNM(ppu_thread&, ppu_opcode_t);
	static bool ORI(ppu_thread&, ppu_opcode_t);
	static bool ORIS(ppu_thread&, ppu_opcode_t);
	static bool XORI(ppu_thread&, ppu_opcode_t);
	static bool XORIS(ppu_thread&, ppu_opcode_t);
	static bool ANDI(ppu_thread&, ppu_opcode_t);
	static bool ANDIS(ppu_thread&, ppu_opcode_t);
	static bool RLDICL(ppu_thread&, ppu_opcode_t);
	static bool RLDICR(ppu_thread&, ppu_opcode_t);
	static bool RLDIC(ppu_thread&, ppu_opcode_t);
	static bool RLDIMI(ppu_thread&, ppu_opcode_t);
	static bool RLDCL(ppu_thread&, ppu_opcode_t);
	static bool RLDCR(ppu_thread&, ppu_opcode_t);
	static bool CMP(ppu_thread&, ppu_opcode_t);
	static bool TW(ppu_thread&, ppu_opcode_t);
	static bool LVSL(ppu_thread&, ppu_opcode_t);
	static bool LVEBX(ppu_thread&, ppu_opcode_t);
	static bool SUBFC(ppu_thread&, ppu_opcode_t);
	static bool MULHDU(ppu_thread&, ppu_opcode_t);
	static bool ADDC(ppu_thread&, ppu_opcode_t);
	static bool MULHWU(ppu_thread&, ppu_opcode_t);
	static bool MFOCRF(ppu_thread&, ppu_opcode_t);
	static bool LWARX(ppu_thread&, ppu_opcode_t);
	static bool LDX(ppu_thread&, ppu_opcode_t);
	static bool LWZX(ppu_thread&, ppu_opcode_t);
	static bool SLW(ppu_thread&, ppu_opcode_t);
	static bool CNTLZW(ppu_thread&, ppu_opcode_t);
	static bool SLD(ppu_thread&, ppu_opcode_t);
	static bool AND(ppu_thread&, ppu_opcode_t);
	static bool CMPL(ppu_thread&, ppu_opcode_t);
	static bool LVSR(ppu_thread&, ppu_opcode_t);
	static bool LVEHX(ppu_thread&, ppu_opcode_t);
	static bool SUBF(ppu_thread&, ppu_opcode_t);
	static bool LDUX(ppu_thread&, ppu_opcode_t);
	static bool DCBST(ppu_thread&, ppu_opcode_t);
	static bool LWZUX(ppu_thread&, ppu_opcode_t);
	static bool CNTLZD(ppu_thread&, ppu_opcode_t);
	static bool ANDC(ppu_thread&, ppu_opcode_t);
	static bool TD(ppu_thread&, ppu_opcode_t);
	static bool LVEWX(ppu_thread&, ppu_opcode_t);
	static bool MULHD(ppu_thread&, ppu_opcode_t);
	static bool MULHW(ppu_thread&, ppu_opcode_t);
	static bool LDARX(ppu_thread&, ppu_opcode_t);
	static bool DCBF(ppu_thread&, ppu_opcode_t);
	static bool LBZX(ppu_thread&, ppu_opcode_t);
	static bool LVX(ppu_thread&, ppu_opcode_t);
	static bool NEG(ppu_thread&, ppu_opcode_t);
	static bool LBZUX(ppu_thread&, ppu_opcode_t);
	static bool NOR(ppu_thread&, ppu_opcode_t);
	static bool STVEBX(ppu_thread&, ppu_opcode_t);
	static bool SUBFE(ppu_thread&, ppu_opcode_t);
	static bool ADDE(ppu_thread&, ppu_opcode_t);
	static bool MTOCRF(ppu_thread&, ppu_opcode_t);
	static bool STDX(ppu_thread&, ppu_opcode_t);
	static bool STWCX(ppu_thread&, ppu_opcode_t);
	static bool STWX(ppu_thread&, ppu_opcode_t);
	static bool STVEHX(ppu_thread&, ppu_opcode_t);
	static bool STDUX(ppu_thread&, ppu_opcode_t);
	static bool STWUX(ppu_thread&, ppu_opcode_t);
	static bool STVEWX(ppu_thread&, ppu_opcode_t);
	static bool SUBFZE(ppu_thread&, ppu_opcode_t);
	static bool ADDZE(ppu_thread&, ppu_opcode_t);
	static bool STDCX(ppu_thread&, ppu_opcode_t);
	static bool STBX(ppu_thread&, ppu_opcode_t);
	static bool STVX(ppu_thread&, ppu_opcode_t);
	static bool MULLD(ppu_thread&, ppu_opcode_t);
	static bool SUBFME(ppu_thread&, ppu_opcode_t);
	static bool ADDME(ppu_thread&, ppu_opcode_t);
	static bool MULLW(ppu_thread&, ppu_opcode_t);
	static bool DCBTST(ppu_thread&, ppu_opcode_t);
	static bool STBUX(ppu_thread&, ppu_opcode_t);
	static bool ADD(ppu_thread&, ppu_opcode_t);
	static bool DCBT(ppu_thread&, ppu_opcode_t);
	static bool LHZX(ppu_thread&, ppu_opcode_t);
	static bool EQV(ppu_thread&, ppu_opcode_t);
	static bool ECIWX(ppu_thread&, ppu_opcode_t);
	static bool LHZUX(ppu_thread&, ppu_opcode_t);
	static bool XOR(ppu_thread&, ppu_opcode_t);
	static bool MFSPR(ppu_thread&, ppu_opcode_t);
	static bool LWAX(ppu_thread&, ppu_opcode_t);
	static bool DST(ppu_thread&, ppu_opcode_t);
	static bool LHAX(ppu_thread&, ppu_opcode_t);
	static bool LVXL(ppu_thread&, ppu_opcode_t);
	static bool MFTB(ppu_thread&, ppu_opcode_t);
	static bool LWAUX(ppu_thread&, ppu_opcode_t);
	static bool DSTST(ppu_thread&, ppu_opcode_t);
	static bool LHAUX(ppu_thread&, ppu_opcode_t);
	static bool STHX(ppu_thread&, ppu_opcode_t);
	static bool ORC(ppu_thread&, ppu_opcode_t);
	static bool ECOWX(ppu_thread&, ppu_opcode_t);
	static bool STHUX(ppu_thread&, ppu_opcode_t);
	static bool OR(ppu_thread&, ppu_opcode_t);
	static bool DIVDU(ppu_thread&, ppu_opcode_t);
	static bool DIVWU(ppu_thread&, ppu_opcode_t);
	static bool MTSPR(ppu_thread&, ppu_opcode_t);
	static bool DCBI(ppu_thread&, ppu_opcode_t);
	static bool NAND(ppu_thread&, ppu_opcode_t);
	static bool STVXL(ppu_thread&, ppu_opcode_t);
	static bool DIVD(ppu_thread&, ppu_opcode_t);
	static bool DIVW(ppu_thread&, ppu_opcode_t);
	static bool LVLX(ppu_thread&, ppu_opcode_t);
	static bool LDBRX(ppu_thread&, ppu_opcode_t);
	static bool LSWX(ppu_thread&, ppu_opcode_t);
	static bool LWBRX(ppu_thread&, ppu_opcode_t);
	static bool LFSX(ppu_thread&, ppu_opcode_t);
	static bool SRW(ppu_thread&, ppu_opcode_t);
	static bool SRD(ppu_thread&, ppu_opcode_t);
	static bool LVRX(ppu_thread&, ppu_opcode_t);
	static bool LSWI(ppu_thread&, ppu_opcode_t);
	static bool LFSUX(ppu_thread&, ppu_opcode_t);
	static bool SYNC(ppu_thread&, ppu_opcode_t);
	static bool LFDX(ppu_thread&, ppu_opcode_t);
	static bool LFDUX(ppu_thread&, ppu_opcode_t);
	static bool STVLX(ppu_thread&, ppu_opcode_t);
	static bool STDBRX(ppu_thread&, ppu_opcode_t);
	static bool STSWX(ppu_thread&, ppu_opcode_t);
	static bool STWBRX(ppu_thread&, ppu_opcode_t);
	static bool STFSX(ppu_thread&, ppu_opcode_t);
	static bool STVRX(ppu_thread&, ppu_opcode_t);
	static bool STFSUX(ppu_thread&, ppu_opcode_t);
	static bool STSWI(ppu_thread&, ppu_opcode_t);
	static bool STFDX(ppu_thread&, ppu_opcode_t);
	static bool STFDUX(ppu_thread&, ppu_opcode_t);
	static bool LVLXL(ppu_thread&, ppu_opcode_t);
	static bool LHBRX(ppu_thread&, ppu_opcode_t);
	static bool SRAW(ppu_thread&, ppu_opcode_t);
	static bool SRAD(ppu_thread&, ppu_opcode_t);
	static bool LVRXL(ppu_thread&, ppu_opcode_t);
	static bool DSS(ppu_thread&, ppu_opcode_t);
	static bool SRAWI(ppu_thread&, ppu_opcode_t);
	static bool SRADI(ppu_thread&, ppu_opcode_t);
	static bool EIEIO(ppu_thread&, ppu_opcode_t);
	static bool STVLXL(ppu_thread&, ppu_opcode_t);
	static bool STHBRX(ppu_thread&, ppu_opcode_t);
	static bool EXTSH(ppu_thread&, ppu_opcode_t);
	static bool STVRXL(ppu_thread&, ppu_opcode_t);
	static bool EXTSB(ppu_thread&, ppu_opcode_t);
	static bool STFIWX(ppu_thread&, ppu_opcode_t);
	static bool EXTSW(ppu_thread&, ppu_opcode_t);
	static bool ICBI(ppu_thread&, ppu_opcode_t);
	static bool DCBZ(ppu_thread&, ppu_opcode_t);
	static bool LWZ(ppu_thread&, ppu_opcode_t);
	static bool LWZU(ppu_thread&, ppu_opcode_t);
	static bool LBZ(ppu_thread&, ppu_opcode_t);
	static bool LBZU(ppu_thread&, ppu_opcode_t);
	static bool STW(ppu_thread&, ppu_opcode_t);
	static bool STWU(ppu_thread&, ppu_opcode_t);
	static bool STB(ppu_thread&, ppu_opcode_t);
	static bool STBU(ppu_thread&, ppu_opcode_t);
	static bool LHZ(ppu_thread&, ppu_opcode_t);
	static bool LHZU(ppu_thread&, ppu_opcode_t);
	static bool LHA(ppu_thread&, ppu_opcode_t);
	static bool LHAU(ppu_thread&, ppu_opcode_t);
	static bool STH(ppu_thread&, ppu_opcode_t);
	static bool STHU(ppu_thread&, ppu_opcode_t);
	static bool LMW(ppu_thread&, ppu_opcode_t);
	static bool STMW(ppu_thread&, ppu_opcode_t);
	static bool LFS(ppu_thread&, ppu_opcode_t);
	static bool LFSU(ppu_thread&, ppu_opcode_t);
	static bool LFD(ppu_thread&, ppu_opcode_t);
	static bool LFDU(ppu_thread&, ppu_opcode_t);
	static bool STFS(ppu_thread&, ppu_opcode_t);
	static bool STFSU(ppu_thread&, ppu_opcode_t);
	static bool STFD(ppu_thread&, ppu_opcode_t);
	static bool STFDU(ppu_thread&, ppu_opcode_t);
	static bool LD(ppu_thread&, ppu_opcode_t);
	static bool LDU(ppu_thread&, ppu_opcode_t);
	static bool LWA(ppu_thread&, ppu_opcode_t);
	static bool STD(ppu_thread&, ppu_opcode_t);
	static bool STDU(ppu_thread&, ppu_opcode_t);
	static bool FDIVS(ppu_thread&, ppu_opcode_t);
	static bool FSUBS(ppu_thread&, ppu_opcode_t);
	static bool FADDS(ppu_thread&, ppu_opcode_t);
	static bool FSQRTS(ppu_thread&, ppu_opcode_t);
	static bool FRES(ppu_thread&, ppu_opcode_t);
	static bool FMULS(ppu_thread&, ppu_opcode_t);
	static bool FMADDS(ppu_thread&, ppu_opcode_t);
	static bool FMSUBS(ppu_thread&, ppu_opcode_t);
	static bool FNMSUBS(ppu_thread&, ppu_opcode_t);
	static bool FNMADDS(ppu_thread&, ppu_opcode_t);
	static bool MTFSB1(ppu_thread&, ppu_opcode_t);
	static bool MCRFS(ppu_thread&, ppu_opcode_t);
	static bool MTFSB0(ppu_thread&, ppu_opcode_t);
	static bool MTFSFI(ppu_thread&, ppu_opcode_t);
	static bool MFFS(ppu_thread&, ppu_opcode_t);
	static bool MTFSF(ppu_thread&, ppu_opcode_t);
	static bool FCMPU(ppu_thread&, ppu_opcode_t);
	static bool FRSP(ppu_thread&, ppu_opcode_t);
	static bool FCTIW(ppu_thread&, ppu_opcode_t);
	static bool FCTIWZ(ppu_thread&, ppu_opcode_t);
	static bool FDIV(ppu_thread&, ppu_opcode_t);
	static bool FSUB(ppu_thread&, ppu_opcode_t);
	static bool FADD(ppu_thread&, ppu_opcode_t);
	static bool FSQRT(ppu_thread&, ppu_opcode_t);
	static bool FSEL(ppu_thread&, ppu_opcode_t);
	static bool FMUL(ppu_thread&, ppu_opcode_t);
	static bool FRSQRTE(ppu_thread&, ppu_opcode_t);
	static bool FMSUB(ppu_thread&, ppu_opcode_t);
	static bool FMADD(ppu_thread&, ppu_opcode_t);
	static bool FNMSUB(ppu_thread&, ppu_opcode_t);
	static bool FNMADD(ppu_thread&, ppu_opcode_t);
	static bool FCMPO(ppu_thread&, ppu_opcode_t);
	static bool FNEG(ppu_thread&, ppu_opcode_t);
	static bool FMR(ppu_thread&, ppu_opcode_t);
	static bool FNABS(ppu_thread&, ppu_opcode_t);
	static bool FABS(ppu_thread&, ppu_opcode_t);
	static bool FCTID(ppu_thread&, ppu_opcode_t);
	static bool FCTIDZ(ppu_thread&, ppu_opcode_t);
	static bool FCFID(ppu_thread&, ppu_opcode_t);

	static bool UNK(ppu_thread&, ppu_opcode_t);
};

struct ppu_interpreter_precise final : ppu_interpreter
{
	static bool VPKSHSS(ppu_thread&, ppu_opcode_t);
	static bool VPKSHUS(ppu_thread&, ppu_opcode_t);
	static bool VPKSWSS(ppu_thread&, ppu_opcode_t);
	static bool VADDSBS(ppu_thread&, ppu_opcode_t);
	static bool VADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VADDSWS(ppu_thread&, ppu_opcode_t);
	static bool VADDUBS(ppu_thread&, ppu_opcode_t);
	static bool VADDUHS(ppu_thread&, ppu_opcode_t);
	static bool VADDUWS(ppu_thread&, ppu_opcode_t);
	static bool VSUBSBS(ppu_thread&, ppu_opcode_t);
	static bool VSUBSHS(ppu_thread&, ppu_opcode_t);
	static bool VSUBUBS(ppu_thread&, ppu_opcode_t);
	static bool VSUBUHS(ppu_thread&, ppu_opcode_t);
	static bool VMHADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VMHRADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VCTSXS(ppu_thread&, ppu_opcode_t);
	static bool VCTUXS(ppu_thread&, ppu_opcode_t);
};

struct ppu_interpreter_fast final : ppu_interpreter
{
	static bool VPKSHSS(ppu_thread&, ppu_opcode_t);
	static bool VPKSHUS(ppu_thread&, ppu_opcode_t);
	static bool VPKSWSS(ppu_thread&, ppu_opcode_t);
	static bool VADDSBS(ppu_thread&, ppu_opcode_t);
	static bool VADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VADDSWS(ppu_thread&, ppu_opcode_t);
	static bool VADDUBS(ppu_thread&, ppu_opcode_t);
	static bool VADDUHS(ppu_thread&, ppu_opcode_t);
	static bool VADDUWS(ppu_thread&, ppu_opcode_t);
	static bool VSUBSBS(ppu_thread&, ppu_opcode_t);
	static bool VSUBSHS(ppu_thread&, ppu_opcode_t);
	static bool VSUBUBS(ppu_thread&, ppu_opcode_t);
	static bool VSUBUHS(ppu_thread&, ppu_opcode_t);
	static bool VMHADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VMHRADDSHS(ppu_thread&, ppu_opcode_t);
	static bool VCTSXS(ppu_thread&, ppu_opcode_t);
	static bool VCTUXS(ppu_thread&, ppu_opcode_t);
};