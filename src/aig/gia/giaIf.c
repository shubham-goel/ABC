/**CFile****************************************************************

  FileName    [giaMap.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Scalable AIG package.]

  Synopsis    [Manipulation of mapping associated with the AIG.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: giaMap.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "gia.h"
#include "aig/aig/aig.h"
#include "map/if/if.h"
#include "bool/kit/kit.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

extern int Kit_TruthToGia( Gia_Man_t * pMan, unsigned * pTruth, int nVars, Vec_Int_t * vMemory, Vec_Int_t * vLeaves, int fHash );
extern int Abc_RecToGia3( Gia_Man_t * pMan, If_Man_t * pIfMan, If_Cut_t * pCut, Vec_Int_t * vLeaves, int fHash );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Load the network into FPGA manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSetIfParsDefault( void * pp )
{
    If_Par_t * pPars = (If_Par_t *)pp;
//    extern void * Abc_FrameReadLibLut();
    If_Par_t * p = (If_Par_t *)pPars;
    // set defaults
    memset( p, 0, sizeof(If_Par_t) );
    // user-controlable paramters
    p->nLutSize    = -1;
//    p->nLutSize    =  6;
    p->nCutsMax    =  8;
    p->nFlowIters  =  1;
    p->nAreaIters  =  2;
    p->DelayTarget = -1;
    p->Epsilon     =  (float)0.005;
    p->fPreprocess =  1;
    p->fArea       =  0;
    p->fFancy      =  0;
    p->fExpRed     =  1; ////
    p->fLatchPaths =  0;
    p->fEdge       =  1;
    p->fPower      =  0;
    p->fCutMin     =  0;
    p->fSeqMap     =  0;
    p->fVerbose    =  0;
    p->pLutStruct  =  NULL;
    // internal parameters
    p->fTruth      =  0;
    p->nLatchesCi  =  0;
    p->nLatchesCo  =  0;
    p->fLiftLeaves =  0;
    p->fUseCoAttrs =  1;   // use CO attributes
    p->pLutLib     =  NULL;
    p->pTimesArr   =  NULL; 
    p->pTimesReq   =  NULL;   
    p->pFuncCost   =  NULL;   
}


/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManLutFaninCount( Gia_Man_t * p )
{
    int i, Counter = 0;
    Gia_ManForEachLut( p, i )
        Counter += Gia_ObjLutSize(p, i);
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManLutSizeMax( Gia_Man_t * p )
{
    int i, nSizeMax = -1;
    Gia_ManForEachLut( p, i )
        nSizeMax = Abc_MaxInt( nSizeMax, Gia_ObjLutSize(p, i) );
    return nSizeMax;
}

/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManLutNum( Gia_Man_t * p )
{
    int i, Counter = 0;
    Gia_ManForEachLut( p, i )
        Counter ++;
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManLutLevel( Gia_Man_t * p )
{
    Gia_Obj_t * pObj;
    int i, k, iFan, Level;
    int * pLevels = ABC_CALLOC( int, Gia_ManObjNum(p) );
    Gia_ManForEachLut( p, i )
    {
        Level = 0;
        Gia_LutForEachFanin( p, i, iFan, k )
            if ( Level < pLevels[iFan] )
                Level = pLevels[iFan];
        pLevels[i] = Level + 1;
    }
    Level = 0;
    Gia_ManForEachCo( p, pObj, k )
        if ( Level < pLevels[Gia_ObjFaninId0p(p, pObj)] )
            Level = pLevels[Gia_ObjFaninId0p(p, pObj)];
    ABC_FREE( pLevels );
    return Level;
}

/**Function*************************************************************

  Synopsis    [Assigns levels.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSetRefsMapped( Gia_Man_t * p )  
{
    Gia_Obj_t * pObj;
    int i, k, iFan;
    ABC_FREE( p->pRefs );
    p->pRefs = ABC_CALLOC( int, Gia_ManObjNum(p) );
    Gia_ManForEachCo( p, pObj, i )
        Gia_ObjRefInc( p, Gia_ObjFanin0(pObj) );
    Gia_ManForEachLut( p, i )
        Gia_LutForEachFanin( p, i, iFan, k )
            Gia_ObjRefInc( p, Gia_ManObj(p, iFan) );
}

/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManPrintMappingStats( Gia_Man_t * p )
{
    int * pLevels;
    int i, k, iFan, nLutSize = 0, nLuts = 0, nFanins = 0, LevelMax = 0;
    if ( !Gia_ManHasMapping(p) )
        return;
    pLevels = ABC_CALLOC( int, Gia_ManObjNum(p) );
    Gia_ManForEachLut( p, i )
    {
        nLuts++;
        nFanins += Gia_ObjLutSize(p, i);
        nLutSize = Abc_MaxInt( nLutSize, Gia_ObjLutSize(p, i) );
        Gia_LutForEachFanin( p, i, iFan, k )
            pLevels[i] = Abc_MaxInt( pLevels[i], pLevels[iFan] );
        pLevels[i]++;
        LevelMax = Abc_MaxInt( LevelMax, pLevels[i] );
    }
    ABC_FREE( pLevels );
    Abc_Print( 1, "Mapping (K=%d)  :  ", nLutSize );
    Abc_Print( 1, "lut =%7d  ", nLuts );
    Abc_Print( 1, "edge =%8d  ", nFanins );
    Abc_Print( 1, "lev =%5d  ", LevelMax );
    Abc_Print( 1, "mem =%5.2f MB", 4.0*(Gia_ManObjNum(p) + 2*nLuts + nFanins)/(1<<20) );
    Abc_Print( 1, "\n" );
}

/**Function*************************************************************

  Synopsis    [Prints mapping statistics.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManPrintPackingStats( Gia_Man_t * p )
{
    int fVerbose = 0;
    int nObjToShow = 200;
    int nNumStr[5] = {0};
    int i, k, Entry, nEntries, nEntries2, MaxSize = -1;
    if ( p->vPacking == NULL )
        return;
    nEntries = Vec_IntEntry( p->vPacking, 0 );
    nEntries2 = 0;
    Vec_IntForEachEntryStart( p->vPacking, Entry, i, 1 )
    {
        assert( Entry > 0 && Entry < 4 );
        nNumStr[Entry]++;
        i++;
        if ( fVerbose && nEntries2 < nObjToShow ) Abc_Print( 1, "{ " );
        for ( k = 0; k < Entry; k++, i++ )
            if ( fVerbose && nEntries2 < nObjToShow ) Abc_Print( 1, "%d ", Vec_IntEntry(p->vPacking, i) );
        if ( fVerbose && nEntries2 < nObjToShow ) Abc_Print( 1, "}\n" );
        i--;
        nEntries2++;
    }
    assert( nEntries == nEntries2 );
    if ( nNumStr[3] > 0 )
        MaxSize = 3;
    else if ( nNumStr[2] > 0 )
        MaxSize = 2;
    else if ( nNumStr[1] > 0 )
        MaxSize = 1;
    Abc_Print( 1, "Packing (N=%d)  :  ", MaxSize );
    for ( i = 1; i <= MaxSize; i++ )
        Abc_Print( 1, "%d x LUT = %d   ", i, nNumStr[i] );
    Abc_Print( 1, "Total = %d", nEntries2 );
    Abc_Print( 1, "\n" );
}


/**Function*************************************************************

  Synopsis    [Computes levels for AIG with choices and white boxes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManChoiceLevel_rec( Gia_Man_t * p, Gia_Obj_t * pObj )
{
    Tim_Man_t * pManTime = (Tim_Man_t *)p->pManTime;
    Gia_Obj_t * pNext;
    int i, iBox, iTerm1, nTerms, LevelMax = 0;
    if ( Gia_ObjIsTravIdCurrent( p, pObj ) )
        return;
    Gia_ObjSetTravIdCurrent( p, pObj );
    if ( Gia_ObjIsCi(pObj) )
    {
        if ( pManTime )
        {
            iBox = Tim_ManBoxForCi( pManTime, Gia_ObjCioId(pObj) );
            if ( iBox >= 0 ) // this is not a true PI
            {
                iTerm1 = Tim_ManBoxInputFirst( pManTime, iBox );
                nTerms = Tim_ManBoxInputNum( pManTime, iBox );
                for ( i = 0; i < nTerms; i++ )
                {
                    pNext = Gia_ManCo( p, iTerm1 + i );
                    Gia_ManChoiceLevel_rec( p, pNext );
                    if ( LevelMax < Gia_ObjLevel(p, pNext) )
                        LevelMax = Gia_ObjLevel(p, pNext);
                }
                LevelMax++;
            }
        }
//        Abc_Print( 1, "%d ", pObj->Level );
    }
    else if ( Gia_ObjIsCo(pObj) )
    {
        pNext = Gia_ObjFanin0(pObj);
        Gia_ManChoiceLevel_rec( p, pNext );
        if ( LevelMax < Gia_ObjLevel(p, pNext) )
            LevelMax = Gia_ObjLevel(p, pNext);
    }
    else if ( Gia_ObjIsAnd(pObj) )
    { 
        // get the maximum level of the two fanins
        pNext = Gia_ObjFanin0(pObj);
        Gia_ManChoiceLevel_rec( p, pNext );
        if ( LevelMax < Gia_ObjLevel(p, pNext) )
            LevelMax = Gia_ObjLevel(p, pNext);
        pNext = Gia_ObjFanin1(pObj);
        Gia_ManChoiceLevel_rec( p, pNext );
        if ( LevelMax < Gia_ObjLevel(p, pNext) )
            LevelMax = Gia_ObjLevel(p, pNext);
        LevelMax++;

        // get the level of the nodes in the choice node
        if ( (pNext = Gia_ObjSiblObj(p, Gia_ObjId(p, pObj))) )
        {
            Gia_ManChoiceLevel_rec( p, pNext );
            if ( LevelMax < Gia_ObjLevel(p, pNext) )
                LevelMax = Gia_ObjLevel(p, pNext);
        }
    }
    else if ( !Gia_ObjIsConst0(pObj) )
        assert( 0 );
    Gia_ObjSetLevel( p, pObj, LevelMax );
}
int Gia_ManChoiceLevel( Gia_Man_t * p )
{
    Gia_Obj_t * pObj;
    int i, LevelMax = 0;
//    assert( Gia_ManRegNum(p) == 0 );
    Gia_ManCleanLevels( p, Gia_ManObjNum(p) );
    Gia_ManIncrementTravId( p );
    Gia_ManForEachCo( p, pObj, i )
    {
        Gia_ManChoiceLevel_rec( p, pObj );
        if ( LevelMax < Gia_ObjLevel(p, pObj) )
            LevelMax = Gia_ObjLevel(p, pObj);
    }
    // account for dangling boxes
    Gia_ManForEachCi( p, pObj, i )
    {
        Gia_ManChoiceLevel_rec( p, pObj );
        if ( LevelMax < Gia_ObjLevel(p, pObj) )
            LevelMax = Gia_ObjLevel(p, pObj);
//        Abc_Print( 1, "%d ", Gia_ObjLevel(p, pObj) );
    }
//    Abc_Print( 1, "\n" );
    Gia_ManForEachAnd( p, pObj, i )
        assert( Gia_ObjLevel(p, pObj) > 0 );
//    printf( "Max level %d\n", LevelMax );
    return LevelMax;
} 



/**Function*************************************************************

  Synopsis    [Converts GIA into IF manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline If_Obj_t * If_ManFanin0Copy( If_Man_t * pIfMan, Gia_Obj_t * pObj ) { return If_NotCond( If_ManObj(pIfMan, Gia_ObjValue(Gia_ObjFanin0(pObj))), Gia_ObjFaninC0(pObj) ); }
static inline If_Obj_t * If_ManFanin1Copy( If_Man_t * pIfMan, Gia_Obj_t * pObj ) { return If_NotCond( If_ManObj(pIfMan, Gia_ObjValue(Gia_ObjFanin1(pObj))), Gia_ObjFaninC1(pObj) ); }
If_Man_t * Gia_ManToIf( Gia_Man_t * p, If_Par_t * pPars )
{
    If_Man_t * pIfMan;
    If_Obj_t * pIfObj;
    Gia_Obj_t * pObj;
    int i;
    // create levels with choices
    Gia_ManChoiceLevel( p );
    // mark representative nodes
    Gia_ManMarkFanoutDrivers( p );
    // start the mapping manager and set its parameters
    pIfMan = If_ManStart( pPars );
    pIfMan->pName = Abc_UtilStrsav( Gia_ManName(p) );
    // print warning about excessive memory usage
    if ( 1.0 * Gia_ManObjNum(p) * pIfMan->nObjBytes / (1<<30) > 1.0 )
        printf( "Warning: The mapper will allocate %.1f GB for to represent the subject graph with %d AIG nodes.\n", 
            1.0 * Gia_ManObjNum(p) * pIfMan->nObjBytes / (1<<30), Gia_ManObjNum(p) );
    // load the AIG into the mapper
    Gia_ManFillValue( p );
    Gia_ManConst0(p)->Value = If_ObjId( If_ManConst1(pIfMan) );
    Gia_ManForEachObj1( p, pObj, i )
    {
        if ( Gia_ObjIsAnd(pObj) )
            pIfObj = If_ManCreateAnd( pIfMan, If_ManFanin0Copy(pIfMan, pObj), If_ManFanin1Copy(pIfMan, pObj) );
        else if ( Gia_ObjIsCi(pObj) )
        {
            pIfObj = If_ManCreateCi( pIfMan );
            If_ObjSetLevel( pIfObj, Gia_ObjLevel(p, pObj) );
//            Abc_Print( 1, "pi%d=%d\n ", If_ObjId(pIfObj), If_ObjLevel(pIfObj) );
            if ( pIfMan->nLevelMax < (int)pIfObj->Level )
                pIfMan->nLevelMax = (int)pIfObj->Level;
        }
        else if ( Gia_ObjIsCo(pObj) )
        {
            pIfObj = If_ManCreateCo( pIfMan, If_NotCond( If_ManFanin0Copy(pIfMan, pObj), Gia_ObjIsConst0(Gia_ObjFanin0(pObj))) );
//            Abc_Print( 1, "po%d=%d\n ", If_ObjId(pIfObj), If_ObjLevel(pIfObj) );
        }
        else assert( 0 );
        assert( i == If_ObjId(pIfObj) );
        Gia_ObjSetValue( pObj, If_ObjId(pIfObj) );
        // set up the choice node
        if ( Gia_ObjSibl(p, i) && pObj->fMark0 )
        {
            Gia_Obj_t * pSibl, * pPrev;
            for ( pPrev = pObj, pSibl = Gia_ObjSiblObj(p, i); pSibl; pPrev = pSibl, pSibl = Gia_ObjSiblObj(p, Gia_ObjId(p, pSibl)) )
                If_ObjSetChoice( If_ManObj(pIfMan, Gia_ObjValue(pObj)), If_ManObj(pIfMan, Gia_ObjValue(pSibl)) );
            If_ManCreateChoice( pIfMan, If_ManObj(pIfMan, Gia_ObjValue(pObj)) );
            pPars->fExpRed = 0;
        }
//        assert( If_ObjLevel(pIfObj) == Gia_ObjLevel(pNode) );
    }
    Gia_ManCleanMark0( p );
    return pIfMan;
}


/**Function*************************************************************

  Synopsis    [Derives node's AIG after SOP balancing]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManNodeIfSopToGiaInt( Gia_Man_t * pNew, Vec_Wrd_t * vAnds, int nVars, Vec_Int_t * vLeaves, int fHash )
{
    Vec_Int_t * vResults;
    int iRes0, iRes1, iRes = -1;
    If_And_t This;
    word Entry;
    int i;
    if ( Vec_WrdSize(vAnds) == 0 )
        return 0;
    if ( Vec_WrdSize(vAnds) == 1 && Vec_WrdEntry(vAnds,0) == 0 )
        return 1;
    vResults = Vec_IntAlloc( Vec_WrdSize(vAnds) );
    for ( i = 0; i < nVars; i++ )
        Vec_IntPush( vResults, Vec_IntEntry(vLeaves, i) );
    Vec_WrdForEachEntryStart( vAnds, Entry, i, nVars )
    {
        This  = If_WrdToAnd( Entry );
        iRes0 = Abc_LitNotCond( Vec_IntEntry(vResults, This.iFan0), This.fCompl0 ); 
        iRes1 = Abc_LitNotCond( Vec_IntEntry(vResults, This.iFan1), This.fCompl1 ); 
        if ( fHash )
            iRes  = Gia_ManHashAnd( pNew, iRes0, iRes1 );
        else if ( iRes0 == iRes1 )
            iRes = iRes0;
        else
            iRes  = Gia_ManAppendAnd( pNew, iRes0, iRes1 );
        Vec_IntPush( vResults, iRes );
    }
    Vec_IntFree( vResults );
    return Abc_LitNotCond( iRes, This.fCompl );
}
int Gia_ManNodeIfSopToGia( Gia_Man_t * pNew, If_Man_t * p, If_Cut_t * pCut, Vec_Int_t * vLeaves, int fHash )
{
    int iResult;
    Vec_Wrd_t * vArray;
    vArray  = If_CutDelaySopArray( p, pCut );
    iResult = Gia_ManNodeIfSopToGiaInt( pNew, vArray, If_CutLeaveNum(pCut), vLeaves, fHash );
//    Vec_WrdFree( vArray );
    return iResult;
}

/**Function*************************************************************

  Synopsis    [Converts IF into GIA manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Gia_ManFromIfAig( If_Man_t * pIfMan )
{
    int fHash = 0;
    Gia_Man_t * pNew;
    If_Obj_t * pIfObj, * pIfLeaf;
    If_Cut_t * pCutBest;
    Vec_Int_t * vLeaves;
    Vec_Int_t * vCover;
    int i, k;
    assert( pIfMan->pPars->pLutStruct == NULL );
    assert( pIfMan->pPars->fDelayOpt || pIfMan->pPars->fUserRecLib );
    // create new manager
    pNew = Gia_ManStart( If_ManObjNum(pIfMan) );
    Gia_ManHashAlloc( pNew );
    // iterate through nodes used in the mapping
    vCover = Vec_IntAlloc( 1 << 16 );
    vLeaves = Vec_IntAlloc( 16 );
    If_ManCleanCutData( pIfMan );
    If_ManForEachObj( pIfMan, pIfObj, i )
    {
        if ( pIfObj->nRefs == 0 && !If_ObjIsTerm(pIfObj) )
            continue;
        if ( If_ObjIsAnd(pIfObj) )
        {
            pCutBest = If_ObjCutBest( pIfObj );
            // collect leaves of the best cut
            Vec_IntClear( vLeaves );
            If_CutForEachLeaf( pIfMan, pCutBest, pIfLeaf, k )
                Vec_IntPush( vLeaves, pIfLeaf->iCopy );
            // get the functionality
            if ( pIfMan->pPars->fDelayOpt )
                pIfObj->iCopy = Gia_ManNodeIfSopToGia( pNew, pIfMan, pCutBest, vLeaves, fHash );
            else if ( pIfMan->pPars->fUserRecLib )
                pIfObj->iCopy = Abc_RecToGia3( pNew, pIfMan, pCutBest, vLeaves, fHash );
            else assert( 0 );
        }
        else if ( If_ObjIsCi(pIfObj) )
            pIfObj->iCopy = Gia_ManAppendCi(pNew);
        else if ( If_ObjIsCo(pIfObj) )
            pIfObj->iCopy = Gia_ManAppendCo( pNew, Abc_LitNotCond(If_ObjFanin0(pIfObj)->iCopy, If_ObjFaninC0(pIfObj)) );
        else if ( If_ObjIsConst1(pIfObj) )
            pIfObj->iCopy = 1;
        else assert( 0 );
    }
    Vec_IntFree( vCover );
    Vec_IntFree( vLeaves );
    Gia_ManHashStop( pNew );
    return pNew;
}


/**Function*************************************************************

  Synopsis    [Write mapping for LUT with given fanins.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManFromIfLogicCreateLut( Gia_Man_t * pNew, word * pRes, Vec_Int_t * vLeaves, Vec_Int_t * vCover, Vec_Int_t * vMapping, Vec_Int_t * vMapping2 )
{
    int i, iLit, iObjLit1;
    iObjLit1 = Kit_TruthToGia( pNew, (unsigned *)pRes, Vec_IntSize(vLeaves), vCover, vLeaves, 0 );
    // write mapping
    Vec_IntSetEntry( vMapping, Abc_Lit2Var(iObjLit1), Vec_IntSize(vMapping2) );
    Vec_IntPush( vMapping2, Vec_IntSize(vLeaves) );
//    Vec_IntForEachEntry( vLeaves, iLit, i )
//        assert( Abc_Lit2Var(iLit) < Abc_Lit2Var(iObjLit1) );
    Vec_IntForEachEntry( vLeaves, iLit, i )
        Vec_IntPush( vMapping2, Abc_Lit2Var(iLit)  );
    Vec_IntPush( vMapping2, Abc_Lit2Var(iObjLit1) );
    return iObjLit1;
}


/**Function*************************************************************

  Synopsis    [Write the node into a file.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManFromIfLogicNode( Gia_Man_t * pNew, int iObj, Vec_Int_t * vLeaves, Vec_Int_t * vLeavesTemp, 
    word * pRes, char * pStr, Vec_Int_t * vCover, Vec_Int_t * vMapping, Vec_Int_t * vMapping2, Vec_Int_t * vPacking )
{
    int nLeaves = Vec_IntSize(vLeaves);
    int i, Length, nLutLeaf, nLutLeaf2, nLutRoot, iObjLit1, iObjLit2, iObjLit3;
    // check simple case
/*
    static word s_Truths6[6] = {
        ABC_CONST(0xAAAAAAAAAAAAAAAA),
        ABC_CONST(0xCCCCCCCCCCCCCCCC),
        ABC_CONST(0xF0F0F0F0F0F0F0F0),
        ABC_CONST(0xFF00FF00FF00FF00),
        ABC_CONST(0xFFFF0000FFFF0000),
        ABC_CONST(0xFFFFFFFF00000000)
    };
    if ( *pRes == 0 || ~*pRes == 0 )
        return Abc_LitNotCond( 0, ~*pRes == 0 );
    for ( i = 0; i < Vec_IntSize(vLeaves); i++ )
        if ( *pRes == s_Truths6[i] || ~*pRes == s_Truths6[i] )
            return Abc_LitNotCond( Vec_IntEntry(vLeaves, i), ~*pRes == s_Truths6[i] );
*/
/*
    if ( *pRes == 0 || ~*pRes == 0 )
        printf( "Const\n" );
    for ( i = 0; i < Vec_IntSize(vLeaves); i++ )
        if ( *pRes == s_Truths6[i] || ~*pRes == s_Truths6[i] )
            printf( "Literal\n" );
*/
    // check if there is no LUT structures
    if ( pStr == NULL )
        return Gia_ManFromIfLogicCreateLut( pNew, pRes, vLeaves, vCover, vMapping, vMapping2 );

    // quit if parameters are wrong
    Length = strlen(pStr);
    if ( Length != 2 && Length != 3 )
    {
        printf( "Wrong LUT struct (%s)\n", pStr );
        return -1;
    }
    for ( i = 0; i < Length; i++ )
        if ( pStr[i] - '0' < 3 || pStr[i] - '0' > 6 )
        {
            printf( "The LUT size (%d) should belong to {3,4,5,6}.\n", pStr[i] - '0' );
            return -1;
        }

    nLutLeaf  =                   pStr[0] - '0';
    nLutLeaf2 = ( Length == 3 ) ? pStr[1] - '0' : 0;
    nLutRoot  =                   pStr[Length-1] - '0';
    if ( nLeaves > nLutLeaf - 1 + (nLutLeaf2 ? nLutLeaf2 - 1 : 0) + nLutRoot )
    {
        printf( "The node size (%d) is too large for the LUT structure %s.\n", nLeaves, pStr );
        return -1;
    }

    // consider easy case
    if ( nLeaves <= Abc_MaxInt( nLutLeaf2, Abc_MaxInt(nLutLeaf, nLutRoot) ) )
    {
        // create mapping
        iObjLit1 = Gia_ManFromIfLogicCreateLut( pNew, pRes, vLeaves, vCover, vMapping, vMapping2 );
        // write packing
        Vec_IntPush( vPacking, 1 );
        Vec_IntPush( vPacking, Abc_Lit2Var(iObjLit1) );
        Vec_IntAddToEntry( vPacking, 0, 1 );
        return iObjLit1;
    }
    else
    {
        extern int If_CluMinimumBase( word * t, int * pSupp, int nVarsAll, int * pnVars );

        static word TruthStore[16][1<<10] = {{0}}, * pTruths[16];
        word Func0, Func1, Func2;
        char pLut0[32], pLut1[32], pLut2[32] = {0};

        if ( TruthStore[0][0] == 0 )
        {
            static word Truth6[6] = {
                0xAAAAAAAAAAAAAAAA,
                0xCCCCCCCCCCCCCCCC,
                0xF0F0F0F0F0F0F0F0,
                0xFF00FF00FF00FF00,
                0xFFFF0000FFFF0000,
                0xFFFFFFFF00000000
            };
            int nVarsMax = 16;
            int nWordsMax = (1 << 10);
            int i, k;
            assert( nVarsMax <= 16 );
            for ( i = 0; i < nVarsMax; i++ )
                pTruths[i] = TruthStore[i];
            for ( i = 0; i < 6; i++ )
                for ( k = 0; k < nWordsMax; k++ )
                    pTruths[i][k] = Truth6[i];
            for ( i = 6; i < nVarsMax; i++ )
                for ( k = 0; k < nWordsMax; k++ )
                    pTruths[i][k] = ((k >> (i-6)) & 1) ? ~(word)0 : 0;
        }
        // derive truth table
        if ( Kit_TruthIsConst0((unsigned *)pRes, nLeaves) || Kit_TruthIsConst1((unsigned *)pRes, nLeaves) )
        {
//            fprintf( pFile, ".names %s\n %d\n", Abc_ObjName(Abc_ObjFanout0(pObj)), Kit_TruthIsConst1((unsigned *)pRes, nLeaves) );
            iObjLit1 = Abc_LitNotCond( 0, Kit_TruthIsConst1((unsigned *)pRes, nLeaves) );
            // write mapping
            if ( Vec_IntEntry(vMapping, 0) == 0 )
            {
                Vec_IntSetEntry( vMapping, 0, Vec_IntSize(vMapping2) );
                Vec_IntPush( vMapping2, 0 );
                Vec_IntPush( vMapping2, 0 );
            }
            return iObjLit1;
        }

        // perform decomposition
        if ( Length == 2 )
        {
            if ( !If_CluCheckExt( NULL, pRes, nLeaves, nLutLeaf, nLutRoot, pLut0, pLut1, &Func0, &Func1 ) )
            {
                Extra_PrintHex( stdout, (unsigned *)pRes, nLeaves );  printf( "    " );
                Kit_DsdPrintFromTruth( (unsigned*)pRes, nLeaves );  printf( "\n" );
                printf( "Node %d is not decomposable. Deriving LUT structures has failed.\n", iObj );
                return -1;
            }
        }
        else
        {
            if ( !If_CluCheckExt3( NULL, pRes, nLeaves, nLutLeaf, nLutLeaf2, nLutRoot, pLut0, pLut1, pLut2, &Func0, &Func1, &Func2 ) )
            {
                Extra_PrintHex( stdout, (unsigned *)pRes, nLeaves );  printf( "    " );
                Kit_DsdPrintFromTruth( (unsigned*)pRes, nLeaves );  printf( "\n" );
                printf( "Node %d is not decomposable. Deriving LUT structures has failed.\n", iObj );
                return -1;
            }
        }

/*
        // write leaf node
        Id = Abc2_NtkAllocObj( pNew, pLut1[0], Abc2_ObjType(pObj) );
        iObjLit1 = Abc_Var2Lit( Id, 0 );
        pObjNew = Abc2_NtkObj( pNew, Id );
        for ( i = 0; i < pLut1[0]; i++ )
            Abc2_ObjSetFaninLit( pObjNew, i, Abc2_ObjFaninCopy(pObj, pLut1[2+i]) );
        Abc2_ObjSetTruth( pObjNew, Func1 );
*/
        // write leaf node
        Vec_IntClear( vLeavesTemp );
        for ( i = 0; i < pLut1[0]; i++ )
            Vec_IntPush( vLeavesTemp, Vec_IntEntry(vLeaves, pLut1[2+i]) );
        iObjLit1 = Gia_ManFromIfLogicCreateLut( pNew, &Func1, vLeavesTemp, vCover, vMapping, vMapping2 );

        if ( Length == 3 && pLut2[0] > 0 )
        {
        /*
            Id = Abc2_NtkAllocObj( pNew, pLut2[0], Abc2_ObjType(pObj) );
            iObjLit2 = Abc_Var2Lit( Id, 0 );
            pObjNew = Abc2_NtkObj( pNew, Id );
            for ( i = 0; i < pLut2[0]; i++ )
                if ( pLut2[2+i] == nLeaves )
                    Abc2_ObjSetFaninLit( pObjNew, i, iObjLit1 );
                else
                    Abc2_ObjSetFaninLit( pObjNew, i, Abc2_ObjFaninCopy(pObj, pLut2[2+i]) );
            Abc2_ObjSetTruth( pObjNew, Func2 );
        */

            // write leaf node
            Vec_IntClear( vLeavesTemp );
            for ( i = 0; i < pLut2[0]; i++ )
                if ( pLut2[2+i] == nLeaves )
                    Vec_IntPush( vLeavesTemp, iObjLit1 );
                else
                    Vec_IntPush( vLeavesTemp, Vec_IntEntry(vLeaves, pLut2[2+i]) );
            iObjLit2 = Gia_ManFromIfLogicCreateLut( pNew, &Func2, vLeavesTemp, vCover, vMapping, vMapping2 );

            // write packing
            Vec_IntPush( vPacking, 3 );
            Vec_IntPush( vPacking, Abc_Lit2Var(iObjLit1) );
            Vec_IntPush( vPacking, Abc_Lit2Var(iObjLit2) );
        }
        else
        {
            // write packing
            Vec_IntPush( vPacking, 2 );
            Vec_IntPush( vPacking, Abc_Lit2Var(iObjLit1) );
            iObjLit2 = -1;
        }
/*
        // write root node
        Id = Abc2_NtkAllocObj( pNew, pLut0[0], Abc2_ObjType(pObj) );
        iObjLit3 = Abc_Var2Lit( Id, 0 );
        pObjNew = Abc2_NtkObj( pNew, Id );
        for ( i = 0; i < pLut0[0]; i++ )
            if ( pLut0[2+i] == nLeaves )
                Abc2_ObjSetFaninLit( pObjNew, i, iObjLit1 );
            else if ( pLut0[2+i] == nLeaves+1 )
                Abc2_ObjSetFaninLit( pObjNew, i, iObjLit2 );
            else
                Abc2_ObjSetFaninLit( pObjNew, i, Abc2_ObjFaninCopy(pObj, pLut0[2+i]) );
        Abc2_ObjSetTruth( pObjNew, Func0 );
        Abc2_ObjSetCopy( pObj, iObjLit3 );
*/
        // write root node
        Vec_IntClear( vLeavesTemp );
        for ( i = 0; i < pLut0[0]; i++ )
            if ( pLut0[2+i] == nLeaves )
                Vec_IntPush( vLeavesTemp, iObjLit1 );
            else if ( pLut0[2+i] == nLeaves+1 )
                Vec_IntPush( vLeavesTemp, iObjLit2 );
            else
                Vec_IntPush( vLeavesTemp, Vec_IntEntry(vLeaves, pLut0[2+i]) );
        iObjLit3 = Gia_ManFromIfLogicCreateLut( pNew, &Func0, vLeavesTemp, vCover, vMapping, vMapping2 );

        // write packing
        Vec_IntPush( vPacking, Abc_Lit2Var(iObjLit3) );
        Vec_IntAddToEntry( vPacking, 0, 1 );
    }
    return iObjLit3;
}

