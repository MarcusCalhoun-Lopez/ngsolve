#ifndef FILE_FACETFE
#define FILE_FACETFE

/*********************************************************************/
/* File:   facetfe.hpp                                               */
/* Author: A. Sinwel, H. Egger, J. Schoeberl                         */
/* Date:   2008                                                      */
/*********************************************************************/

namespace ngfem
{

  /*
   * Facet Finite Elements
   */ 

  template <int D>
  class NGS_DLL_HEADER FacetVolumeFiniteElement;
  

  template <int D>
  class FacetFEFacet : public ScalarFiniteElement<D>
  {
    int fnr;
    const FacetVolumeFiniteElement<D> & fe;
  public:
    FacetFEFacet (int afnr,
		  const FacetVolumeFiniteElement<D> & afe,
		  int andof, int aorder)
      : ScalarFiniteElement<D> (andof, aorder), fnr(afnr), fe(afe) 
    { 
      ; // cout << "created facetfefacet" << endl;
    }

    virtual ELEMENT_TYPE ElementType() const { return fe.ElementType(); }

    virtual void CalcShape (const IntegrationPoint & ip, 
			    SliceVector<> shape) const
    {
      fe.CalcFacetShapeVolIP(fnr, ip, shape);
    }
    
    virtual void CalcDShape (const IntegrationPoint & ip, 
			     SliceMatrix<> dshape) const
    {
      throw Exception ("facetfe - calcdshape not olverloaded");
    }

  };

  
  

  template <int D>
  class FacetVolumeFiniteElement : public FiniteElement
  {
  protected:
    int vnums[8];
    int facet_order[6]; 
    int first_facet_dof[7];

    using FiniteElement::ndof;
    using FiniteElement::order;

  public:

    void SetVertexNumbers (FlatArray<int> & avnums)
    {
      for (int i = 0; i < avnums.Size(); i++)
	vnums[i] = avnums[i];
    }

    void SetOrder (int ao)  
    {
      order = ao;
      for (int i = 0; i < 6; i++)
	facet_order[i] = ao;
    }
    
    void SetOrder (FlatArray<int> & ao)
    {
      for (int i=0; i<ao.Size(); i++)
	facet_order[i] = ao[i];
      
      order = facet_order[0];        // integration order
      for (int i = 1; i < ao.Size(); i++)
	order = max(order, ao[i]);
    }

    FacetFEFacet<D> Facet (int fnr) const 
    { 
      return FacetFEFacet<D> (fnr, *this, 
			      GetFacetDofs(fnr).Size(), facet_order[fnr]); 
    }


    virtual void CalcFacetShapeVolIP (int fnr, const IntegrationPoint & ip, 
				      SliceVector<> shape) const = 0;



    IntRange GetFacetDofs(int fnr) const
    {
      return IntRange (first_facet_dof[fnr], first_facet_dof[fnr+1]);
    }

    virtual string ClassName() const { return "FacetVolumeFiniteElement"; }

    virtual void ComputeNDof () = 0;
  };




#ifdef FILE_FACETHOFE_CPP
#define FACETHOFE_EXTERN
#else
#define FACETHOFE_EXTERN extern
#endif

  FACETHOFE_EXTERN template class FacetVolumeFiniteElement<1>;
  FACETHOFE_EXTERN template class FacetVolumeFiniteElement<2>;
  FACETHOFE_EXTERN template class FacetVolumeFiniteElement<3>;

  FACETHOFE_EXTERN template class FacetFEFacet<1>;
  FACETHOFE_EXTERN template class FacetFEFacet<2>;
  FACETHOFE_EXTERN template class FacetFEFacet<3>;

}



#endif
