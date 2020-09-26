/****************************************************************************/
/*  File:       refcnt.h                                                   */
/*  Author:     J. Kanze                                                    */
/*  Date:       22/10/93                                                    */
/*      Copyright (c) 1993,1994 James Kanze                                 */
/* ------------------------------------------------------------------------ */
/*  Modified:   18/05/94    J. Kanze                                        */
/*      Converti aux templates.                                             */
/*  Modified:   25/10/94    J. Kanze                                        */
/*      Actualise en ce qui concerne les conventions de nomage.             */
/*  Modified:   28/03/1996  J. Kanze                                        */
/*      Retravailler selon les id�es dans `More Effective C++' (merci,      */
/*      Scott).                                                             */
/*  Modified:   07/02/2000  J. Kanze                                        */
/*      Fait marcher les template membres : CRexRefCntObj ne peut pas        */
/*      �tre un template. On perd un peu de s�curit�, mais la               */
/*      flexibilit� suppl�mentaire en vaut la peine.                        */
/* ------------------------------------------------------------------------ */
//      RefCnt :
//      ===========
//
//      <lang=french>
//      Une classe g�n�rique pour la gestion des objets compt�s.
//
//      Afin d'�tre g�r� par cette classe, un objet doit
//      obligatoirement d�rive de CRexRefCntObj ; en plus, cette
//      d�rivation ne doit pas �tre virtuelle.  Aussi (�videmment),
//      l'objet en question doit �tre allou� dynamiquement, avec un
//      new sans placement.  Pour meiux s'assurer de ces contraints,
//      il est fortement conseill� d'encapsuler RefCntPtr dans un
//      classe manipulateuse (handle) associ�e � la classe (ou � la
//      hi�rarchie des classes) cibl�e.
//
//      Noter bien qu'il n'y a rien qui emp�che qu'une classe d�riv�e
//      de CRexRefCntObj soit instanci�e sur la pile ou statiquement,
//      mais un tel objet ne doit jamais servir � initialiser un
//      CRexRefCntPtr.
//
//      CRexRefCntObj contient un constructeur noInit pour les cas
//      particulier.  Ce constructeur ne doit servir que pour les
//      objets statique, o� l'initialisation � 0 au depart garantit
//      une valeur correcte dans le compteur.  C'est en fait de
//      l'histoire ; aujourd'hui je connais de meillieur moyens pour
//      obtenir les m�mes fins.
//
//      CRexRefCntObj n'a pas de destructeur virtuel.  Ainsi, on peut
//      d�river de CRexRefCntObj sans encourir les frais d'un pointeur
//      � la table des fonctions virtuelles.  Mais...  il ne faut pas
//      non plus appeler delete avec un pointeur � un CRexRefCntObj.
//      (Normallement, cela ne doit pas se produire, puisque c'est
//      CRexRefCntPtr qui doit appeler delete, et pas l'utilisateur.)
//      L'intention est que l'utilisateur peut (et doit) ignorer
//      compl�tement l'existance du CRexRefCntObj, une fois qu'il l'a
//      d�clar� comme classe de base.  En particulier, il n'est pas
//      pr�vu que l'utilisateur ait des pointeurs � un CRexRefCntObj �
//      lui.  Il n'y a que CRexRefCntPtr qui doit s'en charger.
// --------------------------------------------------------------------------
//      <lang=english>
//      The following defines a generic class for handling reference
//      counted objects.
//
//      To use reference counting, the reference counted class must
//      derive (not virtually) from CRexRefCntObj.  Also, the object
//      must be on the heap.  To ensure these invariants, it is
//      recommended that the RefCntPtr be encapsulated in a handle
//      class for the target type.  (In other words, this class is not
//      intended to be used at the application level, but rather in
//      the implementation of handle classes for the application.)
//
//      Note that an object deriving from CRexRefCntObj can be
//      constructed on the stack or as a static, but such an object
//      should *not* be used to initialize a CRexRefCntPtr.
//
//      A "no init" constructor for CRexRefCntObj is available for
//      special cases.
//
//      CRexRefCntObj does *not* have a virtual destructor!  This means
//      that classes can derive from CRexRefCntObj without necessarily
//      incuring the cost of a virtual function table pointer.  But...
//      it also means that deleting directly through a pointer to a
//      CRexRefCntObj will *not* work.
//
//      It is the intent that the user ignore completely CRexRefCntObj,
//      except for declaring it as a base class.  Generally, the user
//      should *not* maintain his own pointers to CRexRefCntObj; all
//      pointers to a reference counted class should be
//      CRexRefCntPtr's.
// --------------------------------------------------------------------------

#ifndef REX_REFCNT_HH
#define REX_REFCNT_HH


#include    <inc/global.h>
#include    <inc/counter.h>

template< class T > class CRexRefCntPtr ;

