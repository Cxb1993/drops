//**************************************************************************
// File:    poisson.h                                                      *
// Content: classes that constitute the poisson-problem                    *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
// Version: 0.1                                                            *
// History: begin - March, 16 2001                                         *
//**************************************************************************

#ifndef DROPS_POISSON_H
#define DROPS_POISSON_H

#include "misc/problem.h"
#include "num/fe.h"
#include "num/discretize.h"
#include <deque>
#include <iostream>
#include <numeric>


namespace DROPS
{


class PoissonBndSegDataCL
{
  public:
    typedef double bnd_type;
    typedef bnd_type (*bnd_val_fun)(const Point2DCL&);
    
  private:
    bool        _Neumann;
    bnd_val_fun _bnd_val;
    
  public:
    PoissonBndSegDataCL(bool neumann, bnd_val_fun bnd_val)
        : _Neumann(neumann), _bnd_val(bnd_val) {}
    bool
    IsNeumann() const { return _Neumann; }
    bnd_val_fun GetBndFun() const { return _bnd_val; }
    bnd_type    GetBndVal(const Point2DCL& p) const { return _bnd_val(p); }
};


class PoissonBndDataCL
{
  private:
    std::vector<PoissonBndSegDataCL> _BndData;

  public:
//    typedef PoissonBndDataCL                     self;
    typedef PoissonBndSegDataCL::bnd_type    bnd_type;
    typedef PoissonBndSegDataCL::bnd_val_fun bnd_val_fun;
    typedef PoissonBndSegDataCL              BndSegDataCL;

    PoissonBndDataCL(Uint, const bool*, const bnd_val_fun*);

    inline bool IsOnDirBnd(const VertexCL&) const;
    inline bool IsOnNeuBnd(const VertexCL&) const;
    inline bool IsOnDirBnd(const EdgeCL&) const;
    inline bool IsOnNeuBnd(const EdgeCL&) const;
    inline bool IsOnDirBnd(const FaceCL&) const;
    inline bool IsOnNeuBnd(const FaceCL&) const;
    
    inline bnd_type GetDirBndValue(const VertexCL&, double= 0.0) const;
    inline bnd_type GetDirBndValue(const EdgeCL&, double= 0.0) const;
    inline bnd_type GetDirBndValue(const FaceCL&, double= 0.0) const;
    inline bnd_type GetNeuBndValue(const VertexCL&, double= 0.0) const;
    inline bnd_type GetNeuBndValue(const EdgeCL&, double= 0.0) const;
    inline bnd_type GetNeuBndValue(const FaceCL&, double= 0.0) const;

    bnd_val_fun GetBndFun(BndIdxT i) const { return _BndData[i].GetBndFun(); }
    const BndSegDataCL& GetSegData(BndIdxT i) const { return _BndData[i]; }
};

inline PoissonBndDataCL::PoissonBndDataCL(Uint numbndseg,
                                          const bool* isneumann,
                                          const bnd_val_fun* fun)
{
    _BndData.reserve(numbndseg);
    for (Uint i=0; i<numbndseg; ++i)
        _BndData.push_back( PoissonBndSegDataCL(isneumann[i], fun[i]) );
}


template <class Coeff>
class PoissonP1CL : public ProblemCL<Coeff, PoissonBndDataCL>
{
  public:
    typedef ProblemCL<Coeff, PoissonBndDataCL> _base;
    typedef typename _base::BndDataCL          BndDataCL;
    typedef typename _base::CoeffCL            CoeffCL;
    using                                      _base::_MG;
    using                                      _base::_Coeff;
    using                                      _base::_BndData;
    using                                      _base::GetBndData;
    using                                      _base::GetMG;
    
    typedef P1EvalCL<double, const BndDataCL, VecDescCL>       DiscSolCL;
    typedef P1EvalCL<double, const BndDataCL, const VecDescCL> const_DiscSolCL;
    typedef double (*est_fun)(const TetraCL&, const VecDescCL&, const BndDataCL&);

    IdxDescCL idx;
//    IdxDescCL _err_idx;
    VecDescCL x;
    VecDescCL b;
//    VecDescCL _err;
    MatDescCL A;
    
    PoissonP1CL(const MGBuilderCL& mgb, const CoeffCL& coeff, const BndDataCL& bdata)
        : _base( mgb, coeff, bdata), idx( 1) {}  
    // numbering of unknowns
    void CreateNumbering(Uint, IdxDescCL*);
    void DeleteNumbering(IdxDescCL*);
    // set up matrices and rhs
    void SetupSystem         (MatDescCL&, VecDescCL&) const;
    void SetupStiffnessMatrix(MatDescCL&) const;
    void SetupProlongation   (MatDescCL& P, IdxDescCL* cIdx, IdxDescCL* fIdx) const;
    // check computed solution etc.
    double CheckSolution(const VecDescCL&, scalar_fun_ptr) const;
    double CheckSolution(scalar_fun_ptr Lsg) const { return CheckSolution(x, Lsg); }
    void GetDiscError (const MatDescCL&, scalar_fun_ptr) const;
    void GetDiscError (scalar_fun_ptr Lsg) const { GetDiscError(A, Lsg); }

    bool          EstimateError         (const VecDescCL&, const double, double&, est_fun);
    static double ResidualErrEstimator  (const TetraCL&, const VecDescCL&, const BndDataCL&);
    static double ResidualErrEstimatorL2(const TetraCL&, const VecDescCL&, const BndDataCL&);

    DiscSolCL GetSolution()
        { return DiscSolCL(&x, &GetBndData(), &GetMG()); }
    const_DiscSolCL GetSolution() const
        { return const_DiscSolCL(&x, &GetBndData(), &GetMG()); }
};

template <class Coeff>
class PoissonP2CL : public ProblemCL<Coeff, PoissonBndDataCL>
{
  public:
    typedef ProblemCL<Coeff, PoissonBndDataCL> _base;
    typedef typename _base::BndDataCL               BndDataCL;
    typedef typename _base::CoeffCL                 CoeffCL;
    using                                           _base::_MG;
    using                                           _base::_Coeff;
    using                                           _base::_BndData;
    using                                           _base::GetBndData;
    using                                           _base::GetMG;
    
    typedef P2EvalCL<double, const BndDataCL, VecDescCL>       DiscSolCL;
    typedef P2EvalCL<double, const BndDataCL, const VecDescCL> const_DiscSolCL;
    typedef double (*est_fun)(const TetraCL&, const VecDescCL&, const BndDataCL&);

    // new fields for the matrix A, the rhs b and the solution x
    IdxDescCL idx;
    VecDescCL x;
    VecDescCL b;
    MatDescCL A;
    
    //create an element of the class
    PoissonP2CL(const MGBuilderCL& mgb, const CoeffCL& coeff, 
		const BndDataCL& bdata) : _base(mgb, coeff, bdata), idx(1) {}  
		
    // numbering of unknowns
    void CreateNumbering(Uint, IdxDescCL*);
    void DeleteNumbering(IdxDescCL*);
    
    // set up matrices and rhs
    void SetupSystem         (MatDescCL&, VecDescCL&) const;
    void SetupStiffnessMatrix(MatDescCL&) const;
    
    // check computed solution, etc.
    double CheckSolution(const VecDescCL&, scalar_fun_ptr) const;
    double CheckSolution(scalar_fun_ptr Lsg) const { return CheckSolution(x, Lsg); }
};


double SimpleGradEstimator (const TetraCL& t, const VecDescCL& lsg, const PoissonBndDataCL&);

//==============================================================
//                  Marker classes
//==============================================================

template <class _TetraEst, class _ProblemCL>
class PoissonErrEstCL
{
  private:
    double    _InitGlobErr;
    double    _RelReduction;
    double    _ConvExponent;
    double    _Meas;
    double    _ActGlobErr;
    _TetraEst _Estimator;
    _ProblemCL& _Problem;
    Uint      _NumLastMarkedForRef;
    Uint      _NumLastMarkedForDel;
    bool      _DoMark;
    std::ostream*  _outp;

  public:
      PoissonErrEstCL(double RelReduction, double ConvExp, double Meas, bool DoMark, _TetraEst est,
                      _ProblemCL& problem, std::ostream* osp= &std::cout)
        : _InitGlobErr(0), _RelReduction(RelReduction), _ConvExponent(ConvExp), _Meas(Meas), _ActGlobErr(-1), _Estimator(est),
          _Problem(problem), _NumLastMarkedForRef(0), _NumLastMarkedForDel(0), _DoMark(DoMark), _outp(osp)
        {}
    // default assignment-op, copy-ctor, dtor

    template <class _BndData, class _VD>
      void Init(const P1EvalCL<double, _BndData, _VD>&);

    double GetRelRed() { return _RelReduction; }
    void SetRelRed(double newred) { _RelReduction= newred; }
    bool DoesMark() { return _DoMark; }
    void SwitchMark() { _DoMark= _DoMark ? false : true; }
    template <class _BndData, class _VD>
      bool Estimate(const P1EvalCL<double, _BndData, const _VD>&);
};


template <class _TetraEst, class _ProblemCL>
class DoerflerMarkCL
{
  private:
    double        _InitGlobErr;
    double        _RelReduction;
    double        _min_tetra_ratio;
    double        _Threshold;
    double        _Meas;
    double        _ActGlobErr;
    _TetraEst     _Estimator;
    _ProblemCL&   _Problem;
    Uint          _NumLastMarkedForRef;
    Uint          _NumLastMarkedForDel;
    bool          _DoMark;
    std::ostream* _outp;

  public:
  // the tetras are sorted: T_1 with biggest error, last T_n with smallest
  // a tetra T_i is marked for refinement, iff (a) or (b) holds:
  // (a) it is among "min_ratio" % of the tetras with biggest errors,
  // (b) the sum err_1+..+err_i accounts for less than "Threshold" % of the global error

      DoerflerMarkCL(double RelReduction, double min_ratio, double Threshold, double Meas, bool DoMark, _TetraEst est,
                     _ProblemCL& problem, std::ostream* osp= &std::cout)
        : _InitGlobErr(0), _RelReduction(RelReduction), _min_tetra_ratio(min_ratio), _Threshold(Threshold), _Meas(Meas), _ActGlobErr(-1), _Estimator(est),
          _Problem(problem), _NumLastMarkedForRef(0), _NumLastMarkedForDel(0), _DoMark(DoMark), _outp(osp)
        {}
    // default assignment-op, copy-ctor, dtor

    template <class _BndData, class _VD>
      void Init(const P1EvalCL<double, _BndData, _VD>&);

    double GetRelRed() { return _RelReduction; }
    void   SetRelRed(double newred) { _RelReduction= newred; }
    double GetThreshold() { return _Threshold; }
    void   SetThreshold(double newThreshold) { _Threshold= newThreshold; }
    bool   DoesMark() { return _DoMark; }
    void   SwitchMark() { _DoMark= _DoMark ? false : true; }
    template <class _BndData, class _VD>
      bool Estimate(const P1EvalCL<double, _BndData, const _VD>&);
};


//======================================
//   declaration of inline functions 
//======================================

inline bool PoissonBndDataCL::IsOnDirBnd(const VertexCL& v) const
{
    if ( !v.IsOnBoundary() ) return false;
    for (VertexCL::const_BndVertIt it= v.GetBndVertBegin(), end= v.GetBndVertEnd(); it!=end; ++it)
        if ( !_BndData[it->GetBndIdx()].IsNeumann() )
            return true;
    return false; 
}        

inline bool PoissonBndDataCL::IsOnNeuBnd(const VertexCL& v) const
{
    if ( !v.IsOnBoundary() ) return false;
    for (VertexCL::const_BndVertIt it= v.GetBndVertBegin(), end= v.GetBndVertEnd(); it!=end; ++it)
        if ( !_BndData[it->GetBndIdx()].IsNeumann() )
            return false;
    return true;
}        

inline bool PoissonBndDataCL::IsOnDirBnd(const EdgeCL& e) const
{
    if ( !e.IsOnBoundary() ) return false;
    for (const BndIdxT* it= e.GetBndIdxBegin(), *end= e.GetBndIdxEnd(); it!=end; ++it)
        if ( !_BndData[*it].IsNeumann() )
            return true;
    return false;
}

inline bool PoissonBndDataCL::IsOnNeuBnd(const EdgeCL& e) const
{
    if ( !e.IsOnBoundary() ) return false;
    for (const BndIdxT* it= e.GetBndIdxBegin(), *end= e.GetBndIdxEnd(); it!=end; ++it)
        if ( !_BndData[*it].IsNeumann() )
            return false;
    return true;
}        

inline bool PoissonBndDataCL::IsOnDirBnd(const FaceCL& f) const
{
    return f.IsOnBoundary() && !_BndData[f.GetBndIdx()].IsNeumann();
}

inline bool PoissonBndDataCL::IsOnNeuBnd(const FaceCL& f) const
{
    return f.IsOnBoundary() && _BndData[f.GetBndIdx()].IsNeumann();
}        


inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetDirBndValue(const VertexCL& v, double) const
// Returns value of the Dirichlet boundary value. 
// Expects, that there is any Dirichlet boundary ( IsOnDirBnd(...) == true )
{
    for (VertexCL::const_BndVertIt it= v.GetBndVertBegin(), end= v.GetBndVertEnd(); it!=end; ++it)
        if ( !_BndData[it->GetBndIdx()].IsNeumann() )
            return _BndData[it->GetBndIdx()].GetBndVal( it->GetCoord2D() );
    throw DROPSErrCL("GetDirBndValue(VertexCL): No Dirichlet Boundary Segment!");
}


inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetNeuBndValue(const VertexCL& v, double) const
// Returns value of the Neumann boundary value. 
// Expects, that there is any Neumann boundary ( IsOnNeuBnd(...) == true )
{
    for (VertexCL::const_BndVertIt it= v.GetBndVertBegin(), end= v.GetBndVertEnd(); it!=end; ++it)
        if ( _BndData[it->GetBndIdx()].IsNeumann() )
            return _BndData[it->GetBndIdx()].GetBndVal( it->GetCoord2D() );
    throw DROPSErrCL("GetNeuBndValue(VertexCL): No Neumann Boundary Segment!");
}

inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetDirBndValue(const EdgeCL& e, double) const
{
//    std::cout << " GetDirBndValue --->>> " << std::endl;
    for (const BndIdxT* it= e.GetBndIdxBegin(), *end= e.GetBndIdxEnd(); it!=end; ++it)
        if ( !_BndData[*it].IsNeumann() )
	{
//    std::cout << " it = " << *it << std::endl;  
	    Point2DCL bnd_pt;
	    for (int i=0; i<2; ++i)
	    {
//    std::cout << " i = " << i << std::endl;  
	        const VertexCL& v= *e.GetVertex(i);
                for (VertexCL::const_BndVertIt bit= v.GetBndVertBegin(), bend=	v.GetBndVertEnd(); bit!=bend; ++bit)
                    if ( bit->GetBndIdx()== *it)
		    {
//    std::cout << " bit->GetBndIdx() = " << bit->GetBndIdx() << std::endl;  
		        bnd_pt+= 0.5*bit->GetCoord2D();
//			bit= bend;
			break;
		    }
            }
//    std::cout << " bnd_pt " << bnd_pt << std::endl;  
//    std::cout << " GetDirBndValue <<<--- " << std::endl;
            return _BndData[*it].GetBndVal( bnd_pt);
	}
    throw DROPSErrCL("GetDirBndValue(EdgeCL): No Dirichlet Boundary Segment!");
}


inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetNeuBndValue(const EdgeCL& e, double) const
// Returns value of the Neumann boundary value. 
// Expects, that there is any Neumann boundary ( IsOnNeuBnd(...) == true )
{
    for (const BndIdxT* it= e.GetBndIdxBegin(), *end= e.GetBndIdxEnd(); it!=end; ++it)
        if ( _BndData[*it].IsNeumann() )
	{
	    Point2DCL bnd_pt;
	    for (int i=0; i<2; ++i)
	    {
	        const VertexCL& v= *e.GetVertex(i);
                for (VertexCL::const_BndVertIt bit= v.GetBndVertBegin(), bend=	v.GetBndVertEnd(); bit!=bend; ++bit)
                    if ( bit->GetBndIdx()== *it)
		    {
		        bnd_pt+= 0.5*bit->GetCoord2D();
//			bit= bend;
			break;
		    }
            }
            return _BndData[*it].GetBndVal( bnd_pt);
	}
    throw DROPSErrCL("GetNeuBndValue(EdgeCL): No Neumann Boundary Segment!");
}

/*
inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetDirBndValue(const FaceCL& f) const
{
    Assert( !_BndData[f.GetBndIdx()].IsNeumann(), DROPSErrCL("GetDirBndValue(FaceCL): No Dirichlet Boundary Segment!"), ~0);
    return _BndData[f.GetBndIdx()].GetBndVal( GetBaryCenter(f) );
}

inline PoissonBndDataCL::bnd_type PoissonBndDataCL::GetNeuBndValue(const FaceCL& f) const
// Returns value of the Neumann boundary value. 
// Expects, that there is any Neumann boundary ( IsOnNeuBnd(...) == true )
{
    Assert( _BndData[f.GetBndIdx()].IsNeumann(), DROPSErrCL("GetNeuBndValue(FaceCL): No Neumann Boundary Segment!"), ~0);
    return _BndData[f.GetBndIdx()].GetBndVal( GetBaryCenter(f) );
}
*/
} // end of namespace DROPS

#include "poisson/poisson.tpp"

#endif
