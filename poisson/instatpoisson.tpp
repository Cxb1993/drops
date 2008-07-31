//**************************************************************************
// File:    instatpoisson.tpp                                              *
// Content: classes that constitute the poisson-problem                    *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, Marcus Soemers      *
//          IGPM RWTH Aachen                                               *
// Version: 0.1                                                            *
// History: begin - November, 11 2002                                      *
//**************************************************************************

#include "num/discretize.h"

namespace DROPS
{


template<class Coeff>
void InstatPoissonP1CL<Coeff>::CreateNumbering(Uint level, IdxDescCL* idx)
// used for numbering of the Unknowns depending on the index IdxDesc[idxnum].
// sets up the description of the index idxnum in IdxDesc[idxnum],
// allocates memory for the Unknown-Indices on TriangLevel level und numbers them.
{
    // allocate space for indices; number unknowns in TriangLevel level
    CreateNumb( level, *idx, _MG, GetBndData() );
}


//========================================================
//
//                Set up matrices and rhs
//
//========================================================

inline double Quad2D(const TetraCL& t, Uint face, Uint vert, InstatPoissonBndDataCL::bnd_val_fun bfun, double time)
// Integrate nat_val() * phi_vert over face
{
    Point3DCL vc[3];
    const VertexCL* v[3];

    v[0]= t.GetVertex(vert);
    for (Uint i=0, k=1; i<3; ++i)
    {
        if (VertOfFace(face,i)!=vert)
            v[k++]= t.GetVertex( VertOfFace(face,i) );
        vc[i]= v[i]->GetCoord();
    }
    const double f0= bfun(vc[0], time);
    const double f1= bfun(vc[1], time) +  bfun( vc[2], time);
    const double f2= bfun(1./3.*(vc[0] + vc[1] + vc[2]), time);    //Barycenter of Face
    const double absdet= FuncDet2D(vc[1] - vc[0], vc[2] - vc[0]);
    return (11./240.*f0 + 1./240.*f1 + 9./80.*f2) * absdet;
}

template<class Coeff>
void InstatPoissonP1CL<Coeff>::SetupGradSrc(VecDescCL& src, scalar_instat_fun_ptr T, scalar_instat_fun_ptr dalpha, double t) const
{
  src.Clear();
  const Uint lvl = src.GetLevel(),
             idx = src.RowIdx->GetIdx();
  Point3DCL G[4];

  double det;
  double absdet;
  IdxT UnknownIdx[4];
  Quad2CL<> rhs, quad_a;

  for (MultiGridCL::const_TriangTetraIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangTetraBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangTetraEnd(lvl);
    sit != send; ++sit)
  {
    P1DiscCL::GetGradients(G,det,*sit);
    absdet= std::fabs(det);

    quad_a.assign( *sit, dalpha, t);
    const double int_a= quad_a.quad( absdet);
    Point3DCL gradT;

    for(int i=0; i<4; ++i)
    {
      gradT+= G[i]*T(sit->GetVertex(i)->GetCoord(), t);
      UnknownIdx[i]= sit->GetVertex(i)->Unknowns.Exist(idx) ? sit->GetVertex(i)->Unknowns(idx)
                                                            : NoIdx;
    }

    for(int i=0; i<4;++i)    // assemble row i
    {
      if (sit->GetVertex(i)->Unknowns.Exist(idx)) // vertex i is not on a Dirichlet boundary
      {
        src.Data[UnknownIdx[i]]-= int_a*inner_prod( gradT, G[i]);
        if ( _BndData.IsOnNatBnd(*sit->GetVertex(i)) )
          for (int f=0; f < 3; ++f)
            if ( sit->IsBndSeg(FaceOfVert(i, f)) )
            {
              Point3DCL n;
              sit->GetOuterNormal(FaceOfVert(i, f), n);
              src.Data[UnknownIdx[i]]+=
                Quad2D(*sit, FaceOfVert(i, f), i, dalpha, t) * inner_prod( gradT, n);
            }
      }
    }
  }
}

template<class Coeff>
void InstatPoissonP1CL<Coeff>::SetupInstatRhs(VecDescCL& vA, VecDescCL& vM, double tA, VecDescCL& vf, double tf) const
// Sets up the time dependent right hand sides including couplings
// resulting from inhomogeneous dirichlet bnd conditions
{
  vA.Clear();
  vM.Clear();
  vf.Clear();

  const Uint lvl = vA.GetLevel(),
             idx = vA.RowIdx->GetIdx();
  Point3DCL G[4];

  double coup[4][4];
  double det;
  double absdet;
  IdxT UnknownIdx[4];
  Quad2CL<> rhs, quad_a;

//  StripTimeCL strip( &Coeff::f, tf);

//  if (_Coeff.SpecialRhs)
//      _Coeff.ComputeRhs( vf, tf, _MG);

  for (MultiGridCL::const_TriangTetraIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangTetraBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangTetraEnd(lvl);
    sit != send; ++sit)
  {
    P1DiscCL::GetGradients(G,det,*sit);
    absdet= std::fabs(det);

    quad_a.assign( *sit, _Coeff.alpha, tA);
    const double int_a= quad_a.quad( absdet);

    for(int i=0; i<4; ++i)
    {
      for(int j=0; j<=i; ++j)
      {
        // dot-product of the gradients
        coup[i][j]= inner_prod( G[i], G[j])*int_a;
        // coup[i][j]+= P1DiscCL::Quad(*sit, &Coeff::q, i, j)*absdet;
        coup[j][i]= coup[i][j];
      }
      UnknownIdx[i]= sit->GetVertex(i)->Unknowns.Exist(idx) ? sit->GetVertex(i)->Unknowns(idx)
                                                            : NoIdx;
    }

    for(int j=0; j<4; ++j)
      if (!sit->GetVertex(j)->Unknowns.Exist(idx))  // vertex j is on a Dirichlet boundary
      { // coupling with vertex j on right-hand-side
        const double bndval= _BndData.GetDirBndValue(*sit->GetVertex(j), tA);
        for(int i=0; i<4;++i)    // assemble row i
        {
          if (sit->GetVertex(i)->Unknowns.Exist(idx)) // vertex i is not on a Dirichlet boundary
          {
            vA.Data[UnknownIdx[i]]-= coup[j][i] * bndval;
            vM.Data[UnknownIdx[i]]-= P1DiscCL::GetMass( i, j)*absdet * bndval;
          }
        }
      }

    rhs.assign( *sit, _Coeff.f, tf);
    for(int i=0; i<4;++i)    // assemble row i
      if (sit->GetVertex(i)->Unknowns.Exist(idx)) // vertex i is not on a Dirichlet boundary
      {
//        vf.Data[UnknownIdx[i]]+= P1DiscCL::Quad(*sit, &strip.GetFunc, i)*absdet;
//        if (!_Coeff.SpecialRhs)
        vf.Data[UnknownIdx[i]]+= rhs.quadP1( i, absdet);
        if ( _BndData.IsOnNatBnd(*sit->GetVertex(i)) )
          for (int f=0; f < 3; ++f)
            if ( sit->IsBndSeg(FaceOfVert(i, f)) )
              vA.Data[UnknownIdx[i]]+=
                Quad2D(*sit, FaceOfVert(i, f), i, _BndData.GetBndFun( sit->GetBndIdx( FaceOfVert(i,f))), tA);
      }
  }
}


template<class Coeff>
void InstatPoissonP1CL<Coeff>::SetupInstatSystem( MatDescCL& Amat, MatDescCL& Mmat, double tA) const
// Sets up the stiffness matrix and the mass matrix
{
  MatrixBuilderCL A( &Amat.Data, Amat.RowIdx->NumUnknowns, Amat.ColIdx->NumUnknowns);
  MatrixBuilderCL M( &Mmat.Data, Mmat.RowIdx->NumUnknowns, Mmat.ColIdx->NumUnknowns);

  const Uint lvl = Amat.GetRowLevel();
  const Uint idx = Amat.RowIdx->GetIdx();

  Point3DCL G[4];

  double coup[4][4];
  double det;
  double absdet;
  IdxT UnknownIdx[4];
  Quad2CL<> quad_a;

  for (MultiGridCL::const_TriangTetraIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangTetraBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangTetraEnd(lvl);
    sit != send; ++sit)
  {
    P1DiscCL::GetGradients(G,det,*sit);
    absdet= std::fabs(det);

    quad_a.assign( *sit, _Coeff.alpha, tA);
    const double int_a= quad_a.quad( absdet);

    for(int i=0; i<4; ++i)
    {
      for(int j=0; j<=i; ++j)
      {
        // dot-product of the gradients
        coup[i][j]= inner_prod( G[i], G[j])*int_a;
        // coup[i][j]+= P1DiscCL::Quad(*sit, &Coeff::q, i, j)*absdet;
        coup[j][i]= coup[i][j];
      }
      UnknownIdx[i]= sit->GetVertex(i)->Unknowns.Exist(idx) ? sit->GetVertex(i)->Unknowns(idx) : NoIdx;
    }

    for(int i=0; i<4; ++i)    // assemble row i
      if (sit->GetVertex(i)->Unknowns.Exist(idx))  // vertex i is not on a Dirichlet boundary
      {
        for(int j=0; j<4;++j)
        {
          if (sit->GetVertex(j)->Unknowns.Exist(idx)) // vertex j is not on a Dirichlet boundary
          {
            A( UnknownIdx[i], UnknownIdx[j])+= coup[j][i];
            M( UnknownIdx[i], UnknownIdx[j])+= P1DiscCL::GetMass( i, j)*absdet;
          }
          // else coupling with vertex j on right-hand-side  --> 0
        }
      }
  }
  A.Build();
  M.Build();
}

template<class Coeff>
void InstatPoissonP1CL<Coeff>::SetupConvection( MatDescCL& Umat, VecDescCL& vU, double t) const
// Sets up matrix and couplings with bnd unknowns for convection term
{
  vU.Clear();
  MatrixBuilderCL U( &Umat.Data, Umat.RowIdx->NumUnknowns, Umat.ColIdx->NumUnknowns);

  const Uint lvl = Umat.GetRowLevel();
  const Uint idx = Umat.RowIdx->GetIdx();

  Point3DCL G[4];

  double det;
  double absdet;
  IdxT UnknownIdx[4];
  Quad2CL<Point3DCL> u;
  for (MultiGridCL::const_TriangTetraIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangTetraBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangTetraEnd(lvl);
    sit != send; ++sit)
  {
    P1DiscCL::GetGradients(G,det,*sit);
    absdet= std::fabs(det);

    for(int i=0; i<4; ++i)
    {
      UnknownIdx[i]= sit->GetVertex(i)->Unknowns.Exist(idx) ? sit->GetVertex(i)->Unknowns(idx) : NoIdx;
      u[i]= _Coeff.Vel( sit->GetVertex(i)->GetCoord(), t);
    }
    u[4]= _Coeff.Vel( GetBaryCenter( *sit), t);

    if (!adjoint_)
    {
        for(int j=0; j<4;++j)
        {
          const Quad2CL<> u_Gradj( dot( u, Quad2CL<Point3DCL>( G[j])));

          if (UnknownIdx[j] != NoIdx) // vertex j is not on a Dirichlet boundary
          {
            for(int i=0; i<4; ++i)    // assemble row i
              if (UnknownIdx[i] != NoIdx)  // vertex i is not on a Dirichlet boundary
              {
                U( UnknownIdx[i], UnknownIdx[j])+= u_Gradj.quadP1( i, absdet);
              }
          }
          else // coupling with vertex j on right-hand-side
          {
            const double bndval= _BndData.GetDirBndValue(*sit->GetVertex(j), t);
            for(int i=0; i<4; ++i)    // assemble row i
              if (UnknownIdx[i] != NoIdx)  // vertex i is not on a Dirichlet boundary
                vU.Data[ UnknownIdx[i]]-= u_Gradj.quadP1( i, absdet) * bndval;
          }
        }
    }
    else // adjoint problem: discretization of u grad phi_i phi_j
    {
        for(int i=0; i<4;++i)
        { // assemble row i

          if (UnknownIdx[i] == NoIdx) continue; // vertex i is on a Dirichlet boundary -> no test function

          const Quad2CL<> u_Gradi( dot( u, Quad2CL<Point3DCL>( G[i])));
          for(int j=0; j<4; ++j)
          {
            const double coupl= u_Gradi.quadP1( j, absdet);
            if (UnknownIdx[j] != NoIdx)  // vertex j is not on a Dirichlet boundary
              U( UnknownIdx[i], UnknownIdx[j])+= coupl;
            else // coupling with vertex j on right-hand-side
              vU.Data[ UnknownIdx[i]]-= coupl* _BndData.GetDirBndValue(*sit->GetVertex(j), t);
          }
        }
    }
  }
  U.Build();
}


template<class Coeff>
void InstatPoissonP1CL<Coeff>::SetupProlongation(MatDescCL& P, IdxDescCL* cIdx, IdxDescCL* fIdx) const
// This only works, if Interpolate is called after every refinement of the multigrid.
{
    SetupP1ProlongationMatrix( _MG, P, cIdx, fIdx);
}


template<class Coeff>
void InstatPoissonP1CL<Coeff>::Init( VecDescCL& vec, scalar_instat_fun_ptr func, double t0) const
{
    Uint lvl= vec.GetLevel(),
         idx= vec.RowIdx->GetIdx();


    for (MultiGridCL::const_TriangVertexIteratorCL sit= const_cast<const MultiGridCL&>(_MG).GetTriangVertexBegin(lvl), send= const_cast<const MultiGridCL&>(_MG).GetTriangVertexEnd(lvl);
         sit != send; ++sit)
    {
        if (sit->Unknowns.Exist(idx))
        {
            vec.Data[sit->Unknowns(idx)]= func( sit->GetCoord(), t0);
        }
    }

}

//========================================================
//
//                Check solution
//
//========================================================

template<class Coeff>
void InstatPoissonP1CL<Coeff>::CheckSolution(const VecDescCL& lsg,
  scalar_instat_fun_ptr Lsg, double t) const
{
  double diff, maxdiff=0, norm2= 0, L2=0;
  Uint lvl=lsg.GetLevel(),
       Idx=lsg.RowIdx->GetIdx();

  DiscSolCL sol(&lsg, &GetBndData(), &GetMG(), t);

  IF_MASTER
    std::cerr << "Abweichung von der tatsaechlichen Loesung:" << std::endl;

  for (MultiGridCL::const_TriangTetraIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangTetraBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangTetraEnd(lvl);
    sit != send; ++sit)
  {
    double absdet= sit->GetVolume()*6.,
           sum= 0;

    for(Uint i=0; i<4; ++i)
    {
      diff= (sol.val(*sit->GetVertex(i)) - Lsg(sit->GetVertex(i)->GetCoord(),t));
      sum+= diff*diff;
    }
    sum/= 120;
    diff= sol.val(*sit, 0.25, 0.25, 0.25) - Lsg(GetBaryCenter(*sit),t);
    sum+= 2./15. * diff*diff;
    L2+= sum*absdet;
  }
  L2= std::sqrt(L2);

  for (MultiGridCL::const_TriangVertexIteratorCL
    sit=const_cast<const MultiGridCL&>(_MG).GetTriangVertexBegin(lvl),
    send=const_cast<const MultiGridCL&>(_MG).GetTriangVertexEnd(lvl);
    sit != send; ++sit)
  {
    if (sit->Unknowns.Exist(idx.GetIdx()))
    {
      diff= std::fabs( Lsg(sit->GetCoord(),t) - lsg.Data[sit->Unknowns(Idx)] );
      norm2+= diff*diff;
      if (diff>maxdiff)
      {
        maxdiff= diff;
      }
    }
  }

  int Lsize = lsg.Data.size();
  IF_MASTER
    std::cout << "  2-Norm= " << std::sqrt(norm2)
              << "\nw-2-Norm= " << std::sqrt(norm2/Lsize)
              << "\nmax-Norm= " << maxdiff
              << "\n L2-Norm= " << std::sqrt(L2) << std::endl;
}


} // end of namespace DROPS
