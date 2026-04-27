// D3D Utility Functions...

#ifndef __D3D_UTILS_H__
#define __D3D_UTILS_H__

// D3DCOLOR is equivalent to D3DFMT_A8R8G8B8
#ifndef D3DCOLOR_DEFINED
typedef DWORD D3DCOLOR;
#define D3DCOLOR_DEFINED
#endif

// Flexible vertex format bits
//
#define D3DFVF_RESERVED0        0x001
#define D3DFVF_POSITION_MASK    0x400E
#define D3DFVF_XYZ              0x002
#define D3DFVF_XYZRHW           0x004
#define D3DFVF_XYZB1            0x006
#define D3DFVF_XYZB2            0x008
#define D3DFVF_XYZB3            0x00a
#define D3DFVF_XYZB4            0x00c
#define D3DFVF_XYZB5            0x00e
#define D3DFVF_XYZW             0x4002

#define D3DFVF_NORMAL           0x010
#define D3DFVF_PSIZE            0x020
#define D3DFVF_DIFFUSE          0x040
#define D3DFVF_SPECULAR         0x080

#define D3DFVF_TEXCOUNT_MASK    0xf00
#define D3DFVF_TEXCOUNT_SHIFT   8
#define D3DFVF_TEX0             0x000
#define D3DFVF_TEX1             0x100
#define D3DFVF_TEX2             0x200
#define D3DFVF_TEX3             0x300
#define D3DFVF_TEX4             0x400
#define D3DFVF_TEX5             0x500
#define D3DFVF_TEX6             0x600
#define D3DFVF_TEX7             0x700
#define D3DFVF_TEX8             0x800

#define D3DFVF_LASTBETA_UBYTE4   0x1000
#define D3DFVF_LASTBETA_D3DCOLOR 0x8000

#define D3DFVF_RESERVED2         0x6000  // 2 reserved bits

// ENUMs
enum VERTEX_BLEND_TYPE				{ eNO_WORLD_BLENDS, eNONINDEXED_B1, eNONINDEXED_B2, eNONINDEXED_B3, eINDEXED_B1, eINDEXED_B2, eINDEXED_B3 };

// VERTEX DATA TYPE FLAGS (Note: Should match those in the LTB header)...
#define	VERTDATATYPE_POSITION					0x0001
#define	VERTDATATYPE_NORMAL						0x0002
#define	VERTDATATYPE_DIFFUSE					0x0004
#define	VERTDATATYPE_PSIZE						0x0008
#define	VERTDATATYPE_UVSETS_1					0x0010
#define	VERTDATATYPE_UVSETS_2					0x0020
#define	VERTDATATYPE_UVSETS_3					0x0040
#define	VERTDATATYPE_UVSETS_4					0x0080
#define	VERTDATATYPE_BASISVECTORS				0x0100

// BASIC VERTEX TYPES...
#define BASIC_VERTEX_FLAGS					(D3DFVF_XYZ | D3DFVF_DIFFUSE)
struct  BASIC_VERTEX						{ float x; float y; float z; D3DCOLOR color; };
#define BASIC_VERTEX_UV1_TRANSFORMED_FLAGS	(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
struct  BASIC_VERTEX_UV1_TRANSFORMED		{ float x; float y; float z; float rhw; D3DCOLOR color; float u; float v; };

// VERTEX FLAGS/STRUCTS...
#define VSTREAM_XYZ_NORMAL_FLAGS	(D3DFVF_XYZ | D3DFVF_NORMAL)	// FVF Vertex Structures...
struct  VSTREAM_XYZ_NORMAL			{ float x; float y; float z; float nx; float ny; float nz; };
#define VSTREAM_UV1_FLAGS			(D3DFVF_TEX1)
struct  VSTREAM_UV1					{ float u; float v; };
#define VSTREAM_UV2_FLAGS			(D3DFVF_TEX2)
struct  VSTREAM_UV2					{ float u1; float v1; float u2; float v2; };
#define VSTREAM_UV3_FLAGS			(D3DFVF_TEX3)
struct  VSTREAM_UV3					{ float u1; float v1; float u2; float v2; float u3; float v3; };
#define VSTREAM_UV4_FLAGS			(D3DFVF_TEX4)
struct  VSTREAM_UV4					{ float u1; float v1; float u2; float v2; float u3; float v3; float u4; float v4; };

#define VSTREAM_XYZ_NORMAL_B1_FLAGS	(D3DFVF_XYZB1 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B1		{ float x; float y; float z; float blend1; float nx; float ny; float nz; };
#define VSTREAM_XYZ_NORMAL_B2_FLAGS	(D3DFVF_XYZB2 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B2		{ float x; float y; float z; float blend1; float blend2; float nx; float ny; float nz; };
#define VSTREAM_XYZ_NORMAL_B3_FLAGS	(D3DFVF_XYZB3 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B3		{ float x; float y; float z; float blend1; float blend2; float blend3; float nx; float ny; float nz; };
#define VSTREAM_XYZ_NORMAL_B1_INDEX_FLAGS	(D3DFVF_XYZB2 | D3DFVF_LASTBETA_UBYTE4 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B1_INDEX { float x; float y; float z; float blend1; uint8 Index[4]; float nx; float ny; float nz; };
#define VSTREAM_XYZ_NORMAL_B2_INDEX_FLAGS	(D3DFVF_XYZB3 | D3DFVF_LASTBETA_UBYTE4 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B2_INDEX	{ float x; float y; float z; float blend1; float blend2; uint8 Index[4]; float nx; float ny; float nz; };
#define VSTREAM_XYZ_NORMAL_B3_INDEX_FLAGS	(D3DFVF_XYZB4 | D3DFVF_LASTBETA_UBYTE4 | D3DFVF_NORMAL)
struct  VSTREAM_XYZ_NORMAL_B3_INDEX	{ float x; float y; float z; float blend1; float blend2; float blend3; uint8 Index[4]; float nx; float ny; float nz; };

