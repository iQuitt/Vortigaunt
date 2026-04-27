
// This file defines old things for compatibility.
// Don't use anything in here cuz it's on its way out.

#ifndef __LT_COMPAT_H__
#define __LT_COMPAT_H__


	// Old types
	#define DStream			ILTStream

	
	// Old constants
	#define DDWORD	uint32
	
	// Old defines
	#define DMIN	LTMIN
	#define DMAX	LTMAX
	#define DCLAMP	LTCLAMP
	
	#define DTOCVEC(_vec) 
	#define CTODVEC(_vec) 
	
	// Old result codes
	#define DRESULT							LTRESULT
	#define DE_OK							LT_OK
	
	// 
	// Old macros, these shouldn't be used in new code. Their
	// respective classes have functions to do the same thing.
	// 

	#define PLANE_COPY(dest, src) ((dest) = (src))

	
	// Get the distance from a point to a plane.
	#define DIST_TO_PLANE(vec, plane) ( VEC_DOT((plane).m_Normal, (vec)) - (plane).m_Dist )

#endif  // __LT_COMPAT_H__
