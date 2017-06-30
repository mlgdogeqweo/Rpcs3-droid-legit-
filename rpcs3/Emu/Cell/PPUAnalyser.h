#pragma once

#include <string>
#include <map>
#include <set>

#include "Utilities/bit_set.h"
#include "Utilities/BEType.h"

// PPU Function Attributes
enum class ppu_attr : u32
{
	known_addr,
	known_size,
	no_return,
	no_size,

	__bitset_enum_max
};

// PPU Function Information
struct ppu_function
{
	u32 addr = 0;
	u32 toc = 0;
	u32 size = 0;
	bs_t<ppu_attr> attr{};

	u32 stack_frame = 0;
	u32 trampoline = 0;

	std::map<u32, u32> blocks; // Basic blocks: addr -> size
	std::set<u32> calls; // Set of called functions
	std::set<u32> callers;
	std::string name; // Function name
};

// PPU Relocation Information
struct ppu_reloc
{
	u32 type;
	u32 off;
	u32 ptr;
	u8 index_value;
	u8 index_addr;
};

// PPU Segment Information
struct ppu_segment
{
	u32 addr;
	u32 size;
	u32 type;
	u32 flags;
};

// PPU Module Information
struct ppu_module
{
	std::string name;
	std::vector<ppu_reloc> rels;
	std::vector<ppu_segment> segs;
	std::vector<ppu_function> funcs;
	std::vector<ppu_segment> sections;
};

// Aux
struct ppu_pattern
{
	be_t<u32> opcode;
	be_t<u32> mask;

	ppu_pattern() = default;

	ppu_pattern(u32 op)
		: opcode(op)
		, mask(0xffffffff)
	{
	}

	ppu_pattern(u32 op, u32 ign)
		: opcode(op & ~ign)
		, mask(~ign)
	{
	}
};

struct ppu_pattern_array
{
	const ppu_pattern* ptr;
	std::size_t count;

	template <std::size_t N>
	constexpr ppu_pattern_array(const ppu_pattern(&array)[N])
		: ptr(array)
		, count(N)
	{
	}

	constexpr const ppu_pattern* begin() const
	{
		return ptr;
	}

	constexpr const ppu_pattern* end() const
	{
		return ptr + count;
	}
};

struct ppu_pattern_matrix
{
	const ppu_pattern_array* ptr;
	std::size_t count;

	template <std::size_t N>
	constexpr ppu_pattern_matrix(const ppu_pattern_array(&array)[N])
		: ptr(array)
		, count(N)
	{
	}

	constexpr const ppu_pattern_array* begin() const
	{
		return ptr;
	}

	constexpr const ppu_pattern_array* end() const
	{
		return ptr + count;
	}
};

extern void ppu_validate(const std::string& fname, const std::vector<ppu_function>& funcs, u32 reloc);

extern std::vector<ppu_function> ppu_analyse(const std::vector<std::pair<u32, u32>>& segs, const std::vector<std::pair<u32, u32>>& secs, u32 lib_toc, u32 entry);

// PPU Instruction Type
struct ppu_itype
{
	enum type
	{
		UNK = 0,