/**Function*************************************************************

  Synopsis    [Recursively derives the local AIG for the cut.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManNodeIfToGia_rec( Gia_Man_t * pNew, If_Man_t * pIfMan, If_Obj_t * pIfObj, Vec_Ptr_t * vVisited, int fHash )
{
    If_Cut_t * pCut;
    If_Obj_t * pTemp;
    int iFunc, iFunc0, iFunc1;
    // get the best cut
    pCut = If_ObjCutBest(pIfObj);
    // if the cut is visited, return the result
    if ( If_CutDataInt(pCut) )
        return If_CutDataInt(pCut);
    // mark the node as visited
    Vec_PtrPush( vVisited, pCut );
    // insert the worst case
    If_CutSetDataInt( pCut, ~0 );
    // skip in case of primary input
    if ( If_ObjIsCi(pIfObj) )
        return If_CutDataInt(pCut);
    // compute the functions of the children
    for ( pTemp = pIfObj; pTemp; pTemp = pTemp->pEquiv )
    {
        iFunc0 = Gia_ManNodeIfToGia_rec( pNew, pIfMan, pTemp->pFanin0, vVisited, fHash );
        if ( iFunc0 == ~0 )
            continue;
        iFunc1 = Gia_ManNodeIfToGia_rec( pNew, pIfMan, pTemp->pFanin1, vVisited, fHash );
        if ( iFunc1 == ~0 )
            continue;
        // both branches are solved
        if ( fHash )
            iFunc = Gia_ManHashAnd( pNew, Abc_LitNotCond(iFunc0, pTemp->fCompl0), Abc_LitNotCond(iFunc1, pTemp->fCompl1) ); 
        else
            iFunc = Gia_ManAppendAnd( pNew, Abc_LitNotCond(iFunc0, pTemp->fCompl0), Abc_LitNotCond(iFunc1, pTemp->fCompl1) ); 
        if ( pTemp->fPhase != pIfObj->fPhase )
            iFunc = Abc_LitNot(iFunc);
        If_CutSetDataInt( pCut, iFunc );
        break;
    }
    return If_CutDataInt(pCut);
}
int Gia_ManNodeIfToGia( Gia_Man_t * pNew, If_Man_t * pIfMan, If_Obj_t * pIfObj, Vec_Int_t * vLeaves, int fHash )
{
    If_Cut_t * pCut;
    If_Obj_t * pLeaf;
    int i, iRes;
    // get the best cut
    pCut = If_ObjCutBest(pIfObj);
    assert( pCut->nLeaves > 1 );
    // set the leaf variables
    If_CutForEachLeaf( pIfMan, pCut, pLeaf, i )
        If_CutSetDataInt( If_ObjCutBest(pLeaf), Vec_IntEntry(vLeaves, i) );
    // recursively compute the function while collecting visited cuts
    Vec_PtrClear( pIfMan->vTemp );
    iRes = Gia_ManNodeIfToGia_rec( pNew, pIfMan, pIfObj, pIfMan->vTemp, fHash ); 
    if ( iRes == ~0 )
    {
        Abc_Print( -1, "Gia_ManNodeIfToGia(): Computing local AIG has failed.\n" );
        return ~0;
    }
    // clean the cuts
    If_CutForEachLeaf( pIfMan, pCut, pLeaf, i )
        If_CutSetDataInt( If_ObjCutBest(pLeaf), 0 );
    Vec_PtrForEachEntry( If_Cut_t *, pIfMan->vTemp, pCut, i )
        If_CutSetDataInt( pCut, 0 );
    return iRes;
}

/**Function*************************************************************

  Synopsis    [Converts IF into GIA manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline word Gia_ManTt6Stretch( word t, int nVars )
{
    assert( nVars >= 0 );
    if ( nVars == 0 )
        nVars++, t = (t & 0x1) | ((t & 0x1) << 1);
    if ( nVars == 1 )
        nVars++, t = (t & 0x3) | ((t & 0x3) << 2);
    if ( nVars == 2 )
        nVars++, t = (t & 0xF) | ((t & 0xF) << 4);
    if ( nVars == 3 )
        nVars++, t = (t & 0xFF) | ((t & 0xFF) << 8);
    if ( nVars == 4 )
        nVars++, t = (t & 0xFFFF) | ((t & 0xFFFF) << 16);
    if ( nVars == 5 )
        nVars++, t = (t & 0xFFFFFFFF) | ((t & 0xFFFFFFFF) << 32);
    assert( nVars == 6 );
    return t;
}

/**Function*************************************************************

  Synopsis    [Converts IF into GIA manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Gia_ManFromIfLogic( If_Man_t * pIfMan )
{
    Gia_Man_t * pNew;
    If_Cut_t * pCutBest;
    If_Obj_t * pIfObj, * pIfLeaf;
    Vec_Int_t * vMapping, * vMapping2, * vPacking = NULL;
    Vec_Int_t * vLeaves, * vLeaves2, * vCover;
    word Truth = 0, * pTruthTable;
    int i, k, Entry;
    assert( !pIfMan->pPars->fDeriveLuts || pIfMan->pPars->fTruth );
    // start mapping and packing
    vMapping  = Vec_IntStart( If_ManObjNum(pIfMan) );
    vMapping2 = Vec_IntStart( 1 );
    if ( pIfMan->pPars->fDeriveLuts && pIfMan->pPars->pLutStruct )
    {
        vPacking = Vec_IntAlloc( 1000 );
        Vec_IntPush( vPacking, 0 );
    }
    // create new manager
    pNew = Gia_ManStart( If_ManObjNum(pIfMan) );
    // iterate through nodes used in the mapping
    vCover   = Vec_IntAlloc( 1 << 16 );
    vLeaves  = Vec_IntAlloc( 16 );
    vLeaves2 = Vec_IntAlloc( 16 );
    If_ManCleanCutData( pIfMan );
    If_ManForEachObj( pIfMan, pIfObj, i )
    {
        if ( pIfObj->nRefs == 0 && !If_ObjIsTerm(pIfObj) )
            continue;
        if ( If_ObjIsAnd(pIfObj) )
        {
            pCutBest = If_ObjCutBest( pIfObj );
            // collect leaves of the best cut
            Vec_IntClear( vLeaves );
            If_CutForEachLeaf( pIfMan, pCutBest, pIfLeaf, k )
                Vec_IntPush( vLeaves, pIfLeaf->iCopy );
            // perform one of the two types of mapping: with and without structures
            if ( pIfMan->pPars->fDeriveLuts && pIfMan->pPars->fTruth )
            {
                // adjust the truth table
                int nSize = pIfMan->pPars->nLutSize;
                pTruthTable = (word *)If_CutTruth(pCutBest);
                if ( nSize < 6 )
                {
                    Truth = Gia_ManTt6Stretch( *pTruthTable, nSize );
                    pTruthTable = &Truth;
                }
                // perform decomposition of the cut
                pIfObj->iCopy = Gia_ManFromIfLogicNode( pNew, i, vLeaves, vLeaves2, pTruthTable, pIfMan->pPars->pLutStruct, vCover, vMapping, vMapping2, vPacking );
                pIfObj->iCopy = Abc_LitNotCond( pIfObj->iCopy, pCutBest->fCompl );
            }
            else
            {
                pIfObj->iCopy = Gia_ManNodeIfToGia( pNew, pIfMan, pIfObj, vLeaves, 0 );
                // write mapping
                Vec_IntSetEntry( vMapping, Abc_Lit2Var(pIfObj->iCopy), Vec_IntSize(vMapping2) );
                Vec_IntPush( vMapping2, Vec_IntSize(vLeaves) );
                Vec_IntForEachEntry( vLeaves, Entry, k )
                    assert( Abc_Lit2Var(Entry) < Abc_Lit2Var(pIfObj->iCopy) );
                Vec_IntForEachEntry( vLeaves, Entry, k )
                    Vec_IntPush( vMapping2, Abc_Lit2Var(Entry)  );
                Vec_IntPush( vMapping2, Abc_Lit2Var(pIfObj->iCopy) );
            }
        }
        else if ( If_ObjIsCi(pIfObj) )
            pIfObj->iCopy = Gia_ManAppendCi(pNew);
        else if ( If_ObjIsCo(pIfObj) )
            pIfObj->iCopy = Gia_ManAppendCo( pNew, Abc_LitNotCond(If_ObjFanin0(pIfObj)->iCopy, If_ObjFaninC0(pIfObj)) );
        else if ( If_ObjIsConst1(pIfObj) )
        {
            pIfObj->iCopy = 1;
            // create const LUT
            Vec_IntWriteEntry( vMapping, 0, Vec_IntSize(vMapping2) );
            Vec_IntPush( vMapping2, 0 );
            Vec_IntPush( vMapping2, 0 );
        }
        else assert( 0 );
    }
    Vec_IntFree( vCover );
    Vec_IntFree( vLeaves );
    Vec_IntFree( vLeaves2 );
//    printf( "Mapping array size:  IfMan = %d. Gia = %d. Increase = %.2f\n", 
//        If_ManObjNum(pIfMan), Gia_ManObjNum(pNew), 1.0 * Gia_ManObjNum(pNew) / If_ManObjNum(pIfMan) );
    // finish mapping 
    if ( Vec_IntSize(vMapping) > Gia_ManObjNum(pNew) )
        Vec_IntShrink( vMapping, Gia_ManObjNum(pNew) );
    else
        Vec_IntFillExtra( vMapping, Gia_ManObjNum(pNew), 0 );
    assert( Vec_IntSize(vMapping) == Gia_ManObjNum(pNew) );
    Vec_IntForEachEntry( vMapping, Entry, i )
        if ( Entry > 0 )
            Vec_IntAddToEntry( vMapping, i, Gia_ManObjNum(pNew) );
    Vec_IntAppend( vMapping, vMapping2 );
    Vec_IntFree( vMapping2 );
    // attach mapping and packing
    assert( pNew->vMapping == NULL );
    assert( pNew->vPacking == NULL );
    pNew->vMapping = vMapping;
    pNew->vPacking = vPacking;
    // verify that COs have mapping
    {
        Gia_Obj_t * pObj;
        Gia_ManForEachCo( pNew, pObj, i )
           assert( !Gia_ObjIsAnd(Gia_ObjFanin0(pObj)) || Gia_ObjIsLut(pNew, Gia_ObjFaninId0p(pNew, pObj)) );
    }
    return pNew;
}


/**Function*************************************************************

  Synopsis    [Verifies mapping.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManMappingVerify_rec( Gia_Man_t * p, Gia_Obj_t * pObj )
{
    int Id, iFan, k, Result = 1;
    if ( Gia_ObjIsTravIdCurrent(p, pObj) )
        return 1;
    Gia_ObjSetTravIdCurrent(p, pObj);
    if ( !Gia_ObjIsAnd(pObj) )
        return 1;
    if ( !Gia_ObjIsLut(p, Gia_ObjId(p, pObj)) )
    {
        Abc_Print( -1, "Gia_ManMappingVerify: Internal node %d does not have mapping.\n", Gia_ObjId(p, pObj) );
        return 0;
    }
    Id = Gia_ObjId(p, pObj);
    Gia_LutForEachFanin( p, Id, iFan, k )
        if ( Result )
            Result &= Gia_ManMappingVerify_rec( p, Gia_ManObj(p, iFan) );
    return Result;
}
void Gia_ManMappingVerify( Gia_Man_t * p )
{
    Gia_Obj_t * pObj, * pFanin;
    int i, Result = 1;
    assert( Gia_ManHasMapping(p) );
    Gia_ManIncrementTravId( p );
    Gia_ManForEachCo( p, pObj, i )
    {
        pFanin = Gia_ObjFanin0(pObj);
        if ( !Gia_ObjIsAnd(pFanin) )
            continue;
        if ( !Gia_ObjIsLut(p, Gia_ObjId(p, pFanin)) )
        {
            Abc_Print( -1, "Gia_ManMappingVerify: CO driver %d does not have mapping.\n", Gia_ObjId(p, pFanin) );
            Result = 0;
            continue;
        }
        Result &= Gia_ManMappingVerify_rec( p, pFanin );
    }
//    if ( Result && Gia_NtkIsRoot(p) )
//        Abc_Print( 1, "Mapping verified correctly.\n" );
}


/**Function*************************************************************

  Synopsis    [Transfers mapping from hie GIA to normalized GIA.]

  Description [Hie GIA (pGia) points to normalized GIA (p).]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManTransferMapping( Gia_Man_t * pGia, Gia_Man_t * p )
{
    Gia_Obj_t * pObj;
    int i, k, iFan;
    if ( !Gia_ManHasMapping(pGia) )
        return;
    Gia_ManMappingVerify( pGia );
    Vec_IntFreeP( &p->vMapping );
    p->vMapping = Vec_IntAlloc( 2 * Gia_ManObjNum(p) );
    Vec_IntFill( p->vMapping, Gia_ManObjNum(p), 0 );
    Gia_ManForEachLut( pGia, i )
    {
        assert( !Abc_LitIsCompl(Gia_ObjValue(Gia_ManObj(pGia, i))) );
        pObj = Gia_ManObj( p, Abc_Lit2Var(Gia_ObjValue(Gia_ManObj(pGia, i))) );
        Vec_IntWriteEntry( p->vMapping, Gia_ObjId(p, pObj), Vec_IntSize(p->vMapping) );
        Vec_IntPush( p->vMapping, Gia_ObjLutSize(pGia, i) );
        Gia_LutForEachFanin( pGia, i, iFan, k )
            Vec_IntPush( p->vMapping, Abc_Lit2Var(Gia_ObjValue(Gia_ManObj(pGia, iFan))) );
        Vec_IntPush( p->vMapping, Gia_ObjId(p, pObj) );
    }
    Gia_ManMappingVerify( p );
}

/**Function*************************************************************

  Synopsis    [Transfers packing from hie GIA to normalized GIA.]

  Description [Hie GIA (pGia) points to normalized GIA (p).]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManTransferPacking( Gia_Man_t * pGia, Gia_Man_t * p )
{
    Vec_Int_t * vPackingNew;
    Gia_Obj_t * pObj, * pObjNew;
    int i, k, Entry, nEntries, nEntries2;
    if ( pGia->vPacking == NULL )
        return;
    nEntries = Vec_IntEntry( pGia->vPacking, 0 );
    nEntries2 = 0;
    // create new packing info
    vPackingNew = Vec_IntAlloc( Vec_IntSize(pGia->vPacking) );
    Vec_IntPush( vPackingNew, nEntries );
    Vec_IntForEachEntryStart( pGia->vPacking, Entry, i, 1 )
    {
        assert( Entry > 0 && Entry < 4 );
        Vec_IntPush( vPackingNew, Entry );
        i++;
        for ( k = 0; k < Entry; k++, i++ )
        {
            pObj = Gia_ManObj(pGia, Vec_IntEntry(pGia->vPacking, i));
            pObjNew = Gia_ManObj(p, Abc_Lit2Var(Gia_ObjValue(pObj)));
            assert( Gia_ObjIsLut(pGia, Gia_ObjId(pGia, pObj)) );
            assert( Gia_ObjIsLut(p, Gia_ObjId(p, pObjNew)) );
            Vec_IntPush( vPackingNew, Gia_ObjId(p, pObjNew) );
//            printf( "%d -> %d  ", Vec_IntEntry(pGia->vPacking, i), Gia_ObjId(p, pObjNew) );
        }
        i--;
        nEntries2++;
    }
    assert( nEntries == nEntries2 );
    // attach packing info
    assert( p->vPacking == NULL );
    p->vPacking = vPackingNew;
}


/**Function*************************************************************

  Synopsis    [Interface of LUT mapping package.]

  Description []
               
  SideEffects []

  SeeAlso     [] 

***********************************************************************/
Gia_Man_t * Gia_ManPerformMapping( Gia_Man_t * p, void * pp, int fNormalized )
{
    Gia_Man_t * pNew;
    If_Man_t * pIfMan;
    If_Par_t * pPars = (If_Par_t *)pp;
    // disable cut minimization when GIA strucure is needed
    if ( !pPars->fDelayOpt && !pPars->fUserRecLib && !pPars->fDeriveLuts )
        pPars->fCutMin = 0;
    // reconstruct GIA according to the hierarchy manager
    assert( pPars->pTimesArr == NULL );
    assert( pPars->pTimesReq == NULL );
    if ( p->pManTime )
    {
        if ( fNormalized )
        {
            pNew = Gia_ManDupUnnormalize( p );
            if ( pNew == NULL )
                return NULL;
            pNew->pManTime   = p->pManTime;   p->pManTime  = NULL;
            pNew->pAigExtra  = p->pAigExtra;  p->pAigExtra = NULL;
            pNew->nAnd2Delay = p->nAnd2Delay; p->nAnd2Delay = 0;
            p = pNew;
            // set arrival and required times
            pPars->pTimesArr = Tim_ManGetArrTimes( (Tim_Man_t *)p->pManTime );
            pPars->pTimesReq = Tim_ManGetReqTimes( (Tim_Man_t *)p->pManTime );
        }
    }
    else 
        p = Gia_ManDup( p );
    // translate into the mapper
    pIfMan = Gia_ManToIf( p, pPars );    
    if ( pIfMan == NULL )
    {
        Gia_ManStop( p );
        return NULL;
    }
    if ( p->pManTime )
        pIfMan->pManTim = Tim_ManDup( (Tim_Man_t *)p->pManTime, 0 );
    if ( !If_ManPerformMapping( pIfMan ) )
    {
        If_ManStop( pIfMan );
        Gia_ManStop( p );
        return NULL;
    }
    // transform the result of mapping into the new network
    if ( pIfMan->pPars->fDelayOpt || pIfMan->pPars->fUserRecLib )
        pNew = Gia_ManFromIfAig( pIfMan );
    else
        pNew = Gia_ManFromIfLogic( pIfMan );
    If_ManStop( pIfMan );
    // transfer name
    assert( pNew->pName == NULL );
    pNew->pName = Abc_UtilStrsav( p->pName );
    pNew->pSpec = Abc_UtilStrsav( p->pSpec );
    Gia_ManSetRegNum( pNew, Gia_ManRegNum(p) );
    // return the original (unmodified by the mapper) timing manager
    pNew->pManTime   = p->pManTime;   p->pManTime   = NULL;
    pNew->pAigExtra  = p->pAigExtra;  p->pAigExtra  = NULL;
    pNew->nAnd2Delay = p->nAnd2Delay; p->nAnd2Delay = 0;
    Gia_ManStop( p );
    // normalize and transfer mapping
    pNew = Gia_ManDupNormalize( p = pNew );
    Gia_ManTransferMapping( p, pNew );
    Gia_ManTransferPacking( p, pNew );
    pNew->pManTime   = p->pManTime;   p->pManTime   = NULL;
    pNew->pAigExtra  = p->pAigExtra;  p->pAigExtra  = NULL;
    pNew->nAnd2Delay = p->nAnd2Delay; p->nAnd2Delay = 0;
    Gia_ManStop( p );
//    printf( "PERFORMING VERIFICATION:\n" );
//    Gia_ManVerifyWithBoxes( pNew, NULL );
    return pNew;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

