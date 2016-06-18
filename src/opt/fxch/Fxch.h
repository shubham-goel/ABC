/**CFile****************************************************************

  FileName    [ Fxch.h ]

  PackageName [ Fast eXtract with Cube Hashing (FXCH) ]

  Synopsis    [ External declarations of fast extract with cube hashing. ]

  Author      [ Bruno Schmitt - boschmitt at inf.ufrgs.br ]

  Affiliation [ UFRGS ]

  Date        [ Ver. 1.0. Started - March 6, 2016. ]

  Revision    []

***********************************************************************/

#ifndef ABC__opt__fxch__fxch_h
#define ABC__opt__fxch__fxch_h

#include "base/abc/abc.h"

#include "misc/vec/vecHsh.h"
#include "misc/vec/vecQue.h"
#include "misc/vec/vecVec.h"
#include "misc/vec/vecWec.h"

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                    TYPEDEF DECLARATIONS                          ///
////////////////////////////////////////////////////////////////////////
typedef struct Fxch_Man_t_               Fxch_Man_t;
typedef struct Fxch_SubCube_t_           Fxch_SubCube_t;
typedef struct Fxch_SCHashTable_t_       Fxch_SCHashTable_t;
typedef struct Fxch_SCHashTable_Entry_t_ Fxch_SCHashTable_Entry_t;
////////////////////////////////////////////////////////////////////////
///                    STRUCTURES DEFINITIONS                        ///
////////////////////////////////////////////////////////////////////////
/* Sub-Cube Structure
 *
 *   In the context of this program, a sub-cube is a cube derived from 
 *   another cube by removing one or two literals. Since a cube is represented
 *   as a vector of literals, one might think that a sub-cube would also be 
 *   represented in the same way. However, in order to same memory, we only 
 *   store a sub-cube identifier and the data necessary to generate the sub-cube:
 *        - The index number of the orignal cube in the cover. (iCube)
 *        - Identifiers of which literal(s) was(were) removed. (iLit0, iLit1)
 *
 *   The sub-cube identifier is generated by adding the unique identifiers of 
 *   its literals. 
 *
 */
struct Fxch_SubCube_t_
{
    unsigned int Id,          
                 iCube;      
    unsigned int iLit0 : 16,
                 iLit1 : 16;
};

/* Sub-cube Hash Table
 *
 */
struct Fxch_SCHashTable_Entry_t_
{
    Fxch_SubCube_t SCData;

    unsigned int iTable : 31,
                 Used   :  1;
    unsigned int iPrev,
                 iNext;
};

struct Fxch_SCHashTable_t_
{
    Fxch_Man_t* pFxchMan;
    /* Internal data */
    Fxch_SCHashTable_Entry_t* pBins;
    unsigned int nEntries,
                 SizeMask;
    Vec_Int_t*   vCubeLinks;

    /* Temporary data */
    Vec_Int_t    vSubCube0;
    Vec_Int_t    vSubCube1;
};

struct Fxch_Man_t_
{
    /* user's data */
    Vec_Wec_t* vCubes;
    int LitCountMax;

    /* internal data */
    Fxch_SCHashTable_t* pSCHashTable;

    Vec_Wec_t*    vLits;        /* lit -> cube */
    Vec_Int_t*    vLitCount;    /* literal counts (currently not used) */
    Vec_Int_t*    vLitHashKeys; /* Literal hash keys used to generate subcube hash */

    Hsh_VecMan_t* pDivHash;
    Vec_Flt_t*    vDivWeights;   /* divisor weights */
    Vec_Que_t*    vDivPrio;      /* priority queue for divisors by weight */
    Vec_Wec_t*    vDivCubePairs; /* cube pairs for each div */

    Vec_Int_t*    vLevels;       /* variable levels */

    // temporary data to update the data-structure when a divisor is extracted
    Vec_Int_t*     vCubesS;     /* cubes for the given single cube divisor */
    Vec_Int_t*     vPairs;     /* cube pairs for the given double cube divisor */
    Vec_Int_t*     vCubeFree;  // cube-free divisor
    Vec_Int_t*     vDiv;       // selected divisor