		MFVSCR,
		MTVSCR,
		VADDCUW,
		VADDFP,
		VADDSBS,
		VADDSHS,
		VADDSWS,
		VADDUBM,
		VADDUBS,
		VADDUHM,
		VADDUHS,
		VADDUWM,
		VADDUWS,
		VAND,
		VANDC,
		VAVGSB,
		VAVGSH,
		VAVGSW,
		VAVGUB,
		VAVGUH,
		VAVGUW,
		VCFSX,
		VCFUX,
		VCMPBFP,
		VCMPEQFP,
		VCMPEQUB,
		VCMPEQUH,
		VCMPEQUW,
		VCMPGEFP,
		VCMPGTFP,
		VCMPGTSB,
		VCMPGTSH,
		VCMPGTSW,
		VCMPGTUB,
		VCMPGTUH,
		VCMPGTUW,
		VCTSXS,
		VCTUXS,
		VEXPTEFP,
		VLOGEFP,
		VMADDFP,
		VMAXFP,
		VMAXSB,
		VMAXSH,
		VMAXSW,
		VMAXUB,
		VMAXUH,
		VMAXUW,
		VMHADDSHS,
		VMHRADDSHS,
		VMINFP,
		VMINSB,
		VMINSH,
		VMINSW,
		VMINUB,
		VMINUH,
		VMINUW,
		VMLADDUHM,
		VMRGHB,
		VMRGHH,
		VMRGHW,
		VMRGLB,
		VMRGLH,
		VMRGLW,
		VMSUMMBM,
		VMSUMSHM,
		VMSUMSHS,
		VMSUMUBM,
		VMSUMUHM,
		VMSUMUHS,
		VMULESB,
		VMULESH,
		VMULEUB,
		VMULEUH,
		VMULOSB,
		VMULOSH,
		VMULOUB,
		VMULOUH,
		VNMSUBFP,
		VNOR,
		VOR,
		VPERM,
		VPKPX,
		VPKSHSS,
		VPKSHUS,
		VPKSWSS,
		VPKSWUS,
		VPKUHUM,
		VPKUHUS,
		VPKUWUM,
		VPKUWUS,
		VREFP,
		VRFIM,
		VRFIN,
		VRFIP,
		VRFIZ,
		VRLB,
		VRLH,
		VRLW,
		VRSQRTEFP,
		VSEL,
		VSL,
		VSLB,
		VSLDOI,
		VSLH,
		VSLO,
		VSLW,
		VSPLTB,
		VSPLTH,
		VSPLTISB,
		VSPLTISH,
		VSPLTISW,
		VSPLTW,
		VSR,
		VSRAB,
		VSRAH,
		VSRAW,
		VSRB,
		VSRH,
		VSRO,
		VSRW,
		VSUBCUW,
		VSUBFP,
		VSUBSBS,
		VSUBSHS,
		VSUBSWS,
		VSUBUBM,
		VSUBUBS,
		VSUBUHM,
		VSUBUHS,
		VSUBUWM,
		VSUBUWS,
		VSUMSWS,
		VSUM2SWS,
		VSUM4SBS,
		VSUM4SHS,
		VSUM4UBS,
		VUPKHPX,
		VUPKHSB,
		VUPKHSH,
		VUPKLPX,
		VUPKLSB,
		VUPKLSH,
		VXOR,
		TDI,
		TWI,
		MULLI,
		SUBFIC,
		CMPLI,
		CMPI,
		ADDIC,
		ADDI,
		ADDIS,
		BC,
		SC,
		B,
		MCRF,
		BCLR,
		CRNOR,
		CRANDC,
		ISYNC,
		CRXOR,
		CRNAND,
		CRAND,
		CREQV,
		CRORC,
		CROR,
		BCCTR,
		RLWIMI,
		RLWINM,
		RLWNM,
		ORI,
		ORIS,
		XORI,
		XORIS,
		ANDI,
		ANDIS,
		RLDICL,
		RLDICR,
		RLDIC,
		RLDIMI,
		RLDCL,
		RLDCR,
		CMP,
		TW,
		LVSL,
		LVEBX,
		SUBFC,
		ADDC,
		MULHDU,
		MULHWU,
		MFOCRF,
		LWARX,
		LDX,
		LWZX,
		SLW,
		CNTLZW,
		SLD,
		AND,
		CMPL,
		LVSR,
		LVEHX,
		SUBF,
		LDUX,
		DCBST,
		LWZUX,
		CNTLZD,
		ANDC,
		TD,
		LVEWX,
		MULHD,
		MULHW,
		LDARX,
		DCBF,
		LBZX,
		LVX,
		NEG,
		LBZUX,
		NOR,
		STVEBX,
		SUBFE,
		ADDE,
		MTOCRF,
		STDX,
		STWCX,
		STWX,
		STVEHX,
		STDUX,
		STWUX,
		STVEWX,
		SUBFZE,
		ADDZE,
		STDCX,
		STBX,
		STVX,
		SUBFME,
		MULLD,
		ADDME,
		MULLW,
		DCBTST,
		STBUX,
		ADD,
		DCBT,
		LHZX,
		EQV,
		ECIWX,
		LHZUX,
		XOR,
		MFSPR,
		LWAX,
		DST,
		LHAX,
		LVXL,
		MFTB,
		LWAUX,
		DSTST,
		LHAUX,
		STHX,
		ORC,
		ECOWX,
		STHUX,
		OR,
		DIVDU,
		DIVWU,
		MTSPR,
		DCBI,
		NAND,
		STVXL,
		DIVD,
		DIVW,
		LVLX,
		LDBRX,
		LSWX,
		LWBRX,
		LFSX,
		SRW,
		SRD,
		LVRX,
		LSWI,
		LFSUX,
		SYNC,
		LFDX,
		LFDUX,
		STVLX,
		STDBRX,
		STSWX,
		STWBRX,
		STFSX,
		STVRX,
		STFSUX,
		STSWI,
		STFDX,
		STFDUX,
		LVLXL,
		LHBRX,
		SRAW,
		SRAD,
		LVRXL,
		DSS,
		SRAWI,
		SRADI,
		EIEIO,
		STVLXL,
		STHBRX,
		EXTSH,
		STVRXL,
		EXTSB,
		STFIWX,
		EXTSW,
		ICBI,
		DCBZ,
		LWZ,
		LWZU,
		LBZ,
		LBZU,
		STW,
		STWU,
		STB,
		STBU,
		LHZ,
		LHZU,
		LHA,
		LHAU,
		STH,
		STHU,
		LMW,
		STMW,
		LFS,
		LFSU,
		LFD,
		LFDU,
		STFS,
		STFSU,
		STFD,
		STFDU,
		LD,
		LDU,
		LWA,
		STD,
		STDU,
		FDIVS,
		FSUBS,
		FADDS,
		FSQRTS,
		FRES,
		FMULS,
		FMADDS,
		FMSUBS,
		FNMSUBS,
		FNMADDS,
		MTFSB1,
		MCRFS,
		MTFSB0,
		MTFSFI,
		MFFS,
		MTFSF,
		FCMPU,
		FRSP,
		FCTIW,
		FCTIWZ,
		FDIV,
		FSUB,
		FADD,
		FSQRT,
		FSEL,
		FMUL,
		FRSQRTE,
		FMSUB,
		FMADD,
		FNMSUB,
		FNMADD,
		FCMPO,
		FNEG,
		FMR,
		FNABS,
		FABS,
		FCTID,
		FCTIDZ,
		FCFID,
	};

