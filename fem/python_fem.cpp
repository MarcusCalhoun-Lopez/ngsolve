#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include "../ngstd/bspline.hpp"
#include <fem.hpp>
#include <comp.hpp>
#include <mutex>
using namespace ngfem;
using ngfem::ELEMENT_TYPE;

#include "pml.hpp"

#include "tpintrule.hpp"
namespace ngfem
{
  extern SymbolTable<double> * constant_table_for_FEM;
  SymbolTable<double> pmlpar;
}


shared_ptr<CoefficientFunction>
MakeCoefficientFunction (CF_Type type,
                         const Array<shared_ptr<CoefficientFunction>> & childs,
                         py::list data);


struct PythonCoefficientFunction : public CoefficientFunction {
  PythonCoefficientFunction() : CoefficientFunction(1,false) { ; }

    virtual double EvaluateXYZ (double x, double y, double z) const = 0;
  
    py::list GetCoordinates(const BaseMappedIntegrationPoint &bip ) {
        double x[3]{0};
        int dim = bip.GetTransformation().SpaceDim();
        const DimMappedIntegrationPoint<1,double> *ip1;
        const DimMappedIntegrationPoint<2,double> *ip2;
        const DimMappedIntegrationPoint<3,double> *ip3;
        switch(dim) {

            case 1:
                ip1 = static_cast<const DimMappedIntegrationPoint<1,double>*>(&bip);
                x[0] = ip1->GetPoint()[0];
                break;
            case 2:
                ip2 = static_cast<const DimMappedIntegrationPoint<2,double>*>(&bip);
                x[0] = ip2->GetPoint()[0];
                x[1] = ip2->GetPoint()[1];
                break;
            case 3:
                ip3 = static_cast<const DimMappedIntegrationPoint<3,double>*>(&bip);
                x[0] = ip3->GetPoint()[0];
                x[1] = ip3->GetPoint()[1];
                x[2] = ip3->GetPoint()[2];
                break;
            default:
                break;
        }
        py::list list;
        int i;
        for(i=0; i<dim; i++)
            list.append(py::float_(x[i]));
        for(i=0; i<3; i++)
            list.append(py::float_(0.0));
        return list;
    }
};

typedef CoefficientFunction CF;

shared_ptr<CF> MakeCoefficient (py::object val)
{
  py::extract<shared_ptr<CF>> ecf(val);
  if (ecf.check()) return ecf();
  
  // a numpy.complex converts itself to a real, and prints a warning
  // thus we check for it first
  if (string(py::str(val.get_type())) == "<class 'numpy.complex128'>")
    return make_shared<ConstantCoefficientFunctionC> (val.cast<Complex>());

  if(py::CheckCast<double>(val))
    return make_shared<ConstantCoefficientFunction> (val.cast<double>());
  if(py::CheckCast<Complex>(val)) 
    return make_shared<ConstantCoefficientFunctionC> (val.cast<Complex>());

  if (py::isinstance<py::list>(val))
    {
      py::list el(val);
      Array<shared_ptr<CoefficientFunction>> cflist(py::len(el));
      for (int i : Range(cflist))
        cflist[i] = MakeCoefficient(el[i]);
      return MakeDomainWiseCoefficientFunction(move(cflist));
    }

  if (py::isinstance<py::tuple>(val))
    {
      py::tuple et(val);
      Array<shared_ptr<CoefficientFunction>> cflist(py::len(et));
      for (int i : Range(cflist))
        cflist[i] = MakeCoefficient(et[i]);
      return MakeVectorialCoefficientFunction(move(cflist));
    }


  throw Exception ("cannot make coefficient");
}

Array<shared_ptr<CoefficientFunction>> MakeCoefficients (py::object py_coef)
{
  Array<shared_ptr<CoefficientFunction>> tmp;
  if (py::isinstance<py::list>(py_coef))
    {
      auto l = py_coef.cast<py::list>();
      for (int i = 0; i < py::len(l); i++)
        tmp += MakeCoefficient(l[i]);
    }
  else if (py::isinstance<py::tuple>(py_coef))
    {
      auto l = py_coef.cast<py::tuple>();
      for (int i = 0; i < py::len(l); i++)
        tmp += MakeCoefficient(l[i]);
    }
  else
    tmp += MakeCoefficient(py_coef);

  // return move(tmp);  // clang recommends not to move it ...
  return tmp;
}

std::map<string, std::function<shared_ptr<CF>(shared_ptr<CF>)>> unary_math_functions;
std::map<string, std::function<shared_ptr<CF>(shared_ptr<CF>, shared_ptr<CF>)>> binary_math_functions;

template <typename FUNC>
void ExportStdMathFunction(py::module &m, string name, string description)
{
  auto f = [name] (shared_ptr<CF> coef) -> shared_ptr<CF>
            {
                FUNC func;
                return UnaryOpCF(coef, func, FUNC::Name());
            };

  unary_math_functions[name] = f;

  m.def (name.c_str(), [name] (py::object x) -> py::object
            {
              FUNC func;
              if (py::extract<shared_ptr<CF>>(x).check())
                {
                  auto coef = py::extract<shared_ptr<CF>>(x)();
                  return py::cast(unary_math_functions[name](coef));
                }
              py::extract<double> ed(x);
              if (ed.check()) return py::cast(func(ed()));
              if (py::extract<Complex> (x).check())
                return py::cast(func(py::extract<Complex> (x)()));
              throw py::type_error (string("can't compute math-function, type = ")
                                    + typeid(FUNC).name());
            }, py::arg("x"), description.c_str());
}


template <typename FUNC>
void ExportStdMathFunction2(py::module &m, string name, string description)
{
  auto f = [name] (shared_ptr<CF> cx, shared_ptr<CF> cy) -> shared_ptr<CF>
            {
                FUNC func;
                return BinaryOpCF(cx, cy, func,
                                          [](bool a, bool b) { return a||b; }, FUNC::Name());
            };

  binary_math_functions[name] = f;

  m.def (name.c_str(), 
         [name] (py::object x, py::object y) -> py::object
         {
           FUNC func;
           if (py::extract<shared_ptr<CF>>(x).check() || py::extract<shared_ptr<CF>>(y).check())
             {
               shared_ptr<CoefficientFunction> cx = py::cast<shared_ptr<CF>>(x);
               shared_ptr<CoefficientFunction> cy = py::cast<shared_ptr<CF>>(y);
               return py::cast(binary_math_functions[name](cx,cy));
             }
           py::extract<double> dx(x), dy(y);
           if (dx.check() && dy.check()) return py::cast(func(dx(), dy()));
           py::extract<Complex> cx(x), cy(y);
           if (cx.check() && cy.check()) return py::cast(func(cx(), cy()));
           throw py::type_error (string("can't compute binary math-function")+typeid(FUNC).name());
         }, py::arg("x"), py::arg("y"), description.c_str());
}





struct GenericBSpline {
  shared_ptr<BSpline> sp;
  GenericBSpline( const BSpline &asp ) : sp(make_shared<BSpline>(asp)) {;}
  GenericBSpline( shared_ptr<BSpline> asp ) : sp(asp) {;}
  template <typename T> T operator() (T x) const { return (*sp)(x); }
  Complex operator() (Complex x) const { return (*sp)(x.real()); }
  SIMD<double> operator() (SIMD<double> x) const
  { return SIMD<double>([&](int i)->double { return (*sp)(x[i]); } );}
  SIMD<Complex> operator() (SIMD<Complex> x) const
  { return SIMD<double>([&](int i)->double { return (*sp)(x.real()[i]); } );}
  AutoDiff<1,SIMD<double>> operator() (AutoDiff<1,SIMD<double>> x) const { throw ExceptionNOSIMD ("AutoDiff for bspline not supported"); }
  AutoDiffDiff<1,SIMD<double>> operator() (AutoDiffDiff<1,SIMD<double>> x) const { throw ExceptionNOSIMD ("AutoDiffDiff for bspline not supported"); }  
};
struct GenericSin {
  template <typename T> T operator() (T x) const { return sin(x); }
  static string Name() { return "sin"; }
};
struct GenericCos {
  template <typename T> T operator() (T x) const { return cos(x); }
  static string Name() { return "cos"; }
};
struct GenericTan {
  template <typename T> T operator() (T x) const { return tan(x); }
  static string Name() { return "tan"; }
};
struct GenericExp {
  template <typename T> T operator() (T x) const { return exp(x); }
  static string Name() { return "exp"; }
};
struct GenericLog {
  template <typename T> T operator() (T x) const { return log(x); }
  static string Name() { return "log"; }
};
struct GenericATan {
  template <typename T> T operator() (T x) const { return atan(x); }
  static string Name() { return "atan"; }
};
struct GenericACos {
  template <typename T> T operator() (T x) const { return acos(x); }
  // double operator() (double x) const { return acos(x); }
  // template <typename T> T operator() (T x) const { throw Exception("acos not available"); }
  SIMD<Complex> operator() (SIMD<Complex> x) const { throw Exception("acos not available for SIMD<complex>"); }
  static string Name() { return "acos"; }
};
struct GenericASin {
  template <typename T> T operator() (T x) const { return asin(x); }
  // double operator() (double x) const { return acos(x); }
  // template <typename T> T operator() (T x) const { throw Exception("acos not available"); }
  SIMD<Complex> operator() (SIMD<Complex> x) const { throw Exception("asin not available for SIMD<complex>"); }
  static string Name() { return "asin"; }
};
struct GenericSqrt {
  template <typename T> T operator() (T x) const { return sqrt(x); }
  static string Name() { return "sqrt"; }
};
struct GenericFloor {
  template <typename T> T operator() (T x) const { return floor(x); }
  Complex operator() (Complex x) const { throw Exception("no floor for Complex"); }  
  // SIMD<double> operator() (SIMD<double> x) const { throw ExceptionNOSIMD("no floor for simd"); }
  SIMD<Complex> operator() (SIMD<Complex> x) const { throw ExceptionNOSIMD("no floor for simd"); }  
  // AutoDiff<1> operator() (AutoDiff<1> x) const { throw Exception("no floor for AD"); }
  AutoDiffDiff<1> operator() (AutoDiffDiff<1> x) const { throw Exception("no floor for ADD"); }
  static string Name() { return "floor"; }
};
struct GenericCeil {
  template <typename T> T operator() (T x) const { return ceil(x); }
  Complex operator() (Complex x) const { throw Exception("no ceil for Complex"); }  
  // SIMD<double> operator() (SIMD<double> x) const { throw ExceptionNOSIMD("no ceil for simd"); }
  SIMD<Complex> operator() (SIMD<Complex> x) const { throw ExceptionNOSIMD("no ceil for simd"); }  
  // AutoDiff<1> operator() (AutoDiff<1> x) const { throw Exception("no ceil for AD"); }
  AutoDiffDiff<1> operator() (AutoDiffDiff<1> x) const { throw Exception("no ceil for ADD"); }
  static string Name() { return "ceil"; }
};

