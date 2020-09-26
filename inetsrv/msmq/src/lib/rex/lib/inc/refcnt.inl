/****************************************************************************/
/*  File:       refcnt.ihh                                                  */
/*  Author:     J. Kanze                                                    */
/*  Date:       28/03/1996                                                  */
/*      Copyright (c) 1996 James Kanze                                      */
/* ------------------------------------------------------------------------ */

#include <inc/dataptr.h>
#include <stdio.h>

//      CRexRefCntObj :
// ==========================================================================

inline unsigned
CRexRefCntObj::useCount() const
{
    return myUseCount.value() ;
}

inline void
CRexRefCntObj::incrUse()
{
    myUseCount.incr() ;
}

inline void
CRexRefCntObj::decrUse()
{
    myUseCount.decr() ;
    if ( myUseCount.value() == 0 ) {
	delete this ;
    }
}

inline
CRexRefCntObj::CRexRefCntObj()
{
}

inline
CRexRefCntObj::~CRexRefCntObj()
{
}

//      CRexRefCntPtr :
//      ==============
//
//      On d�fie de l'ordre de la d�claration afin que les fonctions
//      soit d�finie avant l'utilisation.  En particulier, puisque
//      "get" et "isValid" servent � peu pr�s partout, elles sont
//      d�finie en premi�re.
// ==========================================================================

template< class T >
inline bool
CRexRefCntPtr< T >::isValid() const
{
    return myPtr != NULL ;
}

template< class T >
inline T*
CRexRefCntPtr< T >::get() const
{
    return myPtr ;
}




template< class T >
inline
CRexRefCntPtr< T >::~CRexRefCntPtr()
{
    if ( isValid() ) {
	myPtr->decrUse() ;
    }
}

template< class T >
inline CRexRefCntPtr< T >&
CRexRefCntPtr< T >::operator=( T* newedPtr )
{
    if ( newedPtr != myPtr ) {
	if ( isValid() ) {
	    myPtr->decrUse() ;
        }
	myPtr = newedPtr ;
	if ( isValid() ) {
            //  On utilise l'affectation avec la conversion implicite pour
            //  provoquer une erreur de compilation si T ne d�rive pas de
            //  CRexRefCntObj. M�me l'optimisation la plus primitive doit
            //  pouvoir l'�liminer.
            CRexRefCntObj*       tmp = newedPtr ;
	    tmp->incrUse() ;
        }
    }
    return *this ;
}



template< class T >
inline unsigned
CRexRefCntPtr< T >::count() const
{
    return (! isValid())
        ? 0
        : myPtr->useCount() ;
}

template< class T >
inline T*
CRexRefCntPtr< T >::operator->() const
{
    ASSERT( isValid() ); //Attempt to dereference null (reference counted) pointer
    return get() ;
}

template< class T >
inline T&
CRexRefCntPtr< T >::operator*() const
{
    ASSERT( isValid() ); //Attempt to dereference null (reference counted) pointer
    return *get() ;
}

template< class T >
inline unsigned
CRexRefCntPtr< T >::hashCode() const
{
    return CRexDataPointers::hashCode( get() ) ;
}

template< class T >
inline int
CRexRefCntPtr< T >::compare( CRexRefCntPtr< T > const& other ) const
{
    return CRexDataPointers::compare( get() , other.get() ) ;
}
//  Local Variables:    --- for emacs
//  mode: c++           --- for emacs
//  tab-width: 8        --- for emacs
//  End:                --- for emacs
