// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <stdio.h>
#include <memory.h>
#include <math.h>

// Uncomment to get running commentary on the progress...
// #define VERBOSE_OUTPUT

// Useful function.
int FindInList ( int ItemToFind, int ListLength, int const *List )
{
    for ( int i = 0; i < ListLength; i++ )
    {
        if ( List[i] == ItemToFind )
        {
            return i;
        }
    }
    return -1;
}

// Vertex-cache optimization.

// Uses a very simple greedy algorithm, but with a fairly complex
// heuristic for vertex scoring. Although lookahead methods
// can get very slightly better results, they are orders of
// magnitude slower. The method used here is very fast, and gets very
// acceptable results. From the numbers below, it seems like a perfect
// ordering (which I believe is an NP-complete problem!) would be
// substantially less than 10% faster.

// The algorithm uses a fairly large LRU vertex cache as its heart.
// Almost all hardware caches are in fact FIFOs, but unless you know
// exactly how long the target FIFO is, you get wrong answers!
// So if you can only pick one cache, use a conventional LRU,
// and the same vertex ordering works far more robustly well with FIFOs.

// 32 entries is more than any known hardware, and well into diminishing returns territory.
// (adding more entries does not seem to help in any way, it's just slower)
const int MaxSizeVertexCache = 32;

// Each vertex has a score - higher the score, the more likely we are to add
// triangles that use that vertex.
// Vertex score is scaled by distance "down" the LRU list, which also
// accounts for different FIFO lengths.
// Score at head of LRU cache is 1.0, score near tail is 0.0
// Vertex score is boosted by having a low number of tris still using it.
// ("getting rid" of tris/verts is important - you don't want to have to come
// back and draw a single tri to clean up "holes" at the end of a
// nicely-optimised mesh)
//
//  Remember a current best triangle - starts with "none"
//  While there are triangles to add:
//      If there is no best triangle, or it's not a very good triangle:
//          Scan all tris to see which has the best sum of vertex scores
//      Add tri with best score to drawing list.
//      Add missing verts the tri uses to LRU cache.
//      Update cached vertex scores in LRU (and any that fell off the tail).
//      Update the scores of tris using any of those vertices.
//          Remember the tri with the highest score as you do.

// Normally, the only tris we'd want to consider for the next tri
// are ones related to vertices that we've just added. This will give those
// tris high scores, and because we've just changed their vertices, we will
// have calculated the tri scores & found the highest one. Anyway, sometimes
// the algo runs into a corner, and there's no nearby tris to add, or the only
// tris it knows about are very low scorers.
// That's when we need to re-check the entire mesh and find the lowest score of
// all the tris available, not just the ones we've affected recently,
// because there may be higher-scoring tris elsewhere.
// This is the cutoff for that process. Tweak as desired for speed vs accuracy.
// It actually can be very low without affecting the quality of the caching.
const float Vcache_MinimumBestTriScore = 0.1f;
//const float Vcache_MinimumBestTriScore = 100.0f;          // Good for aggressive debugging!

struct vcache_vertex_data
{
    int CacheTag;       // -1 = not in cache.
    float CurrentScore;

    int NumTris;        // The size of TriIndices
    int NumActiveTris;  // The first n of TriIndices are the still-active (to be drawn) ones.
    int *TriIndices;
};

struct vcache_cache_tag
{
    int VertexNumber;
};

struct vcache_tri_data
{
    bool IsActive;
    float Score;
    int VertexNumber[3];
};



// Globals that control FindVertexScore.
// Feel free to tweak these as desired, but be warned that it
// is not a very obvious process. These numbers were found by
// a combination of exhaustive tests and iteration, not by
// anything resembling intuition!
const float FindVertexScore_CacheDecayPower = 1.5f;
const float FindVertexScore_LastTriScore = 0.75f;
const float FindVertexScore_ValenceBoostScale = 2.0f;
const float FindVertexScore_ValenceBoostPower = 0.5f;

// Here are some typical vertex-fetch/triangle values for different sized FIFO caches for two very different meshes.
// Performance for 4-vertex caches (which are basically equivalent to old non-indexed tristrip hardware)
// was intentionally sacrificed - it seems almost impossible to get good results for 4-vertex
// caches and anything larger. If you are targeting non-indexed tristrip hardware, you really need to
// look at using a specialist tristripper, as they produce very different patterns to those of more
// generic vertex caches.
// However, 8-vertex caches and 32-vertex caches (and those inbetween) seem to be easily compatible.
// These numbers are tuned for good average results for 8, 16, 24 and 32-vertex cache sizes,
// and although some settings can slightly improve scores in any one of these (typically by no more than 10%),
// they result in larger performance losses in the others. Therefore, these settings are chosen
// for their best overall performance if you do not know what size of cache you will be dealing with,
// which is the common case on PC/Mac hardware, and on some consoles as well (because it depends
// on how large your vertex formats are and so on).
// The 1024-vertex-cache numbers are presented as being near the maximum possible. For a mesh
// with fewer than 1024 vertices, it IS the maximum possible of course!

//const float FindVertexScore_CacheDecayPower = 1.5f;
//const float FindVertexScore_LastTriScore = 0.75f;
//const float FindVertexScore_ValenceBoostScale = 2.0f;
//const float FindVertexScore_ValenceBoostPower = 0.5f;
// A mesh with a variety of sections, all less than 1000 tris in size (the Granny mesh from GrannyRocks.gr2, split into 30-bone sections):
// 4: 1.556,  8: 0.908, 12: 0.820, 16: 0.797, 20: 0.781, 24: 0.776, 28: 0.771, 32: 0.765, 1024: 0.732
// A mesh with chunks ranging from 500 tris to 1500 tris (rayback_longwalk.gr2, split into 30-bone sections):
// 4: 1.559,  8: 0.893, 12: 0.791, 16: 0.769, 20: 0.755, 24: 0.747, 28: 0.738, 32: 0.730, 1024: 0.695
// The same mesh, in one 5500-tri chunk (rayback_longwalk.gr2, no splitting):
// 4: 1.596,  8: 0.874, 12: 0.756, 16: 0.727, 20: 0.713, 24: 0.703, 28: 0.691, 32: 0.682, 1024: 0.647
// A large, evenly tessellated mesh (a 32-segment geosphere from 3DSMax):
// 4: 1.785,  8: 0.888, 12: 0.740, 16: 0.715, 20: 0.702, 24: 0.691, 28: 0.681, 32: 0.672, 1024: 0.529

// Visual inspection also shows the geosphere gets triangle patterns very
// like a Hilbert Curve appearing - curves curling back on itself and suchlike,
// which theory says is the most efficient arrangement for a regular grid (which the
// geosphere almost is) when you have an unknown cache size.

// Note that the LastTriScore is actually lower than 1.0, so
// it actively tries not to use verts from the last tri. This
// seems counterintuitive, but it prevents it from simply making long
// thin strips. The only other way to prevent it from making strips
// is to start using lookahead algorithms rather than pure greedy ones,
// but that starts getting very slow!


float FindVertexScore ( vcache_vertex_data *VertexData )
{
    if ( VertexData->NumActiveTris == 0 )
    {
        // No tri needs this vertex!
        return -1.0f;
    }

    float Score = 0.0f;
    int CachePosition = VertexData->CacheTag;
    if ( CachePosition >= 0 )
    {
        if ( CachePosition < 3 )
        {
            // This vertex was used in the last triangle, so it has a fixed score, whichever of the three it's in.
            // Otherwise, you can get very different answers depending on whether you add
            // the triangle 1,2,3 or 3,1,2 - which is silly.
            Score = FindVertexScore_LastTriScore;
        }
        else
        {
            pp_assert ( CachePosition < MaxSizeVertexCache );
            // Points for being high in the cache.
            Score = 1.0f - ( (float)( CachePosition - 3 ) / (float)( MaxSizeVertexCache - 3 ) );
            Score = powf ( Score, FindVertexScore_CacheDecayPower );
        }
    }
    // Bonus points for having a low number of tris still to use the vert,
    // so we get rid of lone verts quickly.
    Score += FindVertexScore_ValenceBoostScale * powf ( (float)VertexData->NumActiveTris, -FindVertexScore_ValenceBoostPower );

    return Score;
}


void VcacheOptimize ( int NumTris, int const *IndexedTriListSource, int *IndexedTriListDst )
{
#ifdef VERBOSE_OUTPUT
    printf( "Optimising %i vertices.", NumTris );
#endif

    int NumVerts = 0;
    for ( int i = 0; i < NumTris * 3; i++ )
    {
        int VertNum = IndexedTriListSource[i];
        pp_assert ( VertNum >= 0 );
        if ( VertNum > NumVerts )
        {
            NumVerts = VertNum;
        }
    }
    NumVerts++;


    vcache_vertex_data *VertexData = new vcache_vertex_data[NumVerts];
    vcache_tri_data *TriData = new vcache_tri_data[NumTris];

    // Allocate three extras so that I can just shuffle stuff down, then do an update.
    vcache_cache_tag CacheTags[MaxSizeVertexCache+3];
    for ( int i = 0; i < MaxSizeVertexCache+3; i++ )
    {
        // Empty cache.
        CacheTags[i].VertexNumber = -1;
    }


    for ( int VertNum = 0; VertNum < NumVerts; VertNum++ )
    {
        VertexData[VertNum].NumTris = 0;
    }

    int const *IndexedTriListSrcTemp = IndexedTriListSource;
    vcache_tri_data *CurTri = TriData;
    for ( int i = 0; i < NumTris; i++ )
    {
        for ( int j = 0; j < 3; j++ )
        {
            int VertNum = *IndexedTriListSrcTemp;
            CurTri->VertexNumber[j] = VertNum;
            VertexData[VertNum].NumTris++;
            IndexedTriListSrcTemp++;
        }
        CurTri->IsActive = true;
        CurTri->Score = -1.0f;
        CurTri++;
    }

    for ( int VertNum = 0; VertNum < NumVerts; VertNum++ )
    {
        VertexData[VertNum].TriIndices = new int[VertexData[VertNum].NumTris];
        VertexData[VertNum].CacheTag = -1;
        VertexData[VertNum].CurrentScore = -1.0f;
        VertexData[VertNum].NumActiveTris = VertexData[VertNum].NumTris;
        // And reset to zero so we can start adding the indices again in the next loop.
        VertexData[VertNum].NumTris = 0;
    }

    CurTri = TriData;
    for ( int i = 0; i < NumTris; i++ )
    {
        for ( int j = 0; j < 3; j++ )
        {
            int VertNum = CurTri->VertexNumber[j];
            VertexData[VertNum].TriIndices[VertexData[VertNum].NumTris] = i;
            VertexData[VertNum].NumTris++;
        }
        CurTri++;
    }

    // Setup finished.


    // Now find the initial scores for the tris and verts.
    // This will be kept up to date as we go along.
    int MaxNumTrisPerVerts = 0;
    vcache_vertex_data *CurVert = VertexData;
    for ( int i = 0; i < NumVerts; i++ )
    {
        pp_assert ( CurVert->NumActiveTris == CurVert->NumTris );
        if ( MaxNumTrisPerVerts < CurVert->NumTris )
        {
            MaxNumTrisPerVerts = CurVert->NumTris;
        }
        CurVert->CurrentScore = FindVertexScore ( CurVert );
        CurVert++;
    }

    CurTri = TriData;
    for ( int i = 0; i < NumTris; i++ )
    {
        CurTri->IsActive = true;
        CurTri->Score = 0.0f;
        for ( int j = 0; j < 3; j++ )
        {
            int VertNum = CurTri->VertexNumber[j];
            CurTri->Score += VertexData[VertNum].CurrentScore;
        }
        CurTri++;
    }


    // A list of tris whose vertices probably changed scores.
    // The maximum is all the vertices in the cache, plus the three of
    // the tri that just got added (which may all be able to be added to the cache,
    // kicking out ones currently in the cache), multiplied by the
    // maximum number of tris per vertex.
    int MaxNumTrisNumsToRetest = (MaxSizeVertexCache + 3) * MaxNumTrisPerVerts;
    int NumTrisToRetest = 0;
    int *TriNumsToRetest = new int[MaxNumTrisNumsToRetest];



    int BestTriNum = -1;
    float BestTriScore = -1.0f;
    for ( int DstTriNum = 0; DstTriNum < NumTris; DstTriNum++ )
    {

#ifdef VERBOSE_OUTPUT
        // Progress counter for large meshes.
        if ( ( DstTriNum & 0xfff ) == 0 )
        {
            printf ( "%c...%i", (( DstTriNum & 0xffff ) == 0)? '\n' : ' ', DstTriNum );
        }
#endif


        // If we don't already have a best score, or the best
        // score is suspiciously low, scan all the tris to find the best one.
        if ( ( BestTriNum < 0 ) || ( BestTriScore < Vcache_MinimumBestTriScore ) )
        {
#ifdef _DEBUG
            float BestOriginalScore = BestTriScore;

            if ( ( BestTriScore > 0.0f ) && ( BestTriScore < Vcache_MinimumBestTriScore ) )
            {
                // Convenient place for a breakpoint.
                BestTriScore = BestTriScore;
            }

            // Check the cached scores are right.
            for ( int i = 0; i < NumVerts; i++ )
            {
                vcache_vertex_data *CurVert = &(VertexData[i]);
                if ( CurVert->CacheTag >= 0 )
                {
                    pp_assert ( CurVert->CacheTag < MaxSizeVertexCache );
                    pp_assert ( CacheTags[CurVert->CacheTag].VertexNumber == i );
                }
                float VertScore = FindVertexScore ( CurVert );
                pp_assert ( CurVert->CurrentScore == VertScore );
            }
#endif

            vcache_tri_data *CurTri = TriData;
            BestTriNum = -1;
            BestTriScore = -1.0f;
            for ( int CurTriNum = 0; CurTriNum < NumTris; CurTriNum++ )
            {
                if ( CurTri->IsActive )
                {
#ifdef _DEBUG
                    // Check all the cached scores are correct.
                    float Score = 0.0f;
                    for ( int j = 0; j < 3; j++ )
                    {
                        int VertNum = CurTri->VertexNumber[j];
                        float VertScore = VertexData[VertNum].CurrentScore;
                        pp_assert ( VertScore >= 0.0f );
                        Score += VertScore;
                    }
                    pp_assert ( Score >= 0.0f );
                    pp_assert ( CurTri->Score == Score );
#endif
                    if ( CurTri->Score > BestTriScore )
                    {
                        BestTriScore = CurTri->Score;
                        BestTriNum = CurTriNum;
                    }
                }
                else
                {
                    pp_assert ( CurTri->Score == -1.0f );
                }
                CurTri++;
            }

#ifdef _DEBUG
            if ( ( BestOriginalScore > 0.0f ) && ( BestOriginalScore + 0.01f < BestTriScore ) )
            {
                // Interesting to breakpoint this for debugging.
                // Turns out, this happens so rarely that it's probably not worth
                // the effort of doing exhaustive tests. So you can keep Vcache_MinimumBestTriScore
                // set to a very low level.
                BestOriginalScore = BestOriginalScore;
            }
#endif
        }


        // Add this tri.
        pp_assert ( BestTriNum >= 0 );
        pp_assert ( BestTriNum < NumTris );
        vcache_tri_data *BestTri = &(TriData[BestTriNum]);
        pp_assert ( BestTri->IsActive );
        pp_assert ( BestTri->Score == BestTriScore );
        *IndexedTriListDst++ = BestTri->VertexNumber[0];
        *IndexedTriListDst++ = BestTri->VertexNumber[1];
        *IndexedTriListDst++ = BestTri->VertexNumber[2];

        BestTri->IsActive = false;
        BestTri->Score = -1.0f;

        NumTrisToRetest = 0;


        // Remove this tri from each vert's active list.
        for ( int VertIndex = 0; VertIndex < 3; VertIndex++ )
        {
            int VertNum = BestTri->VertexNumber[VertIndex];
            vcache_vertex_data *CurVert = &(VertexData[VertNum]);
            pp_assert ( CurVert->CurrentScore >= 0.0f );
            pp_assert ( CurVert->NumActiveTris > 0 );
            // Where is this tri in the list?
            int PositionInList = FindInList ( BestTriNum, CurVert->NumActiveTris, CurVert->TriIndices );
            pp_assert ( PositionInList >= 0 );

            // Move this tri to the bottom of the list and swap with the previous bottom.
            CurVert->NumActiveTris--;
            pp_assert ( CurVert->TriIndices[PositionInList] == BestTriNum );
            CurVert->TriIndices[PositionInList] = CurVert->TriIndices[CurVert->NumActiveTris];
            CurVert->TriIndices[CurVert->NumActiveTris] = BestTriNum;
        }

        // Now update/add the three vertices in the vertex cache, and shuffle all others down (if needed).
        // This value increases as we shuffle things down.
        int NumVertsInCache = MaxSizeVertexCache;

        for ( int VertIndex = 0; VertIndex < 3; VertIndex++ )
        {
            int VertNum = BestTri->VertexNumber[VertIndex];
            vcache_vertex_data *CurVert = &(VertexData[VertNum]);

            int LastToShiftDown;
            int CacheTagNum = CurVert->CacheTag;
            if ( CacheTagNum >= 0 )
            {
                // Already in the cache, so just move it to the top and shuffle everything else down to where it is.
                LastToShiftDown = CacheTagNum;
                pp_assert ( CacheTags[CacheTagNum].VertexNumber == VertNum );
            }
            else
            {
                // Not already in the cache, so add it and shift existing cache contents down, without dropping the last one.
                LastToShiftDown = NumVertsInCache;
                NumVertsInCache++;
            }

            for ( int i = LastToShiftDown; i > 0; i-- )
            {
                int TempVertNum = CacheTags[i-1].VertexNumber;
                CacheTags[i] = CacheTags[i-1];
                if ( TempVertNum >= 0 )
                {
                    pp_assert ( VertexData[TempVertNum].CacheTag == ( i - 1 ) );
                    VertexData[TempVertNum].CacheTag = i;
                }
            }
            // Add this vertex at the top.
            CacheTags[0].VertexNumber = VertNum;
            CurVert->CacheTag = 0;
        }


        // Re-find all scores for all verts in the cache, including any that might have just been pushed out.
        pp_assert ( NumVertsInCache <= MaxSizeVertexCache + 3 );
        for ( int VertIndex = 0; VertIndex < NumVertsInCache; VertIndex++ )
        {
            int VertNum = CacheTags[VertIndex].VertexNumber;
            if ( VertNum >= 0 )
            {
                vcache_vertex_data *CurVert = &(VertexData[VertNum]);
                pp_assert ( CurVert->CacheTag == VertIndex );
                if ( VertIndex >= MaxSizeVertexCache )
                {
                    // This just fell out of the cache.
                    CurVert->CacheTag = -1;
                }

                CurVert->CurrentScore = FindVertexScore ( CurVert );
                // And make sure we update all the tris in the list
                for ( int i = 0; i < CurVert->NumActiveTris; i++ )
                {
                    TriNumsToRetest[NumTrisToRetest++] = CurVert->TriIndices[i];
                    pp_assert ( NumTrisToRetest <= MaxNumTrisNumsToRetest );
                }
            }
        }

        // And now recalculate the affected tri scores.
        // Along the way, find the tri with the highest score.
        BestTriNum = -1;
        BestTriScore = -1.0f;
        for ( int TriListNum = 0; TriListNum < NumTrisToRetest; TriListNum++ )
        {
            int TriNum = TriNumsToRetest[TriListNum];
            vcache_tri_data *CurTri = &(TriData[TriNum]);
            pp_assert ( CurTri->IsActive );
            float Score = 0.0f;
            for ( int j = 0; j < 3; j++ )
            {
                int VertNum = CurTri->VertexNumber[j];
                pp_assert ( VertexData[VertNum].CurrentScore >= 0.0f );
                Score += VertexData[VertNum].CurrentScore;
            }
            CurTri->Score = Score;
            if ( BestTriScore < Score )
            {
                BestTriScore = Score;
                BestTriNum = TriNum;
            }
        }


    }


    for ( int VertNum = 0; VertNum < NumVerts; VertNum++ )
    {
        pp_assert ( VertexData[VertNum].NumActiveTris == 0 );
        delete[] VertexData[VertNum].TriIndices;
    }

    delete[] TriNumsToRetest;
    delete[] VertexData;
    delete[] TriData;

#ifdef VERBOSE_OUTPUT
    printf ( " done.\n" );
#endif
}


void ApplyAnnotationPermutation(granny_vertex_annotation_set& Annotation,
                                granny_int32x const* Permutation,
                                granny_int32x const  ActualCount,
                                granny_memory_arena* Arena)
{
    if (Annotation.IndicesMapFromVertexToAnnotation)
    {
        granny_int32* NewIndices = PushArray(Arena, ActualCount, granny_int32);

        // Easy, just permute the indices
        for (granny_int32 Idx = 0; Idx < Annotation.VertexAnnotationIndexCount; ++Idx)
        {
            if (Permutation[Idx] == -1)
            {
                // Vert doesn't exist anymore
                continue;
            }

            NewIndices[Permutation[Idx]] = Annotation.VertexAnnotationIndices[Idx];
        }

        Annotation.VertexAnnotationIndexCount = ActualCount;
        Annotation.VertexAnnotationIndices = NewIndices;
    }
    else if (Annotation.VertexAnnotationCount != 0)
    {
        pp_assert(Annotation.VertexAnnotationType);
        pp_assert(Annotation.VertexAnnotationCount <= ActualCount);

        // Direct mapped.  Permute the structures.  Technically, we could just introduce a
        // mapping that was identical to Permutation here, but we'd like to not require
        // changes to client code.  If the file works with direct mapping, and that's all
        // that is implemented in the game, then we don't want to require a branch for
        // indexed annotation.
        granny_int32x const ObjectSize =
            GrannyGetTotalObjectSize(Annotation.VertexAnnotationType);

        granny_uint8* Buffer = PushArray(Arena, ObjectSize * ActualCount, granny_uint8);
        {for(granny_int32x Vert = 0; Vert < Annotation.VertexAnnotationCount; ++Vert)
        {
            if (Permutation[Vert] != -1)
            {
                granny_uint8* Dest = Buffer + (Permutation[Vert] * ObjectSize);
                granny_uint8* Src  = Annotation.VertexAnnotations + (Vert * ObjectSize);

                memcpy(Dest, Src, ObjectSize);
            }
        }}

        Annotation.VertexAnnotationCount = ActualCount;
        Annotation.VertexAnnotations = Buffer;
    }
}

void ApplyPermutation(granny_int32x const* Permutation,
                      granny_vertex_data*  VertexData,
                      granny_memory_arena* Arena)
{
    // Just do the permutation to a temp buffer, it's easier
    granny_int32x const VertexSize = GrannyGetTotalObjectSize(VertexData->VertexType);

    granny_uint8* Temp = new granny_uint8[VertexSize * VertexData->VertexCount];
    granny_int32x ActuallyUsed = 0;
    {for(granny_int32x Vert = 0; Vert < VertexData->VertexCount; ++Vert)
    {
        if (Permutation[Vert] == -1)
        {
            // gaps in the mesh?  That should really never happen.  We handle this
            // by adjusting the vertex count before write out, so it's ok, but a
            // little whiff.
            continue;
        }

        granny_uint8 const* OldVert = VertexData->Vertices + (VertexSize * Vert);
        granny_uint8*       NewVert = Temp + (VertexSize * Permutation[Vert]);

        memcpy(NewVert, OldVert, VertexSize);
        ++ActuallyUsed;
    }}

    memcpy(VertexData->Vertices, Temp, VertexSize * ActuallyUsed);
    VertexData->VertexCount = ActuallyUsed;

    if (VertexData->VertexAnnotationSetCount != 0)
    {
        for (granny_int32x Set = 0; Set < VertexData->VertexAnnotationSetCount; ++Set)
        {
            ApplyAnnotationPermutation(VertexData->VertexAnnotationSets[Set],
                                       Permutation, ActuallyUsed, Arena);
        }
    }

    delete [] Temp;
}


granny_file_info*
VertexCacheOptimize(char const*          OriginalFilename,
                    char const*          OutputFilename,
                    granny_file_info*    Info,
                    key_value_pair*      KeyValues,
                    granny_int32x        NumKeyValues,
                    granny_memory_arena* TempArena)
{
    // Optimize all meshes for the pre/post-transform cache
    {for(granny_int32x i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        if (!Mesh)
            continue;

        granny_int32x const MeshIndexCount  = GrannyGetMeshIndexCount(Mesh);
        granny_int32x const MeshVertexCount = GrannyGetMeshVertexCount(Mesh);

        // IndexCopy holds the 4-byte version of the indices
        granny_int32* IndexCopy  = new granny_int32[MeshIndexCount];

        // New indices is what we'll replace the old indices with.  (We don't clean this
        // up, so allocate it in the arena)
        granny_int32* NewIndices =
            PushArray(TempArena, MeshIndexCount, granny_int32);

        // Initialize the arrays.
        GrannyCopyMeshIndices(Mesh, 4, IndexCopy);
        memset(NewIndices, 0, MeshIndexCount * sizeof(granny_int32));

        // Note that we have to do this in material group chunks
        {for(granny_int32x grp = 0; grp < Mesh->PrimaryTopology->GroupCount; ++grp)
        {
            granny_tri_material_group& Group = Mesh->PrimaryTopology->Groups[grp];

            VcacheOptimize(Group.TriCount,
                           IndexCopy + (Group.TriFirst * 3),
                           NewIndices + (Group.TriFirst * 3));
        }}

        // Done with IndexCopy
        delete [] IndexCopy;

        // Generate a permutation buffer from the indices that
        // optimizes for the pre-transform cache.  I.e, we encode the
        // vertices in "first-encountered" order.
        granny_int32x* Permutation = new granny_int32x[MeshVertexCount];
        {
            {for(granny_int32x Vert = 0; Vert < MeshVertexCount; ++Vert)
            {
                Permutation[Vert] = -1;
            }}

            granny_int32x Current = 0;
            {for(granny_int32x Index = 0; Index < MeshIndexCount; ++Index)
            {
                granny_int32 MeshIndex = NewIndices[Index];
                if (Permutation[MeshIndex] == -1)
                {
                    Permutation[MeshIndex] = Current++;
                }
            }}
            pp_assert(Current <= MeshVertexCount);
        }

        // Apply the permutation to all of the vertex_data's for the mesh
        ApplyPermutation(Permutation, Mesh->PrimaryVertexData, TempArena);
        {for(granny_int32x MorphTarget = 0; MorphTarget < Mesh->MorphTargetCount; ++MorphTarget)
        {
            // These can be null if the morph targets are converted into vertex curves
            if (Mesh->MorphTargets[MorphTarget].VertexData != 0)
                ApplyPermutation(Permutation, Mesh->MorphTargets[MorphTarget].VertexData, TempArena);
        }}

        // Permute the indices
        {for(granny_int32x Index = 0; Index < MeshIndexCount; ++Index)
        {
            pp_assert(Permutation[NewIndices[Index]] != -1);
            NewIndices[Index] = Permutation[NewIndices[Index]];
        }}

        // Done with the permutation buffer
        delete [] Permutation;

        // Note that we've destroyed the meaning of the vertex
        // annotations, so we'll null those out in addition to
        // replacing the indices
        Mesh->PrimaryTopology->IndexCount = MeshIndexCount;
        Mesh->PrimaryTopology->Indices = NewIndices;
        Mesh->PrimaryTopology->Index16Count = 0;
        Mesh->PrimaryTopology->Indices16 = 0;
        Mesh->PrimaryTopology->VertexToVertexCount = 0;
        Mesh->PrimaryTopology->VertexToVertexMap = 0;
        Mesh->PrimaryTopology->VertexToTriangleCount = 0;
        Mesh->PrimaryTopology->VertexToTriangleMap = 0;
        Mesh->PrimaryTopology->SideToNeighborCount = 0;
        Mesh->PrimaryTopology->SideToNeighborMap = 0;
        Mesh->PrimaryTopology->BonesForTriangleCount = 0;
        Mesh->PrimaryTopology->BonesForTriangle = 0;
        Mesh->PrimaryTopology->TriangleToBoneCount = 0;
        Mesh->PrimaryTopology->TriangleToBoneIndices = 0;
        Mesh->PrimaryTopology->TriAnnotationSetCount = 0;
        Mesh->PrimaryTopology->TriAnnotationSets = 0;
    }}

    return Info;
}

static const char* HelpString =
    (" VertexCacheOptimize: Reorders the mesh index order for a wide array of vertex\n"
     "  cache architectures. Reduces cache misses in the post-transform buffer found\n"
     "  in all modern GPUs.\n"
     "\n"
     "    preprocessor VertexCacheOptimize char.gr2 -output char_opt.gr2\n");

static CommandRegistrar RegVertexCacheOptimize(VertexCacheOptimize,
                                               "VertexCacheOptimize",
                                               "Optimizes the mesh order for a wide array of vertex cache architectures",
                                               HelpString);