// ==========================================================================
//      CRexRefCntObj :
//      ==============
//
//      <lang=french>
//      Tout objet g�r� par CRexRefCntPtr doit obligatoirement d�rive
//      de cette classe.
//
//      Un CRexRefCntObj ne peut �tre ni assign�, ni copi�.  La plupart
//      du temps, un essai de assigner ou de copier un tel objet
//      resulte d'une erreur de programmation.  (Enfin, le but de la
//      manip, c'est de pouvoir utiliser une semantique de r�f�rence
//      plut�t qu'une semantique de copie.)  Donc, on l'interdit.
//      Dans le cas exceptionel o� une copie peut �tre d�sirable
//      (e.g.: une fonction de clone), la classe d�riv�e a toujours la
//      libert� de d�finir ces propres fonctions de assignement et de
//      copie (un constructeur).  Dans ce cas, ces fonctions doivent
//      s'�crire comme si CRexRefCntObj n'y �tait pas ; le compteur de
//      r�f�rences se trouvant ainsi soit initialis� � z�ro (copie),
//      soit inchang� (assignement).
//
//
//      <lang=english>
//      All objects which are to be managed by a CRexRefCntPtr must
//      have this class as a base.
//
//      CRexRefCntObj is unassignable (and uncopiable).  In most cases,
//      copying or assigning a reference counted object is an error,
//      so we forbid it.  In the exceptional cases where it might make
//      sense, the derived class can always provide an overriding copy
//      constructor or assignment operator.  In such cases: the copy
//      constructor and assignment operator should be written as
//      though the class did not derive from CRexRefCntObj.
// --------------------------------------------------------------------------

class CRexRefCntObj
{
public:
    unsigned            useCount() const ;

    void                incrUse() ;
    void                decrUse() ;

protected:
    //      Constructeurs, destructeurs et operateurs d'assignement :
    //      =========================================================
    //
    //      <lang=french>
    //      CRexRefCntObj a les constructeurs suivant :
    //
    //      le defaut :         initialise le compteur de r�f�rences �
    //                          z�ro.
    //
    //      Il n'y a ni de constructeur de copie, ni d'op�rateur
    //      d'assignement (voir ci-dessus).
    //
    //      Le destructeur est prot�g�, afin que l'utilisateur n'a
    //      m�me pas la possibilit� d'appeler delete sur un pointeur �
    //      un CRexRefCntObj.
    //
    //
    //      <lang=english>
    //      CRexRefCntObj has the following constructors:
    //
    //      default:            Initializes the reference count to
    //                          zero.
    //
    //      Copy construction and assignment are *not* supported, see
    //      above.
    // ----------------------------------------------------------------------
                        CRexRefCntObj() ;
    virtual             ~CRexRefCntObj() ;

private :
                        CRexRefCntObj( CRexRefCntObj const& other ) ;
    CRexRefCntObj&       operator=( CRexRefCntObj const& other ) ;

    CRexCounter< unsigned >
                        myUseCount ;
} ;

// ==========================================================================
//      CRexRefCntPtr :
//      ==============
//
//      <lang=french>
//      Une classe g�n�rique, qui pointe � un objet compt� d'un type
//      d�riv� de CRexRefCntPtr.
//
//      Il y a deux raisons pour qu'il pointe au type d�riv�, plut�t
//      qu'au CRexRefCntObj m�me :
//
//      1.  S�curit� de type.  Un CRexRefCntPtr d'un type donn� ne peut
//          r�f�re qu'� un objet de ce type, ou d'un type d�riv� de ce
//          type.
//
//      2.  Simplicit� des types d�riv�s de CRexRefCntObj.  Si
//          CRexRefCntPtr n'�tait pas g�n�rique, et ne savait pas le
//          type de l'objet auquel il r�f�rait, il faudrait que
//          CRexRefCntObj ait un destructeur virtuel.  Dans
//          l'implementation ici, REX_REfCntObj n'a aucune fonction
//          virtuelle, et donc il n'impose pas de fonctions virtuelles
//          aux classes d�riv�es.
//
//
//      <lang=english>
//      A generic class which points to an object of a type derived
//      from CRexRefCntObj.
//
//      There are two reasons for doing this, rather than simply
//      having a non-template class pointing to CRexRefCntObj:
//
//      1.  Type safety.  A CRexRefCntPtr of one type cannot point to
//          an object of another type.
//
//      2.  Simplicity of the class derived from CRexRefCntObj.  If
//          CRexRefCntPtr were not generic (and thus didn't know the
//          actual type of what it was pointing to), CRexRefCntObj
//          would have to have a virtual destructor.  In the present
//          implementation, CRexRefCntObj has *no* virtual functions,
//          and so does not impose virtual functions on the derived
//          class.
// --------------------------------------------------------------------------

