// ----------------------------------------------------------------
//  d3d_model_load.cpp
// lithtech (c) 2000
// ----------------------------------------------------------------

#include "model.h"
//#include "d3dmeshrendobj_skel.h"


// ------------------------------------------------------------------------
// D3D specific object creation routine.
// ------------------------------------------------------------------------
CDIModelDrawable * ModelPiece::CreateModelRenderObject( const char *name, uint32 type )
{
//	if (type == CRenderObject::eSkelMesh)
//	{
//		CDIModelDrawable *p;
//		p = new CD3DSkelMesh();
//		p->m_pName = name;
//		p->m_iObjectType = type;
//		return p;
//	}
		// default or server option
		CDIModelDrawable* p;
		LT_MEM_TRACK_ALLOC(p = new CDIModelDrawable(),LT_MEM_TYPE_MODEL);
		p->m_pName = name;
		p->m_iObjectType = type;
		return p;	// Create a dummy Drawable object (it knows how to load - or, really, skip by a load)...

}
