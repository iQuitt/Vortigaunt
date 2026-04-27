
#include "l_allocator.h"
#include "stdint.h"//#by yangfan

LAlloc g_DefAlloc;



// -------------------------------------------------------------------------------- //
// LAllocCount implementation.
// -------------------------------------------------------------------------------- //

LAllocCount::LAllocCount(LAlloc *pDelegate)
{
	m_pDelegate = pDelegate;
	
	ClearCounts();
}


void LAllocCount::ClearCounts()
{
	m_nTotalAllocations = 0;
	m_nTotalFrees = 0;
	
	m_TotalMemoryAllocated = 0;
	m_nCurrentAllocations = 0;

	m_nAllocationFailures = 0;
}


void* LAllocCount::Alloc(uint32 size, bool bQuadWordAlign)
{
	void *pRet;
	
	if(size == 0)
		return NULL;

	pRet = m_pDelegate->Alloc(size);
	if(pRet)
	{
		m_nTotalAllocations++;
		m_TotalMemoryAllocated += size;
		m_nCurrentAllocations++;
	}
	else
	{
		m_nAllocationFailures++;
	}

	return pRet;
}


void LAllocCount::Free(void *ptr)
{
	if(!ptr)
		return;

	m_pDelegate->Free(ptr);
	m_nCurrentAllocations--;
	m_nTotalFrees++;
}



// -------------------------------------------------------------------------------- //
// LAllocSimpleBlock implementation.
// -------------------------------------------------------------------------------- //

LAllocSimpleBlock::LAllocSimpleBlock()
{
	Clear();
}


LAllocSimpleBlock::~LAllocSimpleBlock()
{
	Term();
}


LTBOOL LAllocSimpleBlock::Init(LAlloc *pDelegate, uint32 blockSize)
{
	Term();

	blockSize = (blockSize + 3) & ~3;

	if(blockSize > 0)
	{
		m_pBlock = (uint8*)pDelegate->Alloc(blockSize);
		if(!m_pBlock)
			return FALSE;
	}

	m_pDelegate = pDelegate;
	m_BlockSize = blockSize;
	m_CurBlockPos = 0;

	return TRUE;
}


void LAllocSimpleBlock::Term()
{
	if(m_pDelegate)
	{
		m_pDelegate->Free(m_pBlock);
	}

	Clear();
}


void* LAllocSimpleBlock::Alloc(uint32 size, bool bQuadWordAlign)
{
	uint8 *pRet;

	if(size == 0)
		return NULL;

	if (bQuadWordAlign) {	// QuadWord Align (We've over alloced a bit to account for this - if we're not QWAligned, force it to be)...
		pRet = &m_pBlock[m_CurBlockPos];
		//m_CurBlockPos += (((uint32)pRet + 0xf) & ~0xf) - (uint32)pRet; }
		/* by yangfan
		这行代码的目的是将指针 pRet 所指向的地址进行16字节对齐操作。
		具体来说，代码使用了位运算 (((uint32)pRet + 0xf) & ~0xf) 来将地址向上舍入到最接近的 16 的倍数。
		0xf 是 16 进制表示的十进制数 15，~0xf 是对 0xf 取反，即将低四位设置为 0。
        使用uintptr_t 类型来代替 uint32，它是一个无符号整数类型，足够大以容纳指针。
        用于避免出现Cast from pointer to smaller type 'uint32' (aka 'unsigned int') loses information的报错
		*/
        m_CurBlockPos += (((uintptr_t)pRet + 0xf) & ~0xf) - (uintptr_t)pRet; }
	else {					// DWord Align by default...
		size = ((size + 3) & ~3); }

	if((m_CurBlockPos + size) > m_BlockSize)
		return NULL;

	pRet = &m_pBlock[m_CurBlockPos];
	m_CurBlockPos += size;
	//assert(!bQuadWordAlign || ((uint32)pRet & 0xf) == 0);
    assert(!bQuadWordAlign || ((uintptr_t)pRet & 0xf) == 0);
	return pRet;
}


void LAllocSimpleBlock::Free(void *ptr)
{
}


void LAllocSimpleBlock::Clear()
{
	m_pDelegate = NULL;
	m_pBlock = NULL;
	m_CurBlockPos = 0;
	m_BlockSize = 0;
}

void* DefStdlithAlloc(uint32 size)
{
	if (size == 0)
		return NULL;

	return malloc((size_t)size);
}

void DefStdlithFree(void* ptr)
{
	if (ptr)
	{
		free(ptr);
	}
}