template< class T >
class CRexRefCntPtr
{
public :
    //      Constructeurs, destructeurs et operateurs d'assignement :
    //      =========================================================
    //
    //      <lang=french>
    //      Construction par copie et assignement sont pourvus.  En
    //      plus, il est possible d'assigner un T* directement.  (Dans
    //      ce cas, attention : le T* doit obligatoirement provenir
    //      d'une expression de new.)
    //
    //
    //      <lang=english>
    //      There is no default constructor; a CRexRefCntPtr must
    //      always be initialized to point to a T.
    //
    //      Copy construction and assignment are supported.  In
    //      addition to being able to assign another CRexRefCntPtr to a
    //      CRexRefCntPtr, it is possible to assign a T* directly.
    // ----------------------------------------------------------------------
    template< class D > CRexRefCntPtr( D* newedPtr )
        :   myPtr( newedPtr )
    {
        if ( isValid() ) {
            //  On utilise l'affectation avec la conversion implicite pour
            //  provoquer une erreur de compilation si T ne d�rive pas de
            //  CRexRefCntObj. M�me l'optimisation la plus primitive doit
            //  pouvoir l'�liminer.
            CRexRefCntObj*       tmp = newedPtr ;
            tmp->incrUse() ;
        }
    }

    template< class D > CRexRefCntPtr( const CRexRefCntPtr< D >& other )
        :   myPtr( other.get() )
    {
        if ( isValid() ) {
    	myPtr->incrUse() ;
        }
    }

    explicit CRexRefCntPtr( T* newedPtr  = 0)
        :   myPtr( newedPtr )
    {
        if ( isValid() ) {
            //  On utilise l'affectation avec la conversion implicite pour
            //  provoquer une erreur de compilation si T ne d�rive pas de
            //  CRexRefCntObj. M�me l'optimisation la plus primitive doit
            //  pouvoir l'�liminer.
            CRexRefCntObj*       tmp = newedPtr ;
            tmp->incrUse() ;
       }
    }

    CRexRefCntPtr( const CRexRefCntPtr& other )
          :   myPtr( other.get() )
    {
        if ( isValid() ) {
 	    myPtr->incrUse() ;
        }
    }


                        ~CRexRefCntPtr() ;

    CRexRefCntPtr< T >&  operator=( T* newedPtr ) ;

    template< class D >
    CRexRefCntPtr< T >&  operator=( CRexRefCntPtr< D > const& other )
    {
        return operator=( other.get() ) ;
    }

    CRexRefCntPtr< T >& operator=( CRexRefCntPtr const& other )
    {
        return operator=( other.get() ) ;
    }


    //      isValid :
    // ----------------------------------------------------------------------
    bool                isValid() const ;

    //      get :
    //      =====
    //
    //      Cette fonction sert � obtenir un T* � l'�tat brut.  En
    //      g�n�ral, elle est fortement d�conseill�, vu les dangers
    //      qu'elle pr�sente.  En effet, le pointeur qui en resulte
    //      n'�tant pas g�r� par la classe, l'objet auquel il r�f�re
    //      peut ainsi cesser d'exister d'une fa�on inopportune, avec
    //      des resultats g�n�ralement d�sagr�ables.
    // ----------------------------------------------------------------------
    T*                  get() const ;

    //      count :
    //      =======
    //
    //      Retourne le nombre de pointeurs qui refere au meme objet.
    //      Typiquement, cette fonction sert a implementer les strategies de
    //      "copy on write" ; elle sera appelee avant la modification, et si
    //      elle retourne une valeur superieur a un, l'appelant fera une
    //      copie profonde avant d'effectuer la modification. Ex. :
    //
    //          if ( ptr.count() > 1 ) {
    //              ptr = ptr->clone() ;
    //          }
    //
    //      (Ce suppose que l'objet en question a une fonction clone qui
    //      retourne une copie de l'objet.)
    // ----------------------------------------------------------------------
    unsigned            count() const ;

    //      Op�rateurs d'acc�s :
    //      ====================
    //
    //      <lang=french>
    //      Ces op�rateurs sont identiques aux m�mes op�rateurs sur
    //      des pointeurs ; ils r�pr�sentent la fa�on habituelle
    //      d'utiliser des CRexRefCntPtr.
    //
    //      <lang=english>
    //      These operators simulate the same operations on pointers,
    //      and are the normal way of using CRexRefCntPtr's.
    // ----------------------------------------------------------------------
    T*                  operator->() const ;
    T&                  operator*() const ;

    unsigned            hashCode() const ;
    int                 compare( CRexRefCntPtr< T > const& other ) const ;
private :
    T*                  myPtr ;
} ;

#include <inc/refcnt.inl>
#endif
//  Local Variables:    --- for emacs
//  mode: c++           --- for emacs
//  tab-width: 8        --- for emacs
//  End:                --- for emacs