    /* Statistics */
    abctime timeInit;   /* Initialization time */
    abctime timeExt;    /* Extraction time */
    int     nVars;      // original problem variables
    int     nLits;      // the number of SOP literals
    int     nPairsS;    // number of lit pairs
    int     nPairsD;    // number of cube pairs
    int     nExtDivs;   /* Number of extracted divisor */
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////
/* The following functions are from abcFx.c
 * They are use in order to retrive SOP information for fast_extract
 * Since I want an implementation that change only the core part of
 * the algorithm I'm using this */
extern Vec_Wec_t* Abc_NtkFxRetrieve( Abc_Ntk_t* pNtk );
extern void       Abc_NtkFxInsert( Abc_Ntk_t* pNtk, Vec_Wec_t* vCubes );
extern int        Abc_NtkFxCheck( Abc_Ntk_t* pNtk );

/*===== Fxch.c =======================================================*/
int Abc_NtkFxchPerform( Abc_Ntk_t* pNtk, int nMaxDivExt, int fVerbose, int fVeryVerbose );
int Fxch_FastExtract( Vec_Wec_t* vCubes, int ObjIdMax, int nMaxDivExt, int fVerbose, int fVeryVerbose );

/*===== FxchDiv.c ====================================================================================================*/
int  Fxch_DivCreate( Fxch_Man_t* pFxchMan,  Fxch_SubCube_t* pSubCube0, Fxch_SubCube_t* pSubCube1 );
int  Fxch_DivAdd( Fxch_Man_t* pFxchMan, int fUpdate, int fSingleCube, int fBase );
int  Fxch_DivRemove( Fxch_Man_t* pFxchMan, int fUpdate, int fSingleCube, int fBase );
void Fxch_DivSepareteCubes( Vec_Int_t* vDiv, Vec_Int_t* vCube0, Vec_Int_t* vCube1 );
int  Fxch_DivRemoveLits( Vec_Int_t* vCube0, Vec_Int_t* vCube1, Vec_Int_t* vDiv, int *fCompl );
void Fxch_DivPrint( Fxch_Man_t* pFxchMan, int iDiv );

/*===== FxchMan.c ====================================================================================================*/
Fxch_Man_t* Fxch_ManAlloc( Vec_Wec_t* vCubes );
void  Fxch_ManFree( Fxch_Man_t* pFxchMan );
void  Fxch_ManMapLiteralsIntoCubes( Fxch_Man_t* pFxchMan, int nVars );
void  Fxch_ManGenerateLitHashKeys( Fxch_Man_t* pFxchMan );
void  Fxch_ManSCHashTablesInit( Fxch_Man_t* pFxchMan );
void  Fxch_ManSCHashTablesFree( Fxch_Man_t* pFxchMan );
void  Fxch_ManDivCreate( Fxch_Man_t* pFxchMan );
int   Fxch_ManComputeLevelDiv( Fxch_Man_t* pFxchMan, Vec_Int_t* vCubeFree );
int   Fxch_ManComputeLevelCube( Fxch_Man_t* pFxchMan, Vec_Int_t* vCube );
void  Fxch_ManComputeLevel( Fxch_Man_t* pFxchMan );
void  Fxch_ManUpdate( Fxch_Man_t* pFxchMan, int iDiv );
void  Fxch_ManPrintDivs( Fxch_Man_t* pFxchMan );
void  Fxch_ManPrintStats( Fxch_Man_t* pFxchMan );

static inline Vec_Int_t* Fxch_ManGetCube( Fxch_Man_t* pFxchMan,
                                          int iCube )
{
    return Vec_WecEntry( pFxchMan->vCubes, iCube );
}

static inline int Fxch_ManGetLit( Fxch_Man_t* pFxchMan,
                                  int iCube,
                                  int iLit )
{
    return Vec_IntEntry( Vec_WecEntry(pFxchMan->vCubes, iCube), iLit );
}

/*===== FxchSCHashTable.c ============================================*/
Fxch_SCHashTable_t* Fxch_SCHashTableCreate( Fxch_Man_t* pFxchMan, Vec_Int_t* vCubeLinks, int nEntries );

void Fxch_SCHashTableDelete( Fxch_SCHashTable_t* );

int Fxch_SCHashTableInsert( Fxch_SCHashTable_t* pSCHashTable,
                            Vec_Wec_t* vCubes,
                            unsigned int SubCubeID, 
                            unsigned int iSubCube,
                            unsigned int iCube, 
                            unsigned int iLit0, 
                            unsigned int iLit1,
                            char fUpdate );

int Fxch_SCHashTableRemove( Fxch_SCHashTable_t* pSCHashTable,
                            Vec_Wec_t* vCubes,
                            unsigned int SubCubeID, 
                            unsigned int iSubCube,
                            unsigned int iCube, 
                            unsigned int iLit0, 
                            unsigned int iLit1,
                            char fUpdate );

unsigned int Fxch_SCHashTableMemory( Fxch_SCHashTable_t* );
void Fxch_SCHashTablePrint( Fxch_SCHashTable_t* );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