struct GenericConj {
  template <typename T> T operator() (T x) const { return Conj(x); } // from bla
  static string Name() { return "conj"; }
  SIMD<double> operator() (SIMD<double> x) const { return x; }
  template<typename T>
  AutoDiff<1,T> operator() (AutoDiff<1,T> x) const { throw Exception ("Conj(..) is not complex differentiable"); }
  template<typename T>
  AutoDiffDiff<1,T> operator() (AutoDiffDiff<1,T> x) const { throw Exception ("Conj(..) is not complex differentiable"); }
};

struct GenericATan2 {
  double operator() (double x, double y) const { return atan2(x,y); }
  template <typename T1, typename T2> T1 operator() (T1 x, T2 y) const
  { throw Exception (string("atan2 not available for type ")+typeid(T1).name());  }
  static string Name() { return "atan2"; }
};

struct GenericPow {
  double operator() (double x, double y) const { return pow(x,y); }
  Complex operator() (Complex x, Complex y) const { return pow(x,y); }
  template <typename T1, typename T2> T1 operator() (T1 x, T2 y) const
  {
      return exp (log(x)*y);
  }    
  static string Name() { return "pow"; }
};





  template <int D>
  class NormalVectorCF : public CoefficientFunctionNoDerivative
  {
  public:
    NormalVectorCF () : CoefficientFunctionNoDerivative(D,false) { ; }
    // virtual int Dimension() const { return D; }

    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const override 
    {
      return 0;
    }
    virtual void Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<> res) const override 
    {
      if (ip.Dim() != D)
        throw Exception("illegal dim of normal vector");
      res = static_cast<const DimMappedIntegrationPoint<D>&>(ip).GetNV();
    }

    virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<> res) const // override 
    {
      const TPMappedIntegrationRule * tpir = dynamic_cast<const TPMappedIntegrationRule *>(&ir);
       if(!tpir)
       {
         if (ir[0].Dim() != D)
           throw Exception("illegal dim of normal vector");
         FlatMatrixFixWidth<D> resD(res);
         for (int i = 0; i < ir.Size(); i++)
           resD.Row(i) = static_cast<const DimMappedIntegrationPoint<D>&>(ir[i]).GetNV();
       }
       else
       {
         int facet = tpir->GetFacet();
         auto & mir = *tpir->GetIRs()[facet];
         int dim = mir[0].Dim();
         int ii = 0;
         res = 0.0;
         if(facet == 0)
         {
           if(dim == 1)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)
                 res.Row(ii++).Range(0,dim) = static_cast<const DimMappedIntegrationPoint<1>&>(mir[i]).GetNV();//res1.Row(i).Range(0,dim);
           if(dim == 2)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)          
                 res.Row(ii++).Range(0,dim) = static_cast<const DimMappedIntegrationPoint<2>&>(mir[i]).GetNV();//res1.Row(i).Range(0,dim);
           if(dim == 3)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)          
                 res.Row(ii++).Range(0,dim) = static_cast<const DimMappedIntegrationPoint<3>&>(mir[i]).GetNV();//res1.Row(i).Range(0,dim);
         }
         else
         {
           if(dim == 1)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)
                 res.Row(ii++).Range(D-dim,D) = static_cast<const DimMappedIntegrationPoint<1>&>(mir[j]).GetNV();//res1.Row(i).Range(0,dim);
           if(dim == 2)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)          
                 res.Row(ii++).Range(D-dim,D) = static_cast<const DimMappedIntegrationPoint<2>&>(mir[j]).GetNV();//res1.Row(i).Range(0,dim);
           if(dim == 3)
             for(int i=0;i<tpir->GetIRs()[0]->Size();i++)
               for(int j=0;j<tpir->GetIRs()[1]->Size();j++)          
                 res.Row(ii++).Range(D-dim,D) = static_cast<const DimMappedIntegrationPoint<3>&>(mir[j]).GetNV();//res1.Row(i).Range(0,dim);
         }
      }
    }

    virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<Complex> res) const override 
    {
      if (ir[0].Dim() != D)
	throw Exception("illegal dim of normal vector");
      for (int i = 0; i < ir.Size(); i++)
	res.Row(i) = static_cast<const DimMappedIntegrationPoint<D>&>(ir[i]).GetNV();
    }
    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const override  {
        string miptype;
        if(code.is_simd)
          miptype = "SIMD<DimMappedIntegrationPoint<"+ToLiteral(D)+">>*";
        else
          miptype = "DimMappedIntegrationPoint<"+ToLiteral(D)+">*";
        auto nv_expr = CodeExpr("static_cast<const "+miptype+">(&ip)->GetNV()");
        auto nv = Var("tmp", index);
        code.body += nv.Assign(nv_expr);
        for( int i : Range(D))
          code.body += Var(index,i).Assign(nv(i));
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, BareSliceMatrix<SIMD<double>> values) const override 
    {
      for (size_t i = 0; i < ir.Size(); i++)
        for (size_t j = 0; j < D; j++)
          values(j,i) = static_cast<const SIMD<DimMappedIntegrationPoint<D>>&>(ir[i]).GetNV()(j).Data();
    }

    /*
    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, FlatArray<AFlatMatrix<double>*> input,
                           AFlatMatrix<double> values) const override 
    {
      Evaluate (ir, values);
    }

    virtual void EvaluateDeriv (const SIMD_BaseMappedIntegrationRule & ir, 
                                AFlatMatrix<double> values, AFlatMatrix<double> deriv) const
    {
      Evaluate (ir, values);
      deriv = 0.0;
    }
    
    virtual void EvaluateDeriv (const SIMD_BaseMappedIntegrationRule & ir,
                                FlatArray<AFlatMatrix<>*> input,
                                FlatArray<AFlatMatrix<>*> dinput,
                                AFlatMatrix<> result,
                                AFlatMatrix<> deriv) const
    {
      Evaluate (ir, result);
      deriv = 0.0;
    }
    */
    
    virtual CF_Type GetType() const override { return CF_Type_normal_vector; }
    virtual void DoArchive (Archive & archive) override
    {
      int dim = D;
      archive & dim;
      CoefficientFunction::DoArchive(archive);
    }
    
  };

  template <int D>
  class TangentialVectorCF : public CoefficientFunctionNoDerivative
  {
  public:
    TangentialVectorCF () : CoefficientFunctionNoDerivative(D,false) { ; }
    // virtual int Dimension() const { return D; }

    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      return 0;
    }
    virtual void Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<> res) const 
    {
      if (ip.Dim() != D)
        throw Exception("illegal dim of tangential vector");
      res = static_cast<const DimMappedIntegrationPoint<D>&>(ip).GetTV();
    }
    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const {
        string miptype;
        if(code.is_simd)
          miptype = "SIMD<DimMappedIntegrationPoint<"+ToLiteral(D)+">>*";
        else
          miptype = "DimMappedIntegrationPoint<"+ToLiteral(D)+">*";
        auto tv_expr = CodeExpr("static_cast<const "+miptype+">(&ip)->GetTV()");
        auto tv = Var("tmp", index);
        code.body += tv.Assign(tv_expr);
        for( int i : Range(D))
          code.body += Var(index,i).Assign(tv(i));
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, BareSliceMatrix<SIMD<double>> values) const
    {
      for (size_t i = 0; i < ir.Size(); i++)
        for (size_t j = 0; j < D; j++)
          values(j,i) = static_cast<const SIMD<DimMappedIntegrationPoint<D>>&>(ir[i]).GetTV()(j).Data();
    }

    virtual CF_Type GetType() const { return CF_Type_tangential_vector; }
    virtual void DoArchive (Archive & archive)
    {
      int dim = D;
      archive & dim;
      CoefficientFunction::DoArchive(archive);
    }
  };



void ExportCoefficientFunction(py::module &m)
{
  m.def ("IfPos", [] (shared_ptr<CF> c1, py::object then_obj, py::object else_obj)
            {
              return IfPos(c1,
                           MakeCoefficient(then_obj),
                           MakeCoefficient(else_obj));
            }, py::arg("c1"), py::arg("then_obj"), py::arg("else_obj") ,docu_string(R"raw_string(
Returns new CoefficientFunction with values then_obj if c1 is positive and else_obj else.

Parameters:

c1 : ngsolve.CoefficientFunction
  Indicator function

then_obj : object
  Values of new CF if c1 is positive, object must be implicitly convertible to
  ngsolve.CoefficientFunction. See help(:any:`CoefficientFunction` ) for information.

else_obj : object
  Values of new CF if c1 is not positive, object must be implicitly convertible to
  ngsolve.CoefficientFunction. See help(:any:`CoefficientFunction` ) for information.

)raw_string"))
    ;
  
  m.def("CoordCF", 
        [] (int direction)
        { return MakeCoordinateCoefficientFunction(direction); }, py::arg("direction"),
        docu_string(R"raw_string(
CoefficientFunction for x, y, z.

Parameters:

direction : int
  input direction

)raw_string"));
  
  class MeshSizeCF : public CoefficientFunctionNoDerivative
  {
  public:
    MeshSizeCF () : CoefficientFunctionNoDerivative(1, false) { ; }
    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const override
    {
      if (ip.IP().FacetNr() != -1) // on a boundary facet of the element
        {
          double det = 1;
          switch (ip.Dim())
            {
            case 1: det = fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet()); break;
            case 2: det = fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()); break;
            case 3: det = fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()); break;
            default:
              throw Exception("Illegal dimension in MeshSizeCF");
            }
          return det/ip.GetMeasure();
        }
      
      switch (ip.Dim() - int(ip.VB()))
        {
        case 0: throw Exception ("don't have mesh-size on 0-D boundary");
        case 1: return fabs (static_cast<const ScalMappedIntegrationPoint<>&> (ip).GetJacobiDet());
        case 2: return pow (fabs (static_cast<const ScalMappedIntegrationPoint<>&> (ip).GetJacobiDet()), 1.0/2);
        case 3: default:
          return pow (fabs (static_cast<const ScalMappedIntegrationPoint<>&> (ip).GetJacobiDet()), 1.0/3);
        }
      // return pow(ip.GetMeasure(), 1.0/(ip.Dim());
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, BareSliceMatrix<SIMD<double>> values) const override
    {
      if (ir[0].IP().FacetNr() != -1)
        for(size_t i : Range(ir))
          values(i) =  fabs (ir[i].GetJacobiDet()) / ir[i].GetMeasure();
      else
        for(size_t i : Range(ir))
          values(i) =  pow(fabs (ir[i].GetJacobiDet()), 1.0/ir.DimElement()).Data();
    }

    /*
    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, FlatArray<AFlatMatrix<double>*> input,
                           AFlatMatrix<double> values) const
    {
      Evaluate (ir, values);
    }    
    */
    
    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const override {
      if(code.is_simd)
      {
        string type = "SIMD<double>";
        code.body += Var(index).Declare(type);
        code.body += "if (mir[0].IP().FacetNr() != -1)\n{";
        code.body +=  Var(index).Assign( CodeExpr("fabs (ip.GetJacobiDet()) / ip.GetMeasure()"), false );
        code.body += "}else\n";
        code.body += Var(index).Assign( CodeExpr("pow(fabs(ip.GetJacobiDet()), 1.0/mir.DimElement())"), false);
      }
      else
      {
        code.body += Var(index).Declare( "double" );
        code.body += R"CODE_(
        {
          double tmp_res = 0.0;
          if (ip.IP().FacetNr() != -1)
          {
          double det = 1;
          switch (ip.Dim())
            {
            case 1: det = fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet()); break;
            case 2: det = fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()); break;
            case 3: det = fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()); break;
            default:
              throw Exception("Illegal dimension in MeshSizeCF");
            }
          tmp_res = det/ip.GetMeasure();
          }
          else
          {
          switch (ip.Dim()) {
            case 1:  tmp_res =      fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet()); break;
            case 2:  tmp_res = pow (fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()), 1.0/2); break;
            default: tmp_res = pow (fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()), 1.0/3);
            }
          }
        )CODE_" + Var(index).S() + " = tmp_res;\n}\n;";
      }
    }

    virtual CF_Type GetType() const override { return CF_Type_mesh_size; }
  };


  class SpecialCoefficientFunctions
  {
  public:
    shared_ptr<CF> GetMeshSizeCF ()
    { return make_shared<MeshSizeCF>(); }

    shared_ptr<CF> GetNormalVectorCF (int dim)
    { 
      switch(dim)
	{ 
	case 1:
	  return make_shared<NormalVectorCF<1>>();
	case 2:
	  return make_shared<NormalVectorCF<2>>();
	case 3:
	  return make_shared<NormalVectorCF<3>>();
	case 4:
	  return make_shared<NormalVectorCF<4>>();
	case 5:
	  return make_shared<NormalVectorCF<5>>();
	case 6:
	  return make_shared<NormalVectorCF<6>>();
        default:
          throw Exception (string("Normal-vector not implemented for dimension")
                           +ToString(dim));
	}
    }

    shared_ptr<CF> GetTangentialVectorCF (int dim)
    { 
      switch(dim)
	{
	case 1:
	  return make_shared<TangentialVectorCF<1>>();
	case 2:
	  return make_shared<TangentialVectorCF<2>>();
	default:
	  return make_shared<TangentialVectorCF<3>>();
	}
    }
  };

  ExportStdMathFunction<GenericSin>(m, "sin", "Sine of argument in radians");
  ExportStdMathFunction<GenericCos>(m, "cos", "Cosine of argument in radians");
  ExportStdMathFunction<GenericTan>(m, "tan", "Tangent of argument in radians");
  ExportStdMathFunction<GenericExp>(m, "exp", "Exponential function");
  ExportStdMathFunction<GenericLog>(m, "log", "Logarithm function");
  ExportStdMathFunction<GenericATan>(m, "atan", "Inverse tangent in radians");
  ExportStdMathFunction<GenericACos>(m, "acos", "Inverse cosine in radians");
  ExportStdMathFunction<GenericASin>(m, "asin", "Inverse sine in radians");
  ExportStdMathFunction<GenericSqrt>(m, "sqrt", "Square root function");
  ExportStdMathFunction<GenericFloor>(m, "floor", "Round to next lower integer");
  ExportStdMathFunction<GenericCeil>(m, "ceil", "Round to next greater integer");
  ExportStdMathFunction<GenericConj>(m, "Conj", "Conjugate imaginary part of complex number");

  ExportStdMathFunction2<GenericATan2>(m, "atan2", "Four quadrant inverse tangent in radians");
  ExportStdMathFunction2<GenericPow>(m, "pow", "Power function");

  py::class_<SpecialCoefficientFunctions> (m, "SpecialCFCreator")
    .def_property_readonly("mesh_size", 
                  &SpecialCoefficientFunctions::GetMeshSizeCF, "local mesh-size (approximate element diameter) as CF")
    .def("normal", &SpecialCoefficientFunctions::GetNormalVectorCF, py::arg("dim"),
         "depending on contents: normal-vector to geometry or element\n"
         "space-dimension must be provided")
    .def("tangential", &SpecialCoefficientFunctions::GetTangentialVectorCF, py::arg("dim"),
         "depending on contents: tangential-vector to element\n"
         "space-dimension must be provided")
    ;
  static SpecialCoefficientFunctions specialcf;
  
  m.attr("specialcf") = py::cast(&specialcf);

  py::enum_<CF_Type>(m, "CFtype")
    .value("undefined", CF_Type_undefined)
    .value("constant", CF_Type_constant)
    .value("vectorial", CF_Type_vectorial)
    .value("coordinate", CF_Type_coordinate)
    .value("norm", CF_Type_norm)
    .value("trans", CF_Type_trans)
    .value("component", CF_Type_component)
    .value("real", CF_Type_real)
    .value("imag", CF_Type_imag)
    .value("ifpos", CF_Type_ifpos)
    .value("normal_vector", CF_Type_normal_vector)
    .value("tangential_vector", CF_Type_tangential_vector)
    .value("mesh_size", CF_Type_mesh_size)
    .value("scale", CF_Type_scale)
    .value("scale_complex", CF_Type_scale_complex)
    .value("add", CF_Type_add)
    .value("sub", CF_Type_sub)
    .value("mult", CF_Type_mult)
    .value("div", CF_Type_div)
    .value("domainconst", CF_Type_domainconst)
    .value("domainwise", CF_Type_domainwise)
    .value("unary_op", CF_Type_unary_op)
    .value("binary_op", CF_Type_binary_op)
    .value("usertype", CF_Type_usertype)
    .value("eig", CF_Type_eig)
    ;

  
  
  py::class_<CoefficientFunction, shared_ptr<CoefficientFunction>>
    (m, "CoefficientFunction",
R"raw(A CoefficientFunction (CF) is some function defined on a mesh.
Examples are coordinates x, y, z, domain-wise constants, solution-fields, ...
CFs can be combined by mathematical operations (+,-,sin(), ...) to form new CFs
Parameters:

val : can be one of the following:

  scalar (float or complex):
    Creates a constant CoefficientFunction with value val

  tuple of scalars or CoefficientFunctions:
    Creates a vector or matrix valued CoefficientFunction, use dims=(h,w)
    for matrix valued CF
  list of scalars or CoefficientFunctions:
    Creates a domain-wise CF, use with generator expressions and mesh.GetMaterials()
    and mesh.GetBoundaries()
)raw", py::dynamic_attr())
    .def(py::init([] (py::object val, py::object dims)
        {
          shared_ptr<CoefficientFunction> coef;
          
          py::extract<shared_ptr<CF>> ecf(val);
          if (ecf.check())
            coef = UnaryOpCF (ecf(), [](auto x) { return x; }, " ");
          else
            coef = MakeCoefficient(val);
          if(dims)
            {
              try {
                Array<int> cdims = makeCArray<int> (dims);
                coef->SetDimensions(cdims);
              }
              catch (py::type_error){ }
            }
          return coef;
        }),
        py::arg("coef"),py::arg("dims")=DummyArgument(),
         "Construct a CoefficientFunction from either one of\n"
         "  a scalar (float or complex)\n"
         "  a tuple of scalars and or CFs to define a vector-valued CF\n"
         "     use dims=(h,w) to define matrix-valued CF\n"
         "  a list of scalars and or CFs to define a domain-wise CF"
        )

    .def(py::init([] (CF_Type type, py::list childs, py::list data)
                  {
                    auto cchilds = makeCArraySharedPtr<shared_ptr<CoefficientFunction>> (childs);
                    return MakeCoefficientFunction (type, cchilds, data);
                  }),
         py::arg("type"), py::arg("childs"), py::arg("data")
         )
    
    .def("__str__",  [](CF& self) { return ToString<>(self);})

    .def("__call__", [] (CF& self, BaseMappedIntegrationPoint & mip) -> py::object
	  {
	    if (!self.IsComplex())
	      {
                if (self.Dimension() == 1)
                  return py::cast(self.Evaluate(mip));
                Vector<> vec(self.Dimension());
                self.Evaluate (mip, vec);
                py::tuple res(self.Dimension());
                for (auto i : Range(vec))
                  res[i] = py::cast(vec[i]);
                return res;
	      }
	    else
	      {
                Vector<Complex> vec(self.Dimension());
                self.Evaluate (mip, vec);
                if (vec.Size()==1) return py::cast(vec(0));
                py::tuple res(self.Dimension());
                for (auto i : Range(vec))
                  res[i] = py::cast(vec[i]);
                return res;
	      }
	  },
         py::arg("mip"),
         "evaluate CF at a mapped integrationpoint mip. mip can be generated by calling mesh(x,y,z)")
    .def("__call__", [](shared_ptr<CF> self, py::array_t<MeshPoint> points) -> py::array
         {
           auto pts = points.unchecked<1>(); // pts has array access without bounds checks
           size_t npoints = pts.shape(0);
           py::array np_array;
           if (!self->IsComplex())
             {
               Array<double> vals(npoints * self->Dimension());
               ParallelFor(Range(npoints), [&](size_t i)
                           {
                             LocalHeapMem<1000> lh("CF evaluate");
                             auto& mp = pts(i);
                             auto& trafo = mp.mesh->GetTrafo(ElementId(mp.vb, mp.nr), lh);
                             auto& mip = trafo(IntegrationPoint(mp.x,mp.y,mp.z),lh);
                             FlatVector<double> fv(self->Dimension(), &vals[i*self->Dimension()]);
                             self->Evaluate(mip, fv);
                           });
               np_array = MoveToNumpyArray(vals);
             }
           else
             {
               Array<Complex> vals(npoints * self->Dimension());
               ParallelFor(Range(npoints), [&](size_t i)
                           {
                             LocalHeapMem<1000> lh("CF evaluate");
                             auto& mp = pts(i);
                             auto& trafo = mp.mesh->GetTrafo(ElementId(mp.vb, mp.nr), lh);
                             auto& mip = trafo(IntegrationPoint(mp.x,mp.y,mp.z),lh);
                             FlatVector<Complex> fv(self->Dimension(), &vals[i*self->Dimension()]);
                             self->Evaluate(mip, fv);
                           });
               np_array = MoveToNumpyArray(vals);
             }
           return np_array.attr("reshape")(npoints, self->Dimension());
         })
    .def_property_readonly("dim",
         [] (CF& self) { return self.Dimension(); } ,
                  "number of components of CF")

    /*
    .def_property_readonly("dims",
         [] (PyCF self) { return self->Dimensions(); } ,
                  "shape of CF:  (dim) for vector, (h,w) for matrix")    
    */
    .def_property("dims",
                  [] (shared_ptr<CF> self) { return Array<int>(self->Dimensions()); } ,
                  [] (shared_ptr<CF> self, py::tuple tup) { self->SetDimensions(makeCArray<int>(tup)); } , py::arg("tuple"),
                  "shape of CF:  (dim) for vector, (h,w) for matrix")
    
    .def_property_readonly("is_complex",
                           [] (CF &  self) { return self.IsComplex(); },
                           "is CoefficientFunction complex-valued ?")
    
    .def("__getitem__",  [](shared_ptr<CF> self, int comp)
                                         {
                                           if (comp < 0 || comp >= self->Dimension())
                                             throw py::index_error();
                                           return MakeComponentCoefficientFunction (self, comp);
                                         },
         py::arg("comp"),         
         "returns component comp of vectorial CF")
    .def("__getitem__",  [](shared_ptr<CF> self, py::tuple comps)
                                         {
                                           if (py::len(comps) != 2)
                                             throw py::index_error();
                                           FlatArray<int> dims = self->Dimensions();
                                           if (dims.Size() != 2)
                                             throw py::index_error();
                                           
                                           int c1 = py::extract<int> (comps[0])();
                                           int c2 = py::extract<int> (comps[1])();
                                           if (c1 < 0 || c2 < 0 || c1 >= dims[0] || c2 >= dims[1])
                                             throw py::index_error();

                                           int comp = c1 * dims[1] + c2;
                                           return MakeComponentCoefficientFunction (self, comp);
                                         }, py::arg("components"))

    // coefficient expressions
    .def ("__add__", [] (shared_ptr<CF> c1, shared_ptr<CF> c2) { return c1+c2; }, py::arg("cf") )
    .def ("__add__", [] (shared_ptr<CF> coef, double val)
           {
             return coef + make_shared<ConstantCoefficientFunction>(val);
           }, py::arg("value"))
    .def ("__radd__", [] (shared_ptr<CF> coef, double val)
          { return coef + make_shared<ConstantCoefficientFunction>(val); }, py::arg("value"))

    .def ("__sub__", [] (shared_ptr<CF> c1, shared_ptr<CF> c2)
          { return c1-c2; }, py::arg("cf"))

    .def ("__sub__", [] (shared_ptr<CF> coef, double val)
          { return coef - make_shared<ConstantCoefficientFunction>(val); }, py::arg("value"))

    .def ("__rsub__", [] (shared_ptr<CF> coef, double val)
          { return make_shared<ConstantCoefficientFunction>(val) - coef; }, py::arg("value"))

    .def ("__mul__", [] (shared_ptr<CF> c1, shared_ptr<CF> c2)
           {
             return c1*c2;
           }, py::arg("cf") )

    .def ("__pow__", [] (shared_ptr<CF> c1, int p)
           {
             shared_ptr<CF> one = make_shared<ConstantCoefficientFunction>(1.0);
             if(p==0) return one;

             unsigned n = abs(p);
             shared_ptr<CF> square = c1;
             shared_ptr<CF> res;

             // exponentiation by squaring
             while(n)
             {
               if(n%2)
               {
                 // check if res was not yet assigned any value
                 res = res ? res*square : square;
               }
               square = square*square;
               n /= 2;
             }

             if(p<0)
               return one/res;
             else
               return res;
           }, py::arg("exponent") )

    .def ("__pow__", binary_math_functions["pow"])

    .def ("__pow__", [] (shared_ptr<CF> c1, double val)
           {
             GenericPow func;
	     auto c2 = make_shared<ConstantCoefficientFunction>(val);
             return binary_math_functions["pow"](c1, c2);
           }, py::arg("exponent") )  

    .def ("InnerProduct", [] (shared_ptr<CF> c1, shared_ptr<CF> c2)
           { 
             return InnerProduct (c1, c2);
           }, py::arg("cf"), docu_string(R"raw_string( 
Returns InnerProduct with another CoefficientFunction.

Parameters:

cf : ngsolve.CoefficientFunction
  input CoefficientFunction

 )raw_string"))
    
    .def("Norm",  NormCF, "Returns Norm of the CF")

    .def("Eig", EigCF, "Returns eigenvectors and eigenvalues of matrix-valued CF")
    
    .def ("Other", MakeOtherCoefficientFunction,
          "Evaluate on other element, as needed for DG jumps")
    
    // it's using the complex functions anyway ...
    // it seems to take the double-version now
    .def ("__mul__", [] (shared_ptr<CF> coef, double val)
           {
             return val * coef; 
           }, py::arg("value"))
    .def ("__rmul__", [] (shared_ptr<CF> coef, double val)
          { return val * coef; }, py::arg("value")
           )

    .def ("__mul__", [] (shared_ptr<CF> coef, Complex val)
           {
             if (val.imag() == 0)
               return val.real() * coef;
             else
               return val * coef;
           }, py::arg("value"))
    .def ("__rmul__", [] (shared_ptr<CF> coef, Complex val)
           { 
             if (val.imag() == 0)
               return val.real() * coef;
             else
               return val * coef;
           }, py::arg("value"))

    .def ("__truediv__", [] (shared_ptr<CF> coef, shared_ptr<CF> coef2)
           { return coef/coef2;
           }, py::arg("cf"))

    .def ("__truediv__", [] (shared_ptr<CF> coef, double val)
           // { return coef.Get() * make_shared<ConstantCoefficientFunction>(1/val); })
          { return (1/val) * coef; }, py::arg("value"))

    .def ("__truediv__", [] (shared_ptr<CF> coef, Complex val)
          { return (1.0/val) * coef; }, py::arg("value"))

    .def ("__rtruediv__", [] (shared_ptr<CF> coef, double val)
          { return make_shared<ConstantCoefficientFunction>(val) / coef; }, py::arg("value"))
    .def ("__rtruediv__", [] (shared_ptr<CF> coef, Complex val)
          { return make_shared<ConstantCoefficientFunctionC>(val) / coef; }, py::arg("value"))

    .def ("__neg__", [] (shared_ptr<CF> coef)
           { return -1.0 * coef; })

    .def_property_readonly ("trans", [] (shared_ptr<CF> coef)
                    {
                      return TransposeCF(coef);
                    },
                   "transpose of matrix-valued CF")
    .def_property_readonly ("real", [](shared_ptr<CF> coef) { return Real(coef); }, "real part of CF")
    .def_property_readonly ("imag", [](shared_ptr<CF> coef) { return Imag(coef); }, "imaginary part of CF")

    .def ("Compile", [] (shared_ptr<CF> coef, bool realcompile, int maxderiv, bool wait)
           { return Compile (coef, realcompile, maxderiv, wait); },
           py::arg("realcompile")=false,
           py::arg("maxderiv")=2,
           py::arg("wait")=false, docu_string(R"raw_string(
Compile list of individual steps, experimental improvement for deep trees

Parameters:

realcompile : bool
  True -> Compile to C++ code

maxderiv : int
  input maximal derivative

wait : bool
  True -> Waits until the previous Compile call is finished before start compiling

)raw_string"))


    .def_property_readonly ("type", [](shared_ptr<CF> cf) { return cf->GetType(); })
    
    .def_property("data",
                  [] (shared_ptr<CF> cf)
                  {
                    PyOutArchive ar;
                    cf->DoArchive(ar);
                    return ar.GetList();
                  },
                  [] (shared_ptr<CF> cf, py::list data)
                  {
                    PyInArchive ar(data);
                    cf->DoArchive(ar);
                  })
    .def_property_readonly("childs", [](shared_ptr<CF> cf)
                  {
                    py::list pychilds;
                    for (auto child : cf->InputCoefficientFunctions())
                      pychilds.append (child);
                    return pychilds;
                  })
                  
    
    .def (py::pickle([] (CoefficientFunction & cf)
                     {
                       PyOutArchive ar;
                       cf.DoArchive(ar);
                       Array<shared_ptr<CoefficientFunction>> childs = cf.InputCoefficientFunctions();
                       py::list pychilds;
                       for (auto child : childs)
                         {
                           auto gfchild = dynamic_pointer_cast<ngcomp::GridFunction> (child);
                           if (gfchild)
                             pychilds.append (py::cast(gfchild));
                           else
                             pychilds.append (py::cast(child));
                         }
                       return py::make_tuple(int(cf.GetType()), pychilds, ar.GetList());
                     },
                     [] (py::tuple state)
                     {
                       CF_Type type = CF_Type(py::cast<int>(state[0]));
                       auto childs = makeCArraySharedPtr<shared_ptr<CoefficientFunction>> (py::cast<py::list>(state[1]));
                       py::list pylist = py::cast<py::list>(state[2]);;
                       PyInArchive ar(pylist);
                       shared_ptr<CoefficientFunction> cf;
                       string name;
                       int dim;
                       switch (type)
                         {
                         case CF_Type_undefined:
                           cout << "undefined CF" << endl;
                           break;
                         case CF_Type_constant:
                           cf = make_shared<ConstantCoefficientFunction>(1);
                           break;
                         case CF_Type_vectorial:
                           cf = MakeVectorialCoefficientFunction(move(childs));
                           break;
                         case CF_Type_coordinate:
                           cf = MakeCoordinateCoefficientFunction(-1);
                           break;
                         case CF_Type_norm:
                           cf = NormCF(childs[0]);
                           break;
                         case CF_Type_trans:
                           cf = TransposeCF(childs[0]);
                           break;
                         case CF_Type_component:
                           cf = MakeComponentCoefficientFunction (childs[0], 0);
                           break;
                         case CF_Type_real:
                           cf = Real(childs[0]);
                           break;
                         case CF_Type_imag:
                           cf = Imag(childs[0]);
                           break;
                         case CF_Type_ifpos:
                           cf = IfPos(childs[0], childs[1], childs[2]);
                           break;
                         case CF_Type_normal_vector:
                           dim = py::cast<int>(pylist[0]);
                           cf = specialcf.GetNormalVectorCF(dim);
                           break;
                         case CF_Type_tangential_vector:
                           dim = py::cast<int>(pylist[0]);
                           cf = specialcf.GetTangentialVectorCF(dim);
                           break;
                         case CF_Type_mesh_size:
                           cf = specialcf.GetMeshSizeCF();
                           break;
                         case CF_Type_scale:
                           cf = 1.0 * childs[0];
                           break;
                         case CF_Type_scale_complex:
                           cf = Complex(1.0) * childs[0];
                           break;
                         case CF_Type_add:
                           cf = childs[0] + childs[1];
                           break;
                         case CF_Type_sub:
                           cf = childs[0] - childs[1];
                           break;
                         case CF_Type_mult:
                           cf = childs[0] * childs[1];
                           break;
                         case CF_Type_div:
                           cf = childs[0] / childs[1];
                           break;
                         case CF_Type_domainconst:
                           DomainConstantCoefficientFunction(Array<double>{});
                           break;
                         case CF_Type_domainwise:
                           MakeDomainWiseCoefficientFunction(move(childs));
                           break;
                         case CF_Type_unary_op:
                           name = py::cast<string>(pylist[0]);
                           cf = unary_math_functions[name](childs[0]);
                           break;
                         case CF_Type_binary_op:
                           name = py::cast<string>(pylist[0]);
                           cf = binary_math_functions[name](childs[0], childs[1]);
                           break;
                           /*
                         case CF_Type_usertype:
                           break;
                           */
                         default:
                           cout << "undefined cftype" << endl;
                         }
                       if (cf)
                         cf->DoArchive(ar);
                       return cf;
                     }))
    ;

  typedef shared_ptr<ParameterCoefficientFunction> spParameterCF;
  py::class_<ParameterCoefficientFunction, spParameterCF, CF>
    (m, "Parameter", docu_string(R"raw_string(
CoefficientFunction with a modifiable value

Parameters:

value : float
  Parameter value

)raw_string"))
    .def (py::init ([] (double val)
                    { return make_shared<ParameterCoefficientFunction>(val); }), py::arg("value"), "Construct a ParameterCF from a scalar")
    .def ("Set", [] (spParameterCF cf, double val)  { cf->SetValue (val); }, py::arg("value"),
          docu_string(R"raw_string(
Modify parameter value.

Parameters:

value : double
  input scalar  

)raw_string"))
    .def ("Get", [] (spParameterCF cf)  { return cf->GetValue(); },
          "return parameter value")
    .def (py::pickle([] (shared_ptr<ParameterCoefficientFunction> self)
                     {
                       return py::make_tuple(self->GetValue());
                     },
                     [] (py::tuple state)
                     {
                       double val = py::cast<double>(state[0]);
                       return make_shared<ParameterCoefficientFunction>(val);
                     }))
    ;

  py::class_<BSpline, shared_ptr<BSpline> > (m, "BSpline",R"raw(
BSpline of arbitrary order

Parameters:

order : int
  order of the BSpline

knots : list
  list of float

vals : list
  list of float

)raw")
    .def(py::init
         ([](int order, py::list knots, py::list vals)
          {
            return make_shared<BSpline> (order,
                                         makeCArray<double> (knots),
                                         makeCArray<double> (vals));
          }), py::arg("order"), py::arg("knots"), py::arg("vals"),
        "B-Spline of a certain order, provide knot and value vectors")
    .def("__str__", &ToString<BSpline>)
    .def("__call__", &BSpline::Evaluate)
    .def("__call__", [](shared_ptr<BSpline> sp, shared_ptr<CF> coef)
          {
            return UnaryOpCF (coef, GenericBSpline(sp) /* , GenericBSpline(sp) */);
          }, py::arg("cf"))
    .def("Integrate", 
         [](const BSpline & sp) { return make_shared<BSpline>(sp.Integrate()); }, "Integrate the BSpline")
    .def("Differentiate", 
         [](const BSpline & sp) { return make_shared<BSpline>(sp.Differentiate()); }, "Differentiate the BSpline")
    ;
}


  shared_ptr<CoefficientFunction>
    MakeCoefficientFunction (CF_Type type,
                             const Array<shared_ptr<CoefficientFunction>> & childs,
                             py::list data)
  {
    PyInArchive ar(data);
    shared_ptr<CoefficientFunction> cf;
    switch (type)
      {
      case CF_Type_undefined:
        cout << "undefined CF" << endl;
        break;
      case CF_Type_constant:
        cf = make_shared<ConstantCoefficientFunction>(1);
        break;
      case CF_Type_vectorial:
        cf = MakeVectorialCoefficientFunction(Array<shared_ptr<CoefficientFunction>>(childs));
        break;
      case CF_Type_coordinate:
        cf = MakeCoordinateCoefficientFunction(-1);
        break;
      case CF_Type_norm:
        cf = NormCF(childs[0]);
        break;
      case CF_Type_trans:
        cf = TransposeCF(childs[0]);
        break;
      case CF_Type_component:
        cf = MakeComponentCoefficientFunction (childs[0], 0);
        break;
      case CF_Type_real:
        cf = Real(childs[0]);
        break;
      case CF_Type_imag:
        cf = Imag(childs[0]);
        break;
      case CF_Type_ifpos:
        cf = IfPos(childs[0], childs[1], childs[2]);
        break;
        /*
      case CF_Type_normal_vector:
        {
          int dim = py::cast<int>(data[0]);
          cf = specialcf.GetNormalVectorCF(dim);
          break;
        }
      case CF_Type_tangential_vector:
        {
          int dim = py::cast<int>(data[0]);
          cf = specialcf.GetTangentialVectorCF(dim);
          break;
        }
      case CF_Type_mesh_size:
        cf = specialcf.GetMeshSizeCF();
        break;
        */
      case CF_Type_scale:
        cf = 1.0 * childs[0];
        break;
      case CF_Type_scale_complex:
        cf = Complex(1.0) * childs[0];
        break;
      case CF_Type_add:
        cf = childs[0] + childs[1];
        break;
      case CF_Type_sub:
        cf = childs[0] - childs[1];
        break;
      case CF_Type_mult:
        cf = childs[0] * childs[1];
        break;
      case CF_Type_div:
        cf = childs[0] / childs[1];
        break;
      case CF_Type_domainconst:
        DomainConstantCoefficientFunction(Array<double>{});
        break;
      case CF_Type_domainwise:
        MakeDomainWiseCoefficientFunction(Array<shared_ptr<CoefficientFunction>>(childs));
        break;
      case CF_Type_unary_op:
        {
          string name = py::cast<string>(data[0]);
          cf = unary_math_functions[name](childs[0]);
          break;
        }
      case CF_Type_binary_op:
        {
          string name = py::cast<string>(data[0]);
          cf = binary_math_functions[name](childs[0], childs[1]);
          break;
        }
        /*
          case CF_Type_usertype:
          break;
        */
      default:
        cout << "undefined cftype" << endl;
      }
    if (cf)
      cf->DoArchive(ar);
    return cf;
    
  }



// *************************************** Export FEM ********************************


void NGS_DLL_HEADER ExportNgfem(py::module &m) {

  py::enum_<ELEMENT_TYPE>(m, "ET", "Enumeration of all supported element types.")
    .value("POINT", ET_POINT)     .value("SEGM", ET_SEGM)
    .value("TRIG", ET_TRIG)       .value("QUAD", ET_QUAD)
    .value("TET", ET_TET)         .value("PRISM", ET_PRISM)
    .value("PYRAMID", ET_PYRAMID) .value("HEX", ET_HEX)
    .export_values()
    ;

  py::enum_<NODE_TYPE>(m, "NODE_TYPE", "Enumeration of all supported node types.")
    .value("VERTEX", NT_VERTEX)
    .value("EDGE", NT_EDGE)
    .value("FACE", NT_FACE)
    .value("CELL", NT_CELL)
    .value("ELEMENT", NT_ELEMENT)
    .value("FACET", NT_FACET)
    .export_values()
    ;


  py::class_<ElementTopology> (m, "ElementTopology", docu_string(R"raw_string(
Element Topology

Parameters:

et : ngsolve.fem.ET
  input element type

)raw_string"))
    .def(py::init<ELEMENT_TYPE>(), py::arg("et"))
    .def_property_readonly("name", 
                           static_cast<const char*(ElementTopology::*)()> (&ElementTopology::GetElementName), "Name of the element topology")
    .def_property_readonly("vertices", [](ElementTopology & self)
                                              {
                                                py::list verts;
                                                const POINT3D * pts = self.GetVertices();
                                                int dim = self.GetSpaceDim();
                                                for (int i : Range(self.GetNVertices()))
                                                  {
                                                    py::list v;
                                                    for (int j = 0; j < dim; j++)
                                                      v.append(py::cast(pts[i][j]));
                                                    verts.append (py::tuple(v));
                                                  }
                                                return verts;
                                              }, "Vertices of the element topology");
    ;
    
  py::class_<FiniteElement, shared_ptr<FiniteElement>>
    (m, "FiniteElement", "any finite element")
    .def_property_readonly("ndof", &FiniteElement::GetNDof, "number of degrees of freedom of element")    
    .def_property_readonly("order", &FiniteElement::Order, "maximal polynomial order of element")    
    .def_property_readonly("type", &FiniteElement::ElementType, "geometric type of element")    
    .def_property_readonly("dim", &FiniteElement::Dim, "spatial dimension of element")    
    .def_property_readonly("classname", &FiniteElement::ClassName, "name of element family")  
    .def("__str__", &ToString<FiniteElement>)
    // .def("__timing__", [] (FiniteElement & fel) { return py::cast(fel.Timing()); })
    .def("__timing__", &FiniteElement::Timing)
    ;

  py::class_<BaseScalarFiniteElement, shared_ptr<BaseScalarFiniteElement>, 
    FiniteElement>
      (m, "ScalarFE", "a scalar-valued finite element")
    .def("CalcShape",
         [] (const BaseScalarFiniteElement & fe, double x, double y, double z)
          {
            IntegrationPoint ip(x,y,z);
            Vector<> v(fe.GetNDof());
            fe.CalcShape (ip, v);
            return v;
          },
         py::arg("x"),py::arg("y")=0.0,py::arg("z")=0.0,docu_string(R"raw_string(
Parameters:

x : double
  input x value

y : double
  input y value

z : double
  input z value

)raw_string"))
    .def("CalcShape",
         [] (const BaseScalarFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Vector<> v(fe.GetNDof());
            fe.CalcShape (mip.IP(), v);
            return v;
          },
         py::arg("mip"),docu_string(R"raw_string(
Parameters:

mip : ngsolve.BaseMappedIntegrationPoint
  input mapped integration point

)raw_string"))
    .def("CalcDShape",
         [] (const BaseScalarFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Matrix<> mat(fe.GetNDof(), fe.Dim());
            fe.CalcMappedDShape(mip, mat);
            /*
            switch (fe.Dim())
              {
              case 1:
                dynamic_cast<const ScalarFiniteElement<1>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<1,1>&> (mip), mat); break;
              case 2:
                dynamic_cast<const ScalarFiniteElement<2>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<2,2>&> (mip), mat); break;
              case 3:
                dynamic_cast<const ScalarFiniteElement<3>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<3,3>&> (mip), mat); break;
              default:
                ;
              }
            */
            return mat;
          },
         py::arg("mip"),docu_string(R"raw_string(
Computes derivative of the shape in an integration point.

Parameters:

mip : ngsolve.BaseMappedIntegrationPoint
  input mapped integration point

)raw_string"))
    ;


  py::class_<BaseHCurlFiniteElement, shared_ptr<BaseHCurlFiniteElement>, 
             FiniteElement>
    (m, "HCurlFE", "an H(curl) finite element")
    .def("CalcShape",
         [] (const BaseHCurlFiniteElement & fe, double x, double y, double z)
         {
           IntegrationPoint ip(x,y,z);
           Matrix<> mat(fe.GetNDof(), fe.Dim());
           fe.CalcShape (ip, mat);
           return mat;
         },
         py::arg("x"),py::arg("y")=0.0,py::arg("z")=0.0)
    .def("CalcShape",
         [] (const BaseHCurlFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Matrix<> mat(fe.GetNDof(), fe.Dim());
            fe.CalcMappedShape (mip, mat);
            return mat;
          },
         py::arg("mip"))
    .def("CalcCurlShape",
         [] (const BaseHCurlFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Matrix<> mat(fe.GetNDof(), fe.Dim());
            fe.CalcMappedCurlShape(mip, mat);
            return mat;
          },
         py::arg("mip"))
    ;





  

  py::implicitly_convertible 
    <BaseScalarFiniteElement, 
    FiniteElement >(); 


  m.def("H1FE", [](ELEMENT_TYPE et, int order)
           {
             BaseScalarFiniteElement * fe = nullptr;
             switch (et)
               {
               case ET_TRIG: fe = new H1HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new H1HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new H1HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return shared_ptr<BaseScalarFiniteElement>(fe);
           }, py::arg("et"), py::arg("order"),
          docu_string(R"raw_string(Creates an H1 finite element of given geometric shape and polynomial order.

Parameters:

et : ngsolve.fem.ET
  input element type

order : int
  input polynomial order

)raw_string")
          );

  m.def("L2FE", [](ELEMENT_TYPE et, int order)
           {
             BaseScalarFiniteElement * fe = nullptr;
             switch (et)
               {
               case ET_TRIG: fe = new L2HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new L2HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new L2HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return shared_ptr<BaseScalarFiniteElement>(fe);
           }, py::arg("et"), py::arg("order"),
          docu_string(R"raw_string(Creates an L2 finite element of given geometric shape and polynomial order.

Parameters:

et : ngsolve.fem.ET
  input element type

order : int
  input polynomial order

)raw_string")
          );


  py::class_<IntegrationPoint>(m, "IntegrationPoint")
    .def_property_readonly("point", [](IntegrationPoint& self)
                           {
                             return py::make_tuple(self.Point()[0], self.Point()[1], self.Point()[2]);
                           }, "Integration point coordinates as tuple, has always x,y and z component, which do not have meaning in lesser dimensions")
    .def_property_readonly("weight", &IntegrationPoint::Weight, "Weight of the integration point")
    ;

  py::class_<IntegrationRule>(m, "IntegrationRule", docu_string(R"raw_string(
Integration rule

2 __init__ overloads


1)

Parameters:

element type : ngsolve.fem.ET
  input element type

order : int
  input order of integration rule


2)

Parameters:

points : list
  input list of integration points

weights : list
  input list of integration weights

)raw_string"))
    .def(py::init
         ([](ELEMENT_TYPE et, int order)
          { return new IntegrationRule (et, order); }),
         py::arg("element type"), py::arg("order"))
    
    .def(py::init
         ([](py::list points, py::list weights)
          {
            IntegrationRule * ir = new IntegrationRule ();
            for (size_t i = 0; i < len(points); i++)
              {
                py::object pnt = points[i];
                IntegrationPoint ip;
                ip.SetNr(i);
                for (int j = 0; j < len(pnt); j++)
                  ip(j) = py::extract<double> (py::tuple(pnt)[j])();
                ip.SetWeight(py::extract<double> (weights[i])());
                ir -> Append (ip);
              }
            return ir;
          }),
         py::arg("points"), py::arg("weights"))
    .def("__str__", &ToString<IntegrationRule>)
    .def("__getitem__", [](IntegrationRule & ir, int nr)
                                        {
                                          if (nr < 0 || nr >= ir.Size())
                                            throw py::index_error();
                                          return ir[nr];
                                        }, py::arg("nr"), "Return integration point at given position")
    .def("Integrate", [](IntegrationRule & ir, py::object func) -> py::object
          {
            py::object sum;
            bool first = true;
            for (const IntegrationPoint & ip : ir)
              {
                py::object val;
                switch (ir.Dim())
                  {
                  case 1:
                    val = func(ip(0)); break;
                  case 2:
                    val = func(ip(0), ip(1)); break;
                  case 3:
                    val = func(ip(0), ip(1), ip(2)); break;
                  default:
                    throw Exception("integration rule with illegal dimension");
                  }

                val = val.attr("__mul__")(py::cast((double)ip.Weight()));
                if (first)
                  sum = val;
                else
                  sum = sum.attr("__add__")(val);
                first = false;
              }
            return sum;
          }, py::arg("func"), "Integrates a given function")
    .def_property_readonly("weights", [] (IntegrationRule& self)
                           {
                             py::list weights;
                             for (auto ip : self)
                               weights.append(ip.Weight());
                             return weights;
                           }, "Weights of IntegrationRule")
    .def_property_readonly("points", [] (IntegrationRule& self)
                           {
                             py::list points;
                             for(auto ip : self)
                               switch(self.Dim())
                                 {
                                 case 1:
                                   points.append(py::make_tuple(ip.Point()[0]));
                                   break;
                                 case 2:
                                   points.append(py::make_tuple(ip.Point()[0],
                                                                ip.Point()[1]));
                                   break;
                                 default:
                                   points.append(py::make_tuple(ip.Point()[0],
                                                                ip.Point()[1],
                                                                ip.Point()[2]));
                                 }
                             return points;
                           }, "Points of IntegrationRule as tuple")
    ;


  py::class_<MeshPoint>(m, "MeshPoint");

  if (have_numpy)
  {
    py::detail::npy_format_descriptor<MeshPoint>::register_dtype({
        py::detail::field_descriptor { "x", offsetof(MeshPoint, x), sizeof(double),
            py::format_descriptor<double>::format(), py::detail::npy_format_descriptor<double>::dtype() },
          py::detail::field_descriptor { "y", offsetof(MeshPoint, y), sizeof(double),
              py::format_descriptor<double>::format(), py::detail::npy_format_descriptor<double>::dtype() },
            py::detail::field_descriptor { "z", offsetof(MeshPoint, z), sizeof(double),
                py::format_descriptor<double>::format(), py::detail::npy_format_descriptor<double>::dtype() },
            py::detail::field_descriptor { "meshptr", offsetof(MeshPoint, mesh), sizeof(double),
                py::format_descriptor<double>::format(), py::detail::npy_format_descriptor<double>::dtype() },
            py::detail::field_descriptor { "VorB", offsetof(MeshPoint, vb), sizeof(int),
                py::format_descriptor<int>::format(), py::detail::npy_format_descriptor<int>::dtype() },
              py::detail::field_descriptor {"nr", offsetof(MeshPoint, nr), sizeof(int),
                  py::format_descriptor<int>::format(), py::detail::npy_format_descriptor<int>::dtype()}});
  }

  py::class_<BaseMappedIntegrationPoint>(m, "BaseMappedIntegrationPoint")
    .def(py::init([](MeshPoint pnt)
                  {
                    auto& trafo = pnt.mesh->GetTrafo(ElementId(pnt.vb, pnt.nr), global_alloc);
                    auto& mip = trafo(IntegrationPoint(pnt.x,pnt.y,pnt.z),global_alloc);
                    mip.SetOwnsTrafo(true);
                    return &mip;
                  }))
    .def("__str__",
         [] (const BaseMappedIntegrationPoint & bmip)
          {
            stringstream str;
            if (bmip.IsComplex())
            {
              str << "p = " << bmip.GetPointComplex() << endl;
              str << "jac = " << bmip.GetJacobianComplex() << endl;
            }
            else 
            {
              str << "p = " << bmip.GetPoint() << endl;
              str << "jac = " << bmip.GetJacobian() << endl;
            }
            /*
            switch (bmip.Dim())
              {
              case 1: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<1,1>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              case 2: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<2,2>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              case 3: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<3,3>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              default:
                ;
              }
            */
            str << "measure = " << bmip.GetMeasure() << endl;
            return str.str();
          })
    .def_property_readonly("measure", &BaseMappedIntegrationPoint::GetMeasure, "Measure of the mapped integration point ")
    .def_property_readonly("point", &BaseMappedIntegrationPoint::GetPoint, "Point of the mapped integration point")
    .def_property_readonly("jacobi", &BaseMappedIntegrationPoint::GetJacobian, "jacobian of the mapped integration point")
    // .def_property_readonly("trafo", &BaseMappedIntegrationPoint::GetTransformation)
    .def_property_readonly("trafo", &BaseMappedIntegrationPoint::GetTransformation, "Transformation of the mapped integration point")
    .def_property_readonly("elementid", [](BaseMappedIntegrationPoint & mip)
                                               {
                                                 return mip.GetTransformation().GetElementId();
                                               }, "Element ID of the mapped integration point")
    ;

  py::implicitly_convertible<MeshPoint, BaseMappedIntegrationPoint>();

  py::class_<ElementTransformation, shared_ptr<ElementTransformation>>(m, "ElementTransformation")
    .def(py::init([] (ELEMENT_TYPE et, py::list vertices)
        -> shared_ptr<ElementTransformation>
        {
          int nv = ElementTopology::GetNVertices(et);
          int dim = py::len(vertices[0]);
          Matrix<> pmat(nv,dim);
          for (int i : Range(nv))
            for (int j : Range(dim))
              pmat(i,j) = py::extract<double> (vertices[py::int_(i)][py::int_(j)])();
          switch (Dim(et))
            {
            case 1:
              return make_shared<FE_ElementTransformation<1,1>> (et, pmat);
            case 2:
              return make_shared<FE_ElementTransformation<2,2>> (et, pmat);
            case 3:
              return make_shared<FE_ElementTransformation<3,3>> (et, pmat);
            default:
              throw Exception ("cannot create ElementTransformation");
            }
        }),
        py::arg("et")=ET_TRIG,py::arg("vertices"))
    .def_property_readonly("VB", &ElementTransformation::VB, "VorB (VOL, BND, BBND, BBBND)")
    .def_property_readonly("spacedim", &ElementTransformation::SpaceDim, "Space dimension of the element transformation")
    .def_property_readonly("elementid", &ElementTransformation::GetElementId, "Element ID of the element transformation")
    .def ("__call__", [] (shared_ptr<ElementTransformation> self, double x, double y, double z)
           {
             
             return &(*self)(IntegrationPoint(x,y,z), global_alloc);
           },
          py::arg("x"), py::arg("y")=0, py::arg("z")=0,
          py::return_value_policy::reference)
    .def ("__call__", [] (shared_ptr<ElementTransformation> self, IntegrationPoint & ip)
           {
             return &(*self)(ip, global_alloc);
           },
          py::arg("ip"),
          py::return_value_policy::reference)
    ;


  py::class_<DifferentialOperator, shared_ptr<DifferentialOperator>>
    (m, "DifferentialOperator")
    ;

  typedef BilinearFormIntegrator BFI;
  auto bfi_class = py::class_<BFI, shared_ptr<BFI>> (m, "BFI", docu_string(R"raw_string(
Bilinear Form Integrator

Parameters:

name : string
  Name of the bilinear form integrator.

py_coef : object
  CoefficientFunction of the bilinear form.

dim : int
  dimension of the bilinear form integrator

imag : bool
  Multiplies BFI with 1J

filename : string
  filename 

kwargs : kwargs
  For a description of the possible kwargs have a look a bit further down.

)raw_string"), py::dynamic_attr());
  bfi_class
    .def(py::init([bfi_class] (const string name, py::object py_coef, int dim, bool imag,
                      string filename, py::kwargs kwargs)
        -> shared_ptr<BilinearFormIntegrator>
        {
          auto flags = CreateFlagsFromKwArgs(bfi_class,kwargs);
          Array<shared_ptr<CoefficientFunction>> coef = MakeCoefficients(py_coef);
          auto bfi = GetIntegrators().CreateBFI (name, dim, coef);

          if (!bfi) cerr << "undefined integrator '" << name
                         << "' in " << dim << " dimension" << endl;

          if (filename.length())
            {
              cout << "set integrator filename: " << filename << endl;
              bfi -> SetFileName (filename);
            }
          bfi -> SetFlags (flags);
          if (imag)
            bfi = make_shared<ComplexBilinearFormIntegrator> (bfi, Complex(0,1));
          bfi_class.attr("__initialize__")(bfi,**kwargs);
          return bfi;
        }),
        py::arg("name")="",
        py::arg("coef"),py::arg("dim")=-1,
        py::arg("imag")=false, py::arg("filename")=""
        )
    .def_static("__flags_doc__", [] ()
         {
           return py::dict
             (
              py::arg("dim") = "int = -1\n"
              "Dimension of integrator. If -1 then dim is set when integrator is\n"
              "added to BilinearForm",
              py::arg("definedon") = "ngsolve.Region\n"
              "Region the integrator is defined on. Regions can be obtained by i.e.\n"
              "mesh.Materials('regexp') or mesh.Boundaries('regexp'). If not set\n"
              "integration is done on all volume elements",
              py::arg("definedonelem") = "ngsolve.BitArray\n"
              "Element wise integrator definition."
              );
         })
    .def_static("__special_treated_flags__", [] ()
         {
           return py::dict
             (
              py::arg("definedonelem") = py::cpp_function([](py::object,Flags*,py::list) { ; }),
              py::arg("definedon") = py::cpp_function ([] (py::object, Flags*, py::list) { ; })
              );
         })
    .def("__initialize__", [] (shared_ptr<BFI> self, py::kwargs kwargs)
         {
           if(kwargs.contains("definedon"))
             {
               auto definedon = kwargs["definedon"];
               auto definedon_list = py::extract<py::list>(definedon);
               if (definedon_list.check())
                 {
                   Array<int> defon = makeCArray<int> (definedon_list());
                   for (int & d : defon) d--;
                   self->SetDefinedOn (defon);
                 }
               else if (py::extract<BitArray> (definedon).check())
                 self->SetDefinedOn (py::extract<BitArray> (definedon)());
               else if (!py::extract<DummyArgument>(definedon).check())
                 throw Exception (string ("cannot handle definedon of type <todo>"));
             }
           if(kwargs.contains("definedonelem"))
               self->SetDefinedOnElements(py::cast<shared_ptr<BitArray>>(kwargs["definedonelem"]));
         })
    .def("__str__",  [](shared_ptr<BFI> self) { return ToString<BilinearFormIntegrator>(*self); } )

    .def("Evaluator",  [](shared_ptr<BFI> self, string name ) { return self->GetEvaluator(name); }, py::arg("name"), docu_string(R"raw_string(
Returns requested evaluator

Parameters:

name : string
  input name of requested evaluator

)raw_string") )
    // .def("DefinedOn", &Integrator::DefinedOn)
    .def("GetDefinedOn",  [] (shared_ptr<BFI> self) -> const BitArray &{ return self->GetDefinedOn(); } ,
         py::return_value_policy::reference, "Returns a BitArray where the bilinear form is defined on")

    .def("SetDefinedOnElements",  [](shared_ptr<BFI> self, shared_ptr<BitArray> ba )
         { self->SetDefinedOnElements (ba); }, py::arg("bitarray"), docu_string(R"raw_string( 
Set the elements on which the bilinear form is defined on.

Parameters:

bitarray : ngsolve.ngstd.BitArray
  input bitarray

)raw_string") )
    .def("SetIntegrationRule", [] (shared_ptr<BFI> self, ELEMENT_TYPE et, IntegrationRule ir)
         {
           self -> SetIntegrationRule(et,ir);
           return self;
         }, py::arg("et"), py::arg("intrule"), docu_string(R"raw_string( 
Set integration rule of the bilinear form.

Parameters:

et : ngsolve.fem.Element_Type
  input element type

intrule : ngsolve.fem.Integrationrule
  input integration rule

)raw_string"))
    .def("CalcElementMatrix",
         [] (shared_ptr<BFI> self,
             const FiniteElement & fe, const ElementTransformation &trafo,
             size_t heapsize, bool complex)
                         {
                           while (true)
                             {
                               try
                                 {
                                   LocalHeap lh(heapsize);
                                   if (complex)
                                     {
                                       Matrix<Complex> mat(fe.GetNDof() * self->GetDimension());
                                       self->CalcElementMatrix(fe,trafo,mat,lh);
                                       return py::cast(mat);
                                     }
                                   else
                                     {
                                       Matrix<> mat(fe.GetNDof() * self->GetDimension());
                                       self->CalcElementMatrix (fe, trafo, mat, lh);
                                       return py::cast(mat);
                                     }
                                 }
                               catch (LocalHeapOverflow ex)
                                 {
                                   heapsize *= 10;
                                 }
                             }
                         },
         py::arg("fel"),py::arg("trafo"),py::arg("heapsize")=10000, py::arg("complex") = false, docu_string(R"raw_string( 
Calculate element matrix of a specific element.

Parameters:

fel : ngsolve.fem.FiniteElement
  input finite element

trafo : ngsolve.fem.ElementTransformation
  input element transformation

heapsize : int
  input heapsize

complex : bool
  input complex

)raw_string"))
    ;


  m.def("CompoundBFI", 
          []( shared_ptr<BFI> bfi, int comp )
                            {
                                return make_shared<CompoundBilinearFormIntegrator>(bfi, comp);
                            },
           py::arg("bfi")=NULL, py::arg("comp")=0, docu_string(R"raw_string(
Compound Bilinear Form Integrator

Parameters:

bfi : ngsolve.fem.BFI
  input bilinear form integrator

comp : int
  input component

)raw_string"));

  m.def("BlockBFI", 
          []( shared_ptr<BFI> bfi, int dim, int comp )
                            {
                                return make_shared<BlockBilinearFormIntegrator>(bfi, dim, comp);
                            },
           py::arg("bfi")=NULL, py::arg("dim")=2, py::arg("comp")=0
      , docu_string(R"raw_string(
Block Bilinear Form Integrator

Parameters:

bfi : ngsolve.fem.BFI
  input bilinear form integrator

dim : int
  input dimension of block bilinear form integrator

comp : int
  input comp

)raw_string"));

  typedef LinearFormIntegrator LFI;
  py::class_<LFI, shared_ptr<LFI>>
    (m, "LFI", docu_string(R"raw_string(
Linear Form Integrator

Parameters:

name : string
  Name of the linear form integrator.

dim : int
  dimension of the linear form integrator

coef : object
  CoefficientFunction of the bilinear form.

definedon : object
  input region where the linear form is defined on

imag : bool
  Multiplies LFI with 1J

flags : ngsolve.ngstd.Flags
  input flags

definedonelem : object
  input definedonelem

)raw_string"), py::dynamic_attr())
    .def(py::init([] (string name, int dim,
                      py::object py_coef,
                      py::object definedon, bool imag, const Flags & flags,
                      py::object definedonelem)
                  {
                    Array<shared_ptr<CoefficientFunction>> coef = MakeCoefficients(py_coef);
                    auto lfi = GetIntegrators().CreateLFI (name, dim, coef);

                    if (!lfi) throw Exception(string("undefined integrator '")+name+
                                              "' in "+ToString(dim)+ " dimension having 1 coefficient");

                    if(hasattr(definedon,"Mask"))
                      {
                        auto vb = py::cast<VorB>(definedon.attr("VB")());
                        if(vb != lfi->VB())
                          throw Exception(string("LinearFormIntegrator ") + name + " not defined for " +
                                          (vb==VOL ? "VOL" : (vb==BND ? "BND" : "BBND")));
                        lfi->SetDefinedOn(py::cast<BitArray>(definedon.attr("Mask")()));
                      }
                    if (py::extract<py::list> (definedon).check())
                      {
                        Array<int> defon = makeCArray<int> (definedon);
                        for (int & d : defon) d--;
                        lfi -> SetDefinedOn (defon);
                      }
                    if (! py::extract<DummyArgument> (definedonelem).check())
                      lfi -> SetDefinedOnElements (py::extract<shared_ptr<BitArray>>(definedonelem)());

                    if (imag)
                      lfi = make_shared<ComplexLinearFormIntegrator> (lfi, Complex(0,1));
                    return lfi;
                  }),
         py::arg("name")=NULL,py::arg("dim")=-1,
         py::arg("coef"),py::arg("definedon")=DummyArgument(),
         py::arg("imag")=false, py::arg("flags")=py::dict(),
         py::arg("definedonelements")=DummyArgument())

    .def("__str__",  [](shared_ptr<LFI> self) { return ToString<LinearFormIntegrator>(*self); } )
    
    // .def("GetDefinedOn", &Integrator::GetDefinedOn)
    .def("GetDefinedOn",  [] (shared_ptr<LFI> self) -> const BitArray &{ return self->GetDefinedOn(); } ,
         py::return_value_policy::reference, "Reterns regions where the lienar form integrator is defined on.")
    .def("SetDefinedOnElements",  [](shared_ptr<LFI> self, shared_ptr<BitArray> ba )
         { self->SetDefinedOnElements (ba); }, py::arg("ba"), docu_string(R"raw_string(
Set the elements on which the linear form integrator is defined on

Parameters:

ba : ngsolve.ngstd.BitArray
  input bit array ( 1-> defined on, 0 -> not defoned on)

)raw_string"))
    .def("SetIntegrationRule", [](shared_ptr<LFI> self, ELEMENT_TYPE et, IntegrationRule ir)
         {
           self->SetIntegrationRule(et,ir);
           return self;
         }, py::arg("et"), py::arg("ir"), docu_string(R"raw_string(
Set a different integration rule for elements of type et

Parameters:

et : ngsolve.fem.ET
  input element type

ir : ngsolve.fem.IntegrationRule
  input integration rule

)raw_string"))

    .def("CalcElementVector", 
         static_cast<void(LinearFormIntegrator::*)(const FiniteElement&, const ElementTransformation&, FlatVector<double>, LocalHeap&)const>(&LinearFormIntegrator::CalcElementVector), py::arg("fel"), py::arg("trafo"), py::arg("vec"), py::arg("lh"))
    .def("CalcElementVector",
         [] (shared_ptr<LFI>  self, const FiniteElement & fe, const ElementTransformation& trafo,
             size_t heapsize, bool complex)
         {
           while (true)
             {
               try
                 {
                   LocalHeap lh(heapsize);
                   if (complex)
                     {
                       Vector<Complex> vec(fe.GetNDof() * self->GetDimension());
                       self->CalcElementVector(fe,trafo,vec,lh);
                       return py::cast(vec);
                     }
                   else
                     {
                       Vector<> vec(fe.GetNDof() * self->GetDimension());
                       self->CalcElementVector (fe, trafo, vec, lh);
                       return py::cast(vec);
                     }
                 }
               catch (LocalHeapOverflow ex)
                 {
                   heapsize *= 10;
                 }
             };
         },
         py::arg("fel"),py::arg("trafo"),py::arg("heapsize")=10000, py::arg("complex")=false)
    ;



  m.def("CompoundLFI", 
          []( shared_ptr<LFI> lfi, int comp )
                            {
                                return shared_ptr<LFI>(make_shared<CompoundLinearFormIntegrator>(lfi, comp));
                            },
           "lfi"_a=NULL, py::arg("comp")=0, docu_string(R"raw_string(
Compound Linear Form Integrator

Parameters:

lfi : ngsolve.fem.LFI
  input linear form integrator

comp : int
  input component

)raw_string"));

  m.def("BlockLFI", 
          []( shared_ptr<LFI> lfi, int dim, int comp )
                            {
                                return shared_ptr<LFI>(make_shared<BlockLinearFormIntegrator>(lfi, dim, comp));
                            },
           "lfi"_a=NULL, py::arg("dim")=2, py::arg("comp")=0, docu_string(R"raw_string(
Block Linear Form Integrator

Parameters:

lfi : ngsolve.fem.LFI
  input bilinear form integrator

dim : int
  input dimension of block linear form integrator

comp : int
  input comp

)raw_string"));


  ExportCoefficientFunction (m);

  m.def ("SetPMLParameters", 
           [] (double rad, double alpha)
                           {
                             cout << "set pml parameters, r = " << rad << ", alpha = " << alpha << endl;
                             constant_table_for_FEM = &pmlpar;
                             pmlpar.Set("pml_r", rad);
                             pmlpar.Set("pml_alpha", alpha);
                             SetPMLParameters();
                           },
         py::arg("rad")=1,py::arg("alpha")=1, docu_string(R"raw_string(
Parameters:

rad : double
  input radius of PML

alpha : double
  input damping factor of PML

)raw_string"));
    
                           
  m.def("GenerateL2ElementCode", &GenerateL2ElementCode);

}


PYBIND11_MODULE(libngfem, m) {
  m.attr("__name__") = "fem";
  ExportNgfem(m);
}



#endif