	// Enable address-of operator for ppu_decoder<>
	friend constexpr type operator &(type value)
	{
		return value;
	}
};

// Encode instruction name: 6 bits per character (0x20..0x5f), max 10
static constexpr u64 ppu_iname_encode(const char* ptr, u64 value = 0)
{
	return *ptr == '\0' ? value : ppu_iname_encode(ptr + 1, (*ptr - 0x20) | (value << 6));
}

struct ppu_iname
{
#define NAME(x) x = ppu_iname_encode(#x),

	enum type : u64
	{
	NAME(UNK)
	NAME(MFVSCR)
	NAME(MTVSCR)
	NAME(VADDCUW)
	NAME(VADDFP)
	NAME(VADDSBS)
	NAME(VADDSHS)
	NAME(VADDSWS)
	NAME(VADDUBM)
	NAME(VADDUBS)
	NAME(VADDUHM)
	NAME(VADDUHS)
	NAME(VADDUWM)
	NAME(VADDUWS)
	NAME(VAND)
	NAME(VANDC)
	NAME(VAVGSB)
	NAME(VAVGSH)
	NAME(VAVGSW)
	NAME(VAVGUB)
	NAME(VAVGUH)
	NAME(VAVGUW)
	NAME(VCFSX)
	NAME(VCFUX)
	NAME(VCMPBFP)
	NAME(VCMPEQFP)
	NAME(VCMPEQUB)
	NAME(VCMPEQUH)
	NAME(VCMPEQUW)
	NAME(VCMPGEFP)
	NAME(VCMPGTFP)
	NAME(VCMPGTSB)
	NAME(VCMPGTSH)
	NAME(VCMPGTSW)
	NAME(VCMPGTUB)
	NAME(VCMPGTUH)
	NAME(VCMPGTUW)
	NAME(VCTSXS)
	NAME(VCTUXS)
	NAME(VEXPTEFP)
	NAME(VLOGEFP)
	NAME(VMADDFP)
	NAME(VMAXFP)
	NAME(VMAXSB)
	NAME(VMAXSH)
	NAME(VMAXSW)
	NAME(VMAXUB)
	NAME(VMAXUH)
	NAME(VMAXUW)
	NAME(VMHADDSHS)
	NAME(VMHRADDSHS)
	NAME(VMINFP)
	NAME(VMINSB)
	NAME(VMINSH)
	NAME(VMINSW)
	NAME(VMINUB)
	NAME(VMINUH)
	NAME(VMINUW)
	NAME(VMLADDUHM)
	NAME(VMRGHB)
	NAME(VMRGHH)
	NAME(VMRGHW)
	NAME(VMRGLB)
	NAME(VMRGLH)
	NAME(VMRGLW)
	NAME(VMSUMMBM)
	NAME(VMSUMSHM)
	NAME(VMSUMSHS)
	NAME(VMSUMUBM)
	NAME(VMSUMUHM)
	NAME(VMSUMUHS)
	NAME(VMULESB)
	NAME(VMULESH)
	NAME(VMULEUB)
	NAME(VMULEUH)
	NAME(VMULOSB)
	NAME(VMULOSH)
	NAME(VMULOUB)
	NAME(VMULOUH)
	NAME(VNMSUBFP)
	NAME(VNOR)
	NAME(VOR)
	NAME(VPERM)
	NAME(VPKPX)
	NAME(VPKSHSS)
	NAME(VPKSHUS)
	NAME(VPKSWSS)
	NAME(VPKSWUS)
	NAME(VPKUHUM)
	NAME(VPKUHUS)
	NAME(VPKUWUM)
	NAME(VPKUWUS)
	NAME(VREFP)
	NAME(VRFIM)
	NAME(VRFIN)
	NAME(VRFIP)
	NAME(VRFIZ)
	NAME(VRLB)
	NAME(VRLH)
	NAME(VRLW)
	NAME(VRSQRTEFP)
	NAME(VSEL)
	NAME(VSL)
	NAME(VSLB)
	NAME(VSLDOI)
	NAME(VSLH)
	NAME(VSLO)
	NAME(VSLW)
	NAME(VSPLTB)
	NAME(VSPLTH)
	NAME(VSPLTISB)
	NAME(VSPLTISH)
	NAME(VSPLTISW)
	NAME(VSPLTW)
	NAME(VSR)
	NAME(VSRAB)
	NAME(VSRAH)
	NAME(VSRAW)
	NAME(VSRB)
	NAME(VSRH)
	NAME(VSRO)
	NAME(VSRW)
	NAME(VSUBCUW)
	NAME(VSUBFP)
	NAME(VSUBSBS)
	NAME(VSUBSHS)
	NAME(VSUBSWS)
	NAME(VSUBUBM)
	NAME(VSUBUBS)
	NAME(VSUBUHM)
	NAME(VSUBUHS)
	NAME(VSUBUWM)
	NAME(VSUBUWS)
	NAME(VSUMSWS)
	NAME(VSUM2SWS)
	NAME(VSUM4SBS)
	NAME(VSUM4SHS)
	NAME(VSUM4UBS)
	NAME(VUPKHPX)
	NAME(VUPKHSB)
	NAME(VUPKHSH)
	NAME(VUPKLPX)
	NAME(VUPKLSB)
	NAME(VUPKLSH)
	NAME(VXOR)
	NAME(TDI)
	NAME(TWI)
	NAME(MULLI)
	NAME(SUBFIC)
	NAME(CMPLI)
	NAME(CMPI)
	NAME(ADDIC)
	NAME(ADDI)
	NAME(ADDIS)
	NAME(BC)
	NAME(SC)
	NAME(B)
	NAME(MCRF)
	NAME(BCLR)
	NAME(CRNOR)
	NAME(CRANDC)
	NAME(ISYNC)
	NAME(CRXOR)
	NAME(CRNAND)
	NAME(CRAND)
	NAME(CREQV)
	NAME(CRORC)
	NAME(CROR)
	NAME(BCCTR)
	NAME(RLWIMI)
	NAME(RLWINM)
	NAME(RLWNM)
	NAME(ORI)
	NAME(ORIS)
	NAME(XORI)
	NAME(XORIS)
	NAME(ANDI)
	NAME(ANDIS)
	NAME(RLDICL)
	NAME(RLDICR)
	NAME(RLDIC)
	NAME(RLDIMI)
	NAME(RLDCL)
	NAME(RLDCR)
	NAME(CMP)
	NAME(TW)
	NAME(LVSL)
	NAME(LVEBX)
	NAME(SUBFC)
	NAME(ADDC)
	NAME(MULHDU)
	NAME(MULHWU)
	NAME(MFOCRF)
	NAME(LWARX)
	NAME(LDX)
	NAME(LWZX)
	NAME(SLW)
	NAME(CNTLZW)
	NAME(SLD)
	NAME(AND)
	NAME(CMPL)
	NAME(LVSR)
	NAME(LVEHX)
	NAME(SUBF)
	NAME(LDUX)
	NAME(DCBST)
	NAME(LWZUX)
	NAME(CNTLZD)
	NAME(ANDC)
	NAME(TD)
	NAME(LVEWX)
	NAME(MULHD)
	NAME(MULHW)
	NAME(LDARX)
	NAME(DCBF)
	NAME(LBZX)
	NAME(LVX)
	NAME(NEG)
	NAME(LBZUX)
	NAME(NOR)
	NAME(STVEBX)
	NAME(SUBFE)
	NAME(ADDE)
	NAME(MTOCRF)
	NAME(STDX)
	NAME(STWCX)
	NAME(STWX)
	NAME(STVEHX)
	NAME(STDUX)
	NAME(STWUX)
	NAME(STVEWX)
	NAME(SUBFZE)
	NAME(ADDZE)
	NAME(STDCX)
	NAME(STBX)
	NAME(STVX)
	NAME(SUBFME)
	NAME(MULLD)
	NAME(ADDME)
	NAME(MULLW)
	NAME(DCBTST)
	NAME(STBUX)
	NAME(ADD)
	NAME(DCBT)
	NAME(LHZX)
	NAME(EQV)
	NAME(ECIWX)
	NAME(LHZUX)
	NAME(XOR)
	NAME(MFSPR)
	NAME(LWAX)
	NAME(DST)
	NAME(LHAX)
	NAME(LVXL)
	NAME(MFTB)
	NAME(LWAUX)
	NAME(DSTST)
	NAME(LHAUX)
	NAME(STHX)
	NAME(ORC)
	NAME(ECOWX)
	NAME(STHUX)
	NAME(OR)
	NAME(DIVDU)
	NAME(DIVWU)
	NAME(MTSPR)
	NAME(DCBI)
	NAME(NAND)
	NAME(STVXL)
	NAME(DIVD)
	NAME(DIVW)
	NAME(LVLX)
	NAME(LDBRX)
	NAME(LSWX)
	NAME(LWBRX)
	NAME(LFSX)
	NAME(SRW)
	NAME(SRD)
	NAME(LVRX)
	NAME(LSWI)
	NAME(LFSUX)
	NAME(SYNC)
	NAME(LFDX)
	NAME(LFDUX)
	NAME(STVLX)
	NAME(STDBRX)
	NAME(STSWX)
	NAME(STWBRX)
	NAME(STFSX)
	NAME(STVRX)
	NAME(STFSUX)
	NAME(STSWI)
	NAME(STFDX)
	NAME(STFDUX)
	NAME(LVLXL)
	NAME(LHBRX)
	NAME(SRAW)
	NAME(SRAD)
	NAME(LVRXL)
	NAME(DSS)
	NAME(SRAWI)
	NAME(SRADI)
	NAME(EIEIO)
	NAME(STVLXL)
	NAME(STHBRX)
	NAME(EXTSH)
	NAME(STVRXL)
	NAME(EXTSB)
	NAME(STFIWX)
	NAME(EXTSW)
	NAME(ICBI)
	NAME(DCBZ)
	NAME(LWZ)
	NAME(LWZU)
	NAME(LBZ)
	NAME(LBZU)
	NAME(STW)
	NAME(STWU)
	NAME(STB)
	NAME(STBU)
	NAME(LHZ)
	NAME(LHZU)
	NAME(LHA)
	NAME(LHAU)
	NAME(STH)
	NAME(STHU)
	NAME(LMW)
	NAME(STMW)
	NAME(LFS)
	NAME(LFSU)
	NAME(LFD)
	NAME(LFDU)
	NAME(STFS)
	NAME(STFSU)
	NAME(STFD)
	NAME(STFDU)
	NAME(LD)
	NAME(LDU)
	NAME(LWA)
	NAME(STD)
	NAME(STDU)
	NAME(FDIVS)
	NAME(FSUBS)
	NAME(FADDS)
	NAME(FSQRTS)
	NAME(FRES)
	NAME(FMULS)
	NAME(FMADDS)
	NAME(FMSUBS)
	NAME(FNMSUBS)
	NAME(FNMADDS)
	NAME(MTFSB1)
	NAME(MCRFS)
	NAME(MTFSB0)
	NAME(MTFSFI)
	NAME(MFFS)
	NAME(MTFSF)
	NAME(FCMPU)
	NAME(FRSP)
	NAME(FCTIW)
	NAME(FCTIWZ)
	NAME(FDIV)
	NAME(FSUB)
	NAME(FADD)
	NAME(FSQRT)
	NAME(FSEL)
	NAME(FMUL)
	NAME(FRSQRTE)
	NAME(FMSUB)
	NAME(FMADD)
	NAME(FNMSUB)
	NAME(FNMADD)
	NAME(FCMPO)
	NAME(FNEG)
	NAME(FMR)
	NAME(FNABS)
	NAME(FABS)
	NAME(FCTID)
	NAME(FCTIDZ)
	NAME(FCFID)
	};

#undef NAME

	// Enable address-of operator for ppu_decoder<>
	friend constexpr type operator &(type value)
	{
		return value;
	}
};