// INLINE FUNCTIONS...
inline uint32	d3d_GetVertexSize(uint32 iVertexFormat)
{
	uint32 iVertSize = 0;
	if ((iVertexFormat & D3DFVF_XYZB4)		 == D3DFVF_XYZB4)	iVertSize += sizeof(float) * 7;
	else if ((iVertexFormat & D3DFVF_XYZB3)	 == D3DFVF_XYZB3)	iVertSize += sizeof(float) * 6;
	else if ((iVertexFormat & D3DFVF_XYZB2)  == D3DFVF_XYZB2)	iVertSize += sizeof(float) * 5;
	else if ((iVertexFormat & D3DFVF_XYZB1)	 == D3DFVF_XYZB1)	iVertSize += sizeof(float) * 4;
	else if ((iVertexFormat & D3DFVF_XYZRHW) == D3DFVF_XYZRHW)	iVertSize += sizeof(float) * 4;
	else if ((iVertexFormat & D3DFVF_XYZ)	 == D3DFVF_XYZ)		iVertSize += sizeof(float) * 3;
	if (iVertexFormat & D3DFVF_NORMAL)							iVertSize += sizeof(float) * 3;
	if (iVertexFormat & D3DFVF_DIFFUSE)							iVertSize += sizeof(uint32);
	if (iVertexFormat & D3DFVF_SPECULAR)						iVertSize += sizeof(uint32);
	if ((iVertexFormat & D3DFVF_TEX4) == D3DFVF_TEX4)			iVertSize += sizeof(float) * 8;
	else if ((iVertexFormat & D3DFVF_TEX3) == D3DFVF_TEX3)		iVertSize += sizeof(float) * 6;
	else if ((iVertexFormat & D3DFVF_TEX2) == D3DFVF_TEX2)		iVertSize += sizeof(float) * 4;
	else if ((iVertexFormat & D3DFVF_TEX1) == D3DFVF_TEX1)		iVertSize += sizeof(float) * 2;
	return iVertSize;
}

inline void GetVertexFlags_and_Size(VERTEX_BLEND_TYPE VertBlendType, uint32 iVertDataType, uint32& iVertFlags, uint32& iVertSize, uint32& iUVSets, bool& bNonFixPipeData)
{
	// Figure out our usage and vert type...
	uint32 iExtraData				= 0;
	uint32 iBoneCount				= 0;
	bNonFixPipeData					= false;
	if ((iVertDataType & VERTDATATYPE_POSITION) && (iVertDataType & VERTDATATYPE_NORMAL))
	{
		switch (VertBlendType)
		{
		case eNO_WORLD_BLENDS		: iVertFlags = VSTREAM_XYZ_NORMAL_FLAGS; break;
		case eNONINDEXED_B1			: iVertFlags = VSTREAM_XYZ_NORMAL_B1_FLAGS; break;
		case eNONINDEXED_B2			: iVertFlags = VSTREAM_XYZ_NORMAL_B2_FLAGS; break;
		case eNONINDEXED_B3			: iVertFlags = VSTREAM_XYZ_NORMAL_B3_FLAGS; break;
		case eINDEXED_B1			: iVertFlags = VSTREAM_XYZ_NORMAL_B1_INDEX_FLAGS; iBoneCount = 2; break;
		case eINDEXED_B2			: iVertFlags = VSTREAM_XYZ_NORMAL_B2_INDEX_FLAGS; iBoneCount = 3; break;
		case eINDEXED_B3			: iVertFlags = VSTREAM_XYZ_NORMAL_B3_INDEX_FLAGS; iBoneCount = 4; break;
		default						: assert(0); break;
		}
	}
	if (iVertDataType & VERTDATATYPE_UVSETS_1)		{ iVertFlags |= VSTREAM_UV1_FLAGS; iUVSets = 1; }
	else if (iVertDataType & VERTDATATYPE_UVSETS_2) { iVertFlags |= VSTREAM_UV2_FLAGS; iUVSets = 2; }
	else if (iVertDataType & VERTDATATYPE_UVSETS_3) { iVertFlags |= VSTREAM_UV3_FLAGS; iUVSets = 3; }
	else if (iVertDataType & VERTDATATYPE_UVSETS_4) { iVertFlags |= VSTREAM_UV4_FLAGS; iUVSets = 4; }
	if (iVertDataType & VERTDATATYPE_BASISVECTORS)
	{
		iExtraData					+= sizeof(float)*6;
		bNonFixPipeData				= true;
	}

	// Create m_pVB_Verts...
	if (!bNonFixPipeData)
	{
		// It's a typical FVF buffer that can go down the standard pipe...
		iVertSize					= d3d_GetVertexSize(iVertFlags);
	}
	else
	{
		// It's a vertex shader only type...
		iVertSize					= d3d_GetVertexSize(iVertFlags) + iExtraData;
		iVertFlags					= NULL;
	}
}

#endif