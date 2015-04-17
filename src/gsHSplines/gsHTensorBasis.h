/** @file gsHTensorBasis.h

    @brief Provides definition of HTensorBasis abstract interface.

    This file is part of the G+Smo library.
    
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

    Author(s): G. Kiss, A. Mantzaflaris, J. Speh
*/

#pragma once

#include <gsCore/gsBasis.h>

#include <gsHSplines/gsHDomain.h>
#include <gsHSplines/gsHDomainIterator.h>
#include <gsHSplines/gsHDomainBoundaryIterator.h>

#include <gsCore/gsBoundary.h>

#include <gsNurbs/gsCompactKnotVector.h>
#include <gsNurbs/gsTensorBSplineBasis.h>
#include <gsNurbs/gsBSplineBasis.h> // for gsBasis::component(int)

#include <limits>

namespace gismo
{

struct lvl_coef
{
    int pos;
    int unsigned lvl;
    double coef;
};


/**
* @brief Class representing a (scalar) hierarchical tensor
* basis of functions \f$ \mathbb R^d \to \mathbb R \f$.
*
*
* The principal idea for constructing the hierarchical basis is as follows
* (in simplified version):
*
* 1. Take a sequence of simple tensor-product bases \f$B^0,\ B^1,\ldots, B^L\f$.
* Each of these bases \f$ B^\ell \f$ defines a <em>level</em> \f$\ell\f$ of the hierarchy.
* Note that we assume that \f$ B^{k+1} \f$ is always a "finer" basis than \f$ B^k \f$.
* 2. From each of these basis \f$ B^\ell \f$, select a set of basis functions in a very smart way.
* This gives you a set of basis functions \f$S^\ell \subseteq B^\ell \f$ of level \f$\ell\f$.
* 3. Take the union of these sets \f$ H = \bigcup_{\ell = 0,\ldots,L} S^\ell \f$.
* This is your hierarchical basis \f$ H \f$ (assuming that you selected
* the sets of functions \f$ S^\ell \f$ in a smart and appropriate way).
*
* <em>Remark on the numbering of the basis functions of \f$ H \f$:</em>
*
* The functions in \f$ H \f$ have global indices \f$0, \ldots, N\f$.
* The numbering is sorted by levels in the following sense. Let \f$n^\ell\f$ be the number of basis functions
* selected from level \f$\ell\f$ (i.e., \f$ n^\ell = | S^\ell |\f$), then the global
* indices \f$0,\ldots,n^0-1\f$ correspond to functions which are taken from \f$ B^0 \f$,
* indices \f$ n^0,\ldots,n^0+n^1\f$ to functions from \f$B^1\f$ and so forth.
*
*
*    Template parameters
*    \param d is the domain dimension
*    \param T is the coefficient type
*
*    \ingroup basis
*    \ingroup HSplines
*/

template<unsigned d, class T>
class gsHTensorBasis: public gsBasis<T>
{

public:
    /// Shared pointer for gsHTensorBasis
    typedef memory::shared_ptr< gsHTensorBasis > Ptr;
    
    typedef T Scalar_t;

    typedef gsHDomain<d> hdomain_type;

    typedef typename hdomain_type::point point; 

    typedef typename hdomain_type::box   box;
    
    typedef std::vector< box > boxHistory;

    typedef gsSortedVector< unsigned > CMatrix; // charMatrix_

    typedef gsTensorBSplineBasis<d,T,gsCompactKnotVector<T> > tensorBasis;

    /// Dimension of the parameter domain
    static const int Dim = d;
    
public:
    /// Default empty constructor
    gsHTensorBasis() { }

    gsHTensorBasis( gsBasis<T> const&  tbasis, int nlevels)
    {      
        GISMO_ASSERT( nlevels > 0, "Invalid number of levels." );
        initialize_class(tbasis);
        // Build the characteristic matrices
        update_structure();
    }

    gsHTensorBasis( gsTensorBSplineBasis<d,T> const&  tbasis, 
                    int nlevels, std::vector<unsigned> boxes)
    {
        GISMO_ASSERT( nlevels > 0, "Invalid number of levels." );
        initialize_class(tbasis);
        gsVector<unsigned int> i1;
        gsVector<unsigned int> i2;
        i1.resize(d);
        i2.resize(d);
        // Set all functions to active
        GISMO_ASSERT( (boxes.size()%(2*d+1))==0,
        "The points did not define boxes properly. The basis was created without any domain structure.");

        for( unsigned i = 0; i < (boxes.size()/(2*d+1)); i++)
        {
            for( unsigned int j = 0; j < d; j++)
            {
                i1[j] = boxes[(2*d+1)*i+j+1];
                i2[j] = boxes[(2*d+1)*i+j+d+1];
            }
            insert_box(i1,i2,boxes[i*(2*d+1)]);
        }

        // Build the characteristic matrices (note: call is non-vritual)
        update_structure();
    }

/**
 * @brief gsHTensorBasis
 * @param tbasis - tensor basis
 * @param nlevels - number of levels
 * @param boxes - matrix containing boxes - each 2x2 submatrix
 * contains the lover left and upper right corner of the box
 *
 * - the level where the box should be inserted is one higher as the
 *                 level where it is completely contained
 */
    gsHTensorBasis( gsTensorBSplineBasis<d,T> const&  tbasis, 
                    int nlevels, gsMatrix<T> const & boxes)
    {
        //assert(boxes.rows() == 2);    //can accept only 2D coordinates- Delete during the generalization of the lac to nD
        GISMO_ASSERT(boxes.rows() == d, "Points in boxes need to be of dimension d.");
        GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provied gsHTensorBasis constructor with them.");
        GISMO_ASSERT( nlevels > 0, "Invalid number of levels." );
        initialize_class(tbasis);
        
        gsVector<unsigned int,d> k1;
        gsVector<unsigned int,d> k2;
        
        for(index_t i = 0; i < boxes.cols()/2; i++)
        {
            for(unsigned j = 0; j < d; j++)
            {
                k1[j] = this->m_bases.back()->knots(j).Uniquefindspan(boxes(j,2*i));
                k2[j] = this->m_bases.back()->knots(j).Uniquefindspan(boxes(j,2*i+1))+1;
            }
            int level = m_tree.query3(k1,k2,m_bases.size()-1);
            for(unsigned j = 0; j < d; j++)
            {
                k1[j] = this->m_bases[level+1]->knots(j).Uniquefindspan(boxes(j,2*i));
                k2[j] = this->m_bases[level+1]->knots(j).Uniquefindspan(boxes(j,2*i+1))+1;
            }

            insert_box(k1,k2,level+1);

            // Build the characteristic matrices (note: call is non-vritual)
            update_structure();

        }
}

/**
 * @brief gsHTensorBasis
 * @param tbasis
 * @param nlevels - maximum number of levels
 * @param boxes - matrix containing boxes - eaxh 2x2 submatrix
 * contains the lover left and upper right corner of the box
 * @param levels
 */
    gsHTensorBasis( gsTensorBSplineBasis<d,T> const&  tbasis, int nlevels, 
                    gsMatrix<T> const & boxes, 
                    std::vector<unsigned int> levels)
    {
        GISMO_ASSERT(boxes.rows() == d, "Points in boxes need to be of dimension d.");
        GISMO_ASSERT(boxes.cols()%2 == 0, "Each box needs two corners but you don't provied gsHTensorBasis constructor with them.");
        GISMO_ASSERT(unsigned (boxes.cols()/2) <= levels.size(), "We don't have enough levels for the boxes.");

        initialize_class(tbasis);

        gsVector<unsigned,d> k1;
        gsVector<unsigned,d> k2;

        const size_t mLevel = *std::max_element(levels.begin(), levels.end() );
        needLevel( mLevel );
        
        for(index_t i = 0; i < boxes.cols()/2; i++)
        {
            for(unsigned j = 0; j < d; j++)
            {
                k1[j] = m_bases[levels[i]]->knots(j).Uniquefindspan(boxes(j,2*i));
                k2[j] = m_bases[levels[i]]->knots(j).Uniquefindspan(boxes(j,2*i+1))+1;
            }

            /* m_boxHistory.push_back( box(k1,k2,levels[i]) );  */                      
            this->m_tree.insertBox(k1,k2, levels[i]);
            
            // Build the characteristic matrices (note: call is non-vritual)
            update_structure();
        }
    }
    
    /// Copy constructor
    gsHTensorBasis( const gsHTensorBasis & o) : gsBasis<T>(o)
    {
        //max_size         = o.max_size;
        m_xmatrix_offset = o.m_xmatrix_offset;
        m_deg            = o.m_deg;
        m_tree           = o.m_tree;
        m_xmatrix        = o.m_xmatrix;
        
        m_bases.reserve( m_bases.size() );
        for( typename std::vector<gsTensorBSplineBasis<d,T,gsCompactKnotVector<T> >* > 
                 ::const_iterator it = 
                 o.m_bases.begin(); it != o.m_bases.end(); ++it )
            m_bases.push_back( (*it)->clone() );
    }
    
    /// Destructor
    virtual ~gsHTensorBasis() 
    { 
        freeAll( m_bases );
    }

protected:

    // TO DO: remove these members after they are not used anymore
    std::vector<int> m_deg;

protected:

    /// \brief The list of nestes spaces.
    ///
    /// See documentation for the class for details on the underlying
    /// structure.
    ///
    /// Recall that the hierarchical basis is built from
    /// a sequence of underlying bases \f$ B^0, B^1,\ldots, B^L\f$.
    /// These underlying bases are stored in gsHTensorBasis.m_bases,
    /// which is of type std::vector.\n
    /// <em>m_bases[k]</em> stores the pointer to the
    /// (global) tensor-product basis \f$ B^k\f$.
    mutable std::vector<gsTensorBSplineBasis<d,T,gsCompactKnotVector<T> >* > m_bases;

    /// \brief The characteristic matrices for each level.
    ///
    /// See documentation for the class for details on the underlying
    /// structure.
    ///
    /// Characteristic matrices provide information on the relation between\n
    /// the basis functions of this gsHTensorBasis \f$ H \f$ and\n
    /// the tensor-product basis functions of the underlying
    /// tensor-product bases \f$ B^\ell \f$.
    ///
    /// Let <em>vk = m_xmatrix[k]</em>. \em vk is a gsSortedVector. It contains
    /// a list of indices of the basis function of level \em k, i.e., of the
    /// basis functions which "are taken" from \f$B^k\f$.
    /// These indices are stored as the global indices in \f$B^k\f$.
    std::vector< CMatrix > m_xmatrix;

    /// The tree structure of the index space
    hdomain_type m_tree;

    // // Stores the coordinates of all inserted boxes
    // (for debugging purposes)
    // boxHistory m_boxHistory;

public:
    // Needed since m_tree is 16B aligned
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
protected:

    /// \brief Stores the offsets of active functions for all levels
    ///
    /// See documentation for the class for details on the underlying
    /// structure. As mentioned there, the basis functions of the
    /// hierarchical basis \f$ H\f$ have a global numbering, where
    /// the functions from \f$B^0\f$ come first, then those from \f$B^1\f$,
    /// then \f$B^2\f$, and so forth.
    ///
    /// The entry <em>m_xmatrix_offset[k]</em>
    /// indicates the index from which the basis functions from
    /// level \em k (i.e., those taken from \f$ B^k \f$) start.
    std::vector<unsigned> m_xmatrix_offset;

    //------------------------------------
 
public:
    const std::vector< CMatrix >& getXmatrix() const
    {
        return m_xmatrix;
    }

    /// \brief Returns the tensor B-spline space of all levels.
    /// \return
    ///
    const std::vector<gsTensorBSplineBasis<d,T,gsCompactKnotVector<T> >* >& getBases() const
    {
        return m_bases;
    }

    /// Returns the dimension of the parameter space
    virtual int dim() const 
    { return d; }
    
    /// Returns the number of breaks (distinct knot values) in direction \a k of level \a lvl
    int numBreaks(int lvl, int k) const
    {
        return m_tree.numBreaks(lvl,k);
    }

    /// Returns the number of knots in direction \a k of level \a lvl
    int numKnots(int lvl, int k) const
    {
        needLevel(lvl);

        return m_bases[lvl]->knots(k).size();
    }

    /// Returns the \a i-th knot in direction \a k at level \a lvl
    T knot(int lvl, int k, int i) const
    {
        needLevel(lvl);

        return m_bases[lvl]->component(k).knot(i);
        //return m_bases[lvl]->knot(k,i);
    }

    /// Returns the anchors points that represent the members of the basis
    virtual void anchors_into(gsMatrix<T> & result) const 
    {
        result.resize( d, this->size()) ;
        unsigned k(0);

        gsVector<unsigned, d> ind;
        for(std::size_t i = 0; i < m_xmatrix.size(); i++)
        {
            for( CMatrix::const_iterator it =
                 m_xmatrix[i].begin(); it != m_xmatrix[i].end(); it++)
            {
                ind = m_bases[i]->tensorIndex(*it);
                for ( unsigned r = 0; r!=d; ++r ) 
                    result(r,k) = m_bases[i]->knots(r).greville( ind[r] );
                k++;
            }
        }
    }

    virtual void connectivity(const gsMatrix<T> & nodes, gsMesh<T> & mesh) const;

    // Make gsBasis::connectivity visible
    virtual void connectivity(gsMesh<T> & mesh) const
    { gsBasis<T>::connectivity(mesh); }

  // Prints the characteristic matrices (ie. the indices of all basis
  // functions in the basis)
  void printCharMatrix(std::ostream &os = gsInfo) const
    {  
      os<<"Characteristic matrix:\n";
      for(unsigned i = 0; i<= maxLevel(); i++)
      {
          if ( m_xmatrix[i].size() )
          {
              os<<"- level="<<i<< 
                  ", size="<<m_xmatrix[i].size() << ":\n";
              os << "("<< m_bases[i]->tensorIndex(*m_xmatrix[i].begin()).transpose() <<")";
              for( CMatrix::const_iterator  it = 
                   m_xmatrix[i].begin()+1; it != m_xmatrix[i].end(); it++)
              {
                os << ", ("<< m_bases[i]->tensorIndex(*it).transpose() <<")";
              }
              os <<"\n";
          }
          else
          {
              os<<"- level="<<i<<" is empty.\n";
          }
      }
  }

  /// Prints the spline-space hierarchy
  void printSpaces(std::ostream &os = gsInfo) const
    {  
      os<<"Spline-space hierarchy:\n";
      for(unsigned i = 0; i<= maxLevel(); i++)
      {
          if ( m_xmatrix[i].size() )
          {
              os<<"- level="<<i<< 
                  ", size="<<m_xmatrix[i].size() << ":\n";
              os << "Space: "<< * m_bases[i] <<")";
          }
          else
          {
              os<<"- level="<<i<<" is empty.\n";
          }
      }
  }

  /// Prints the spline-space hierarchy
  void printBasic(std::ostream &os = gsInfo) const
  {
      os << "basis of dimension " <<this->dim()<<
          ",\nlevels="<< this->m_tree.getMaxInsLevel()+1 <<", size="
         << this->size()<<", tree_nodes="<< this->m_tree.size()
    //   << ", leaf_nodes="<< this->m_tree.leafSize();
    //const std::pair<int,int> paths  = this->m_tree.minMaxPath();
    //os << ", path lengths=("<<paths.first<<", "<<paths.second
         << ").\n";
      const gsMatrix<T> supp  = this->support();
      os << "Domain: ["<< supp.col(0).transpose()<< "]..["<<
          supp.col(1).transpose()<< "].\n";
      os <<"Size per level: ";
      for(unsigned i = 0; i<= this->m_tree.getMaxInsLevel(); i++)
          os << this->m_xmatrix[i].size()<< " ";
      os<<"\n";
  }
    
    // Look at gsBasis.h for the documentation of this function
    void active_into(const gsMatrix<T> & u, gsMatrix<unsigned>& result) const;


    // Look at gsBasis.h for the documentation of this function
    gsMatrix<unsigned> * allBoundary( ) const;

    // Look at gsBasis.h for the documentation of this function
    virtual gsMatrix<unsigned> * boundaryOffset(boxSide const & s, unsigned offset ) const;

    // Look at gsBasis.h for the documentation of this function
    void evalAllDers_into(const gsMatrix<T> & u, int n, gsMatrix<T>& result) const;

  const gsHDomain<d> & tree() const { return m_tree; }

  gsHDomain<d> &       tree()       { return m_tree; }

  /// Cleans the basis, removing any inactive levels 
  void makeCompressed();


  /// Returns the boundary basis for side s
  // virtual gsHTensorBasis<d,T> * boundaryBasis(boxSide const & s ) const 

  /// Returns a bounding box for the basis' domain
  gsMatrix<T> support() const;

  gsMatrix<T> support(const unsigned & i) const;

  void elementSupport_into(const unsigned& i,
                           gsMatrix<unsigned, d, 2>& result) const
  {
      unsigned lvl = levelOf(i);

      m_bases[lvl]->elementSupport_into(m_xmatrix[lvl][ i - m_xmatrix_offset[lvl] ],
                result);
  }

  /// Clone function. Used to make a copy of a derived basis
  virtual gsHTensorBasis * clone() const = 0;

  /// The number of basis functions in this basis
  int size() const;

  /// The number of nodes in the tree representation
  int treeSize() const
  {
      return m_tree.size();
  }

  /// The number of active basis functions at points \a u
  void numActive(const gsMatrix<T> & u, gsVector<unsigned>& result) const;

  /// The 1-d basis for the i-th parameter component at the highest level
  virtual gsBSplineBasis<T,gsCompactKnotVector<T> > & component(unsigned i) const
  {
      return m_bases[ this->maxLevel() ]->component(i);
  }

  /// Returns the tensor basis member of level i 
  gsTensorBSplineBasis<d,T,gsCompactKnotVector<T> > & tensorLevel(unsigned i) const
  {
      needLevel( i );
      return *this->m_bases[i];
  }

  // Refine the basis uniformly by inserting \a numKnots new knots on each knot span
  virtual void uniformRefine(int numKnots = 1, int mul=1);

  // Refine the basis uniformly and adjust the given matrix of coefficients accordingly
  //virtual void uniformRefine_withCoefs(gsMatrix<T>& coefs, int numKnots = 1, int mul = 1)

  // Refine the basis uniformly and produce a sparse matrix which
  // maps coarse coefficient vectors to refined ones
  //virtual void uniformRefine_withTransfer(gsSparseMatrix<T,RowMajor> & transfer, int numKnots = 1, int mul = 1)

  // Refine the basis uniformly and adjust the given matrix of coefficients accordingly
  void uniformRefine_withCoefs(gsMatrix<T>& coefs, int numKnots = 1, int mul = 1);

  // Refine the basis and adjust the given matrix of coefficients accordingly
  void refine_withCoefs(gsMatrix<T> & coefs, gsMatrix<T> const & boxes);

  /** Refine the basis and adjust the given matrix of coefficients accordingly.
   * @param coefs is a matrix of coefficients as given, e.g., by gsTHBSpline<>::coefs();
   * @param boxes specify where to refine; each 5-tuple gives the level of the box,
   * then two indices (in the current level indexing) of the lower left corner and finally
   * two indices of the upper right corner.
   */
  void refineElements_withCoefs(gsMatrix<T> & coefs,std::vector<unsigned> const & boxes);

  /// @brief If the basis is of polynomial or piecewise polynomial
  /// type, then this function returns the polynomial degree.
  virtual inline int degree() const
    { return m_bases[0]->degree(); }

    int maxDegree() const 
    { 
        int td = m_bases[0]->degree(0);
        // take maximum of coordinate bases degrees
        for (unsigned k=1; k!=d; ++k)
            td = math::max(td, m_bases[0]->degree(k));
        return td;
    }

    int minDegree() const 
    { 
        int td = m_bases[0]->degree(0);
        // take maximum of coordinate bases degrees
        for (unsigned k=1; k!=d; ++k)
            td = math::min(td, m_bases[0]->degree(k));
        return td;
    }

  /// @brief If the basis is a tensor product of (piecewise)
  /// polynomial bases, then this function returns the polynomial
  /// degree of the \a i-th component.
  virtual inline int degree(int i) const
    { return m_bases[0]->degree(i);}

  // S.K.
  /// @brief Returns the level(s) at point(s) in the parameter domain
  ///
  /// \param[in] Pts gsMatrix of size <em>d</em> x <em>n</em>, where\n
  /// \em d is the dimension of the parameter domain and\n
  /// \em n is the number of evaluation points.\n
  /// Each column of \em Pts represents one evaluation point.
  /// \return levels gsMatrix of size <em>1</em> x <em>n</em>.\n
  /// <em>levels(0,i)</em> is the level of the point defined by the <em>i</em>-th column in \em Pts.
  int getLevelAtPoint(const  gsMatrix<T> & Pt ) const;

  /// Returns the level in which the indices are stored internally
  unsigned maxLevel() const
  {
      return m_tree.getMaxInsLevel();
  }

  /// Returns the level of \a function, which is a hier. Id index
  int get_level(unsigned function) const; 

    /// Returns the level of the function indexed \a i (in continued indices)
    inline int levelOf(unsigned i) const
    {
        return std::upper_bound(m_xmatrix_offset.begin(), 
                                m_xmatrix_offset.end(), i)
            - m_xmatrix_offset.begin() - 1;
    }

/*
    const boxHistory & get_inserted_boxes() const
    {
        return m_boxHistory;
    }
*/

    /** @brief Refine the basis to levels and in the areas defined by
     * \a boxes with an extension.
    *
    * \param[in] boxes gsMatrix of size \em d x \em n, where\n
    * \em n is the number of refinement boxes.\n
    * Every two consecutive columns specify the lower and upper corner of one refinement box
    * (See also documentation of refine() for the format of \em box)
    * \param[in] refExt is an integer specifying how many cells should also be
    * refined around the respective boxes.
    *
    */
    virtual void refine(gsMatrix<T> const & boxes, int refExt);

    /** @brief Refine the basis to levels and in the areas defined by \a boxes.
    *
    * \param[in] boxes gsMatrix of size \em d x \em n, where\n
    * \em n is the number of refinement boxes.\n
    * Every two consecutive columns specify the lower and upper corner of one refinement box
    * (See also documentation of refine() for the format of \em box)
    */
    virtual void refine(gsMatrix<T> const & boxes);

    /**
     * @brief Insert the given boxes into the quadtree.
     *
     * Each box is defined by <em>2d+1</em> indices, where \em d is the dimension
     * of the parameter domain.
     * The first index defines the level in which the box should be inserted,
     * the next \a d indices the "coordinates" of the lower corner in the index space,
     * and the last \a d indices the "coordinates" of the upper corner.
     *
     * <b>Example:</b> Let <em>d=3</em> and
     *\f[ \mathsf{boxes} = [ L^1, \ell_x^1, \ell_y^1, \ell_z^1, u_x^1, u_y^1, u_z^1,
      L^2, \ell_x^2, \ell_y^2, \ell_z^2, u_x^2, u_y^2, u_z^2,
      L^3, \ell_x^3, \ell_y^3,
     \ldots ], \f]
     * then, the first box will be inserted in level \f$L^1\f$ and its
     * lower and upper corner will have the indices
     * \f$ (\ell_x^1, \ell_y^1, \ell_z^1)\f$ and \f$ (u_x^1, u_y^1, u_z^1) \f$
     * in the index space of level \f$L^1\f$, respectively.
     *
     *
     * @param boxes vector of size <em>N (2d+1)</em>, where\n
     * \em N is the number of boxes,\n
     * \em d is the dimension of the parameter domain.\n
     * See description above for details on the format.
     */
    virtual void refineElements(std::vector<unsigned> const & boxes);

    // Look at gsBasis.h for the documentation of this function
    //virtual void uniformRefine(int numKnots = 1);

    typename gsBasis<T>::domainIter makeDomainIterator() const 
    { 
        return typename gsBasis<T>::domainIter(new gsHDomainIterator<T, d>(*this));
    }

    typename gsBasis<T>::domainIter makeDomainIterator(const boxSide & s) const
    {
        return ( s == boundary::none ? 
                 typename gsBasis<T>::domainIter(new gsHDomainIterator<T, d>(*this)) :
                 typename gsBasis<T>::domainIter(new gsHDomainBoundaryIterator<T, d>(*this,s) )
                );
    }

    /// @brief Returns the tensor index of the function indexed \a i
    /// (in continued indices).
    ///
    /// @param[in] i Global (continued) index of a basis function of
    /// the hierarchical basis.
    /// @return The tensor index of this basis function
    /// with respect to the tensor-product basis
    /// of the corresponding level.
    inline
    unsigned flatTensorIndexOf(const unsigned i) const
    {

        const int level = this->levelOf(i);

        const unsigned offset = this->m_xmatrix_offset[level];
        const unsigned ind_in_level = this->m_xmatrix[level][i - offset];

        return ind_in_level;
    }


    /// @brief Returns the tensor index of the function indexed \a i
    /// (in continued indices).
    ///
    /// @param[in] i Global (continued) index of a basis function of
    /// the hierarchical basis.
    /// @param level Level of the i-th basis function.
    /// @return The tensor index of this basis function
    /// with respect to the tensor-product basis
    /// of \em level.
    inline
    unsigned flatTensorIndexOf(const unsigned i, const unsigned level) const
    {

        const unsigned offset = this->m_xmatrix_offset[level];
        const unsigned ind_in_level = this->m_xmatrix[level][i - offset];

        return ind_in_level;
    }

    /// @brief Gives polylines on the boundaries between different levels of the mesh.
    /// @param result variable where to write the polylines in the form
    /// < levels < polylines_in_one_level < one_polyline < one_segment (x1, y1, x2, y2) > > > > ,
    /// where <x1, y1, x2, y2 > are so that (x1, y1) <=LEX  (x2, y2)
    /// and where x1, y1, x2 and y2 are parameters (knots).
    /// @return bounding boxes of the polylines in the form
    /// < levels < polylines_in_one_level < x_ll, y_ll, x_ur, y_ur > > >, where "ur" stands for "upper right" and "ll" for "lower left".
    std::vector< std::vector< std::vector< unsigned int > > > domainBoundariesParams( std::vector< std::vector< std::vector< std::vector< T > > > >& result) const;

    /// @brief Gives polylines on the boundaries between different levels of the mesh.
    /// @param result variable where to write the polylines in the form
    /// < levels < polylines_in_one_level < one_polyline < one_segment (x1, y1, x2, y2) > > > >
    /// where <x1, y1, x2, y2 > are so that (x1, y1) <=LEX  (x2, y2)
    /// and where x1, y1, x2 and y2 are indices of the knots with respect to m_maxInsLevel.
    /// @return bounding boxes of the polylines in the form
    /// < levels < polylines_in_one_level < x_ll, y_ll, x_ur, y_ur > > >, where "ur" stands for "upper right" and "ll" for "lower left".
    std::vector< std::vector< std::vector< unsigned int > > > domainBoundariesIndices( std::vector< std::vector< std::vector< std::vector< unsigned int > > > >& result) const;
    // TO DO: use gsHDomainLeafIterator for a better implementation
    int numElements() const
    {
        gsHDomainIterator<T, d> domIter(*this);

        int numEl = 0;
        for (; domIter.good(); domIter.next())
        {
            numEl++;
        }

        return numEl;
    }

    /// @brief transformes a sortedVector \a indexes of flat tensor index
    /// of the bspline basis of \a level to hierachical indexes in place. If a flat
    /// tensor index is not found, it will transform to -1.
    ///
    /// @param[in] indexes flat tensor indexes of the function in level
    /// @param level Level of the basis.
    void flatTensorIndexesToHierachicalIndexes(gsSortedVector< int > & indexes,const int level) const;

    /// @brief takes a flat tensor \a index
    /// of the bspline basis of \a level and gives back the hierachical index. If a flat
    /// tensor index is not found, it will return -1.
    ///
    /// @param[in] index flat tensor index of the function in level
    /// @param level Level of the basis.
    /// @return hierachical index, or -1 if it was not found
    int flatTensorIndexToHierachicalIndex(unsigned index,const int level) const;

    /// Fills the vector actives with booleans, that determine if a function
    /// of the given level is active. The functions on the boundary are ordered
    /// in ascending patchindex order.
    ///
    /// \param[in] level : level of the boundary functions
    /// \param[in] s : boundary side
    /// \param[out] actives : the result, true if its active, false if not
    void activeBoundaryFunctionsOfLevel(const unsigned level,const boxSide & s,std::vector<bool>& actives) const;

    /// @brief Increases the multiplicity of a knot with the value \a knotValue in level \a lvl
    /// in direction \a dir by \a mult.
    /// If knotValue is not currently in the given knot vector its not added.
    ///
    /// \param[in] lvl : level
    /// \param[in] dir : direction
    /// \param[in] knotValue : value of the knot
    /// \param[in] mult : multiplicity

    virtual void increaseMultiplicity(index_t lvl, int dir, T knotValue, int mult = 1);

    /// @brief Increases the multiplicity of several knots with the value \a knotValue in level \a lvl
    /// in direction \a dir by \a mult.
    /// If knotValue is not currently in the given knot vector its not added.
    ///
    /// \param[in] lvl : level
    /// \param[in] dir : direction
    /// \param[in] knotValue : value of the knot
    /// \param[in] mult : multiplicity
    virtual void increaseMultiplicity(index_t lvl, int dir, const std::vector<T> & knotValue, int mult = 1);

protected:

    /// @brief Updates the basis structure (eg. charact. matrices, etc), to
    /// be called after any modifications.
    virtual void update_structure(); // to do: rename as updateCharMatrices
    
    /// @brief Makes sure that there are \a numLevels grids computed
    /// in the hierarachy
    void needLevel(int maxLevel) const;

    /// @brief Creates \a numLevels extra grids in the hierarchy
    void createMoreLevels(int numLevels) const;

    /// Computes difference between coarser knot vector (ckv) and finer knot
    /// vector (fkv). Difference is computed just between c_low, c_high
    /// indices f_low, f_high indices for ckv and fkv respectfully. Result
    /// is stored in vector knots.
    ///
    /// @param ckv coarse knot vector
    /// @param c_low low index of the interested area for the ckv
    /// @param c_high high index of the interested area for the ckv
    /// @param fkv finer knot vector
    /// @param f_low low index of the interested area for the fkv
    /// @param f_high high index of the interested area for the fkv
    /// @param knots {k | k \in fkv & k \notin ckv}
    static
    void _differenceBetweenKnotVectors(const gsCompactKnotVector<T>& ckv,
                                       const unsigned c_low,
                                       const unsigned c_high,
                                       const gsCompactKnotVector<T>& fkv,
                                       const unsigned f_low,
                                       const unsigned f_high,
                                       std::vector<T>& knots)
    {

        unsigned f_knot_mltpl; // finest knot multiplicity
        unsigned c_knot_mltpl; // coarse knot multiplicity

        T f_knot, c_knot;// finest and coarsest knot

        unsigned c_index = c_low;
        unsigned f_index = f_low;

        while (f_index <= f_high)
        {
            f_knot = fkv.uValue(f_index); // finest knot
            c_knot = ckv.uValue(c_index); // coarse knot

            f_knot_mltpl = fkv.u_multiplicityIndex(f_index);

            if (f_knot == c_knot)
            {
                c_knot_mltpl = ckv.u_multiplicityIndex(c_index);

                if (c_knot_mltpl < f_knot_mltpl)
                {
                    for (unsigned j = 0; j < f_knot_mltpl - c_knot_mltpl; ++j)
                        knots.push_back(f_knot);
                }

                ++f_index;
                ++c_index;
            }
            else // f_knot < c_knot
            {
                for (unsigned j = 0; j < f_knot_mltpl; ++j)
                    knots.push_back(f_knot);
                ++f_index;
            }
        }
    }

    /// gets all the boxes along a slice in direction \a dir at parameter \a par.
    /// the boxes are given back in a std::vector<unsigned> and are in the right format
    /// to be given to refineElements().
    void getBoxesAlongSlice( int dir, T par,std::vector<unsigned>& boxes ) const;

private:

    /// insert a domain into quadtree
    void insert_box(gsVector<unsigned,d> const & k1, 
                    gsVector<unsigned,d> const & k2, int lvl);
    
    void initialize_class(gsBasis<T> const&  tbasis);

    /// set all functions to active or passive- one by one
    void set_activ1(int level);

    // set all functions to active or passive- recursive
    //void setActive();



    ///returns a transfer matrix using the characteristic matrix of the old and new basis
    virtual gsMatrix<T> coarsening(const std::vector<gsSortedVector<unsigned> >& old, const std::vector<gsSortedVector<unsigned> >& n, const gsSparseMatrix<T,RowMajor> & transfer) = 0;
    virtual gsMatrix<T> coarsening_direct(const std::vector<gsSortedVector<unsigned> >& old, const std::vector<gsSortedVector<unsigned> >& n,  const std::vector<gsSparseMatrix<T,RowMajor> >& transfer) = 0;

    /// Implementation of the features common to domainBoundariesParams and domainBoundariesIndices. It takes both
    /// @param indices and @param params but fills in only one depending on @param indicesFlag (if true, then it returns indices).
    std::vector< std::vector< std::vector< unsigned int > > > domainBoundariesGeneric(std::vector< std::vector< std::vector< std::vector< unsigned int > > > >& indices,
										      std::vector< std::vector< std::vector< std::vector< T > > > >& params,
										      bool indicesFlag ) const;

 //D
public:
    ///returns transfer matrix betweend the hirarchycal spline given by the characteristic matrix  "old" and this
    void transfer (const std::vector<gsSortedVector<unsigned> > &old, gsMatrix<T>& result);
    /// create characteristic matrices for basis where "level" is the maximum level i.e. ignoring higher level refinements
    void setActiveToLvl(int level, std::vector<gsSortedVector<unsigned> >& x_matrix_lvl);


//    void local2globalIndex( gsVector<unsigned,d> const & index,
//                            unsigned lvl,
//                            gsVector<unsigned,d> & result
//        ) const;

//    void global2localIndex( gsVector<unsigned,d> const & index,
//                            unsigned lvl,
//                            gsVector<unsigned,d> & result
//        ) const;

}; // class gsHTensorBasis
 

} // namespace gismo

//////////////////////////////////////////////////
//////////////////////////////////////////////////

#ifndef GISMO_BUILD_LIB
#include GISMO_HPP_HEADER(gsHTensorBasis.hpp)
#endif
