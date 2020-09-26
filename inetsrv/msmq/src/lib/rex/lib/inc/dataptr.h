/****************************************************************************/
/*  File:       dataptr.h                                                  */
/*  Author:     J. Kanze                                                    */
/*  Date:       28/05/1996                                                  */
/*      Copyright (c) 1996 James Kanze                                      */
/* ------------------------------------------------------------------------ */
/*  Modified:   14/02/2000  J. Kanze                                        */
/*      Ajout� operator() pour compatibilit� avec STL. (En fait, selon      */
/*      la norme, std::less doit faire l'affaire. Mais jusqu'� ce que       */
/*      tous les compilateurs soient � jour...)                             */
/* ------------------------------------------------------------------------ */
//      dataptr :
//      =========
//
//      <lang=french>
//      Un ensemble de fonctions pour faire des choses autrement
//      non-portables avec des pointeurs. (Une partie des ces
//      fonctions ont �t� perim�es par des evolutions dans la norme,
//      mais d'ici � ce que les compilateurs sont tous � jour...)
//
//      Ici, il y a les fonctions pour les pointeurs � des donn�es.
//
//      compare :       d�finit une fonction (relationship) d'ordre
//                      sur les pointeurs, ou qu'ils pointent. (On se
//                      souviendrait que les op�rateurs de comparison
//                      d'inegalit� ne sont d�finis que si les deux
//                      pointeurs point � l'int�rieur d'un m�me
//                      objet.) La fonction d'ordre ainsi d�finie est
//                      completement arbitraire.
//
//                      La fonction retourne une valeur n�gative,
//                      z�ro, ou positive, selon que le premier
//                      pointeur se trouve avant, est le m�me que, ou
//                      se trouve apr�s le second.
//
//      isLessThan :    Une forme simplifi�e du pr�cedant, impl�mente
//                      p1 < p2, en se servant de la fonction d'ordre
//                      d�finie par compare.
//
//                      Correspond � less<T*> dans la norme.
//
//                      L'op�rateur() est un alias pour cette
//                      fonction, afin que la classe puisse servir
//                      directement comme objet de comparison d'une
//                      collection associative.
//
//      hash :          g�n�re un code de hachage sur le pointeur. Si
//                      deux pointeurs pointent au m�me endroit, il
//                      est garanti qu'ils se hacheraient � la m�me
//                      valeur (m�me s'ils ont de r�pr�sentations
//                      diff�rentes). S'ils pointent aux endroits
//                      diff�rents, il y a de fortes chances (mais
//                      pas de garantie) qu'ils auront de valeurs de
//                      hachage diff�rentes.
//
//      asString :      convertit un pointeur dans une cha�ne de
//                      caract�res alphanum�rique. Cette fonction
//                      retourne un pointeur � la cha�ne g�n�r�e.
//
//                      ATTENTION : la cha�ne g�n�r�e peut se trouver
//                      � une adresse fixe, et pourrait �tre �crass�e
//                      par un appel nouveau de la fonction.
// ---------------------------------------------------------------------------
//      <lang=english>
//      A set of functions for things that are otherwise not portable
//      with pointers.  (Most of these functions are actually no
//      longer needed with the new standard, but until all compilers
//      and libraries are up to date...)
//
//      In this file, we define the operations on pointers to data.
//
//      compare:        Defines an ordering relationship over the
//                      pointers, including between pointers which do
//                      not point into the same object.  The ordering
//                      relationship is totally arbitrary, however.
//
//                      This function returns a negative, zero or
//                      positive value, according to whether the first
//                      pointer is before, equal to, or after the
//                      second.
//
//      isLessThan:     A simplified form of the preceding,
//                      implementing p1 < p2, by means of the compare
//                      function.
//
//                      Corresponds to less< T* > in the standard.
//
//                      The operator< is an alias for this function,
//                      so that this class can be used directly as a
//                      comparison object for an associative
//                      container.
//
//      hash:           Generates a hash code for the pointer. If two
//                      pointers designate the same object, this
//                      function is guaranteed to return the same
//                      value for them, even if they have different
//                      representations.  If they designate different
//                      objects, it is highly likely (but not
//                      guaranteed) that the return values will be
//                      different.
//
//      asString:       Converts a pointer to a string of
//                      alphanumerical characters.  This function
//                      returns a pointer to the generated string.
//
//                      ATTENTION: the generated string is in static
//                      memory, and will be overwritten on each
//                      invocation of the function.
// ---------------------------------------------------------------------------

#ifndef REX_DATAPTR_HH
#define REX_DATAPTR_HH

#include <inc/global.h>

struct CRexDataPointers
{
    static int          compare( void const* p1 , void const* p2 ) ;
    static bool         isLessThan( void const* p1 , void const* p2 ) ;
    static unsigned int hashCode( void const* p ) ;
    static char const*  asString( void const* p ) ;

    bool                operator()( void const* p1 , void const* p2 ) ;
} ;

inline bool
CRexDataPointers::operator()( void const* p1 , void const* p2 )
{
    return isLessThan( p1 , p2 ) ;
}
#endif
//  Local Variables:    --- for emacs
//  mode: c++           --- for emacs
//  tab-width: 8        --- for emacs
//  End:                --- for emacs
