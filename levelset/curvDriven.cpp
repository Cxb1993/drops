//**************************************************************************
// File:    curvDriven.cpp                                                 *
// Content: test case for curvature driven flow                            *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//**************************************************************************


#include "geom/multigrid.h"
#include "out/output.h"
#include "geom/builder.h"
#include "navstokes/instatnavstokes2phase.h"
#include "stokes/integrTime.h"
#include "num/stokessolver.h"
#include "out/output.h"
#include "out/ensightOut.h"
#include "levelset/coupling.h"
#include <fstream>

double      delta_t= 0.01;
DROPS::Uint num_steps= 5;
const int   FPsteps= -1;

// du/dt - q*u - nu*laplace u + Dp = f - okn
//                          -div u = 0
//                               u = u0, t=t0


class ZeroFlowCL
{
// \Omega_1 = Tropfen,    \Omega_2 = umgebendes Fluid
  public:
    static DROPS::Point3DCL f(const DROPS::Point3DCL&, double)
        { DROPS::Point3DCL ret(0.0); return ret; }
    const DROPS::SmoothedJumpCL rho, mu;
    const double SurfTens;
    const DROPS::Point3DCL g;

    ZeroFlowCL()
      : rho( DROPS::JumpCL( 1., 1.), DROPS::H_sm, 0.1),
        mu(  DROPS::JumpCL( 1., 1.), DROPS::H_sm, 0.1),
        SurfTens( 0.), g( 0.)    {}
};

// Tropfendaten:
DROPS::Point3DCL Mitte(0.5);
double           Radius= 0.25;

double DistanceFct( const DROPS::Point3DCL& p)
{
    DROPS::Point3DCL d= Mitte-p;
    d[0]/= 1.8;
    return d.norm()-Radius;
}

double sigma;
double sigmaf (const DROPS::Point3DCL&, double) { return sigma; } 

namespace DROPS // for Strategy
{

class Uzawa_PCG_CL : public UzawaSolverCL<PCG_SsorCL>
{
  private:
    PCG_SsorCL _PCGsolver;
  public:
    Uzawa_PCG_CL( MatrixCL& M, int outer_iter, double outer_tol, int inner_iter, double inner_tol, double tau= 1., double omega=1.)
        : UzawaSolverCL<PCG_SsorCL>( _PCGsolver, M, outer_iter, outer_tol, tau),
          _PCGsolver(SSORPcCL(omega), inner_iter, inner_tol)
        {}
};

class PSchur_GSPCG_CL: public PSchurSolverCL<PCG_SgsCL>
{
  private:
    PCG_SgsCL _PCGsolver;
  public:
    PSchur_GSPCG_CL( MatrixCL& M, int outer_iter, double outer_tol, int inner_iter, double inner_tol)
        : PSchurSolverCL<PCG_SgsCL>( _PCGsolver, M, outer_iter, outer_tol),
          _PCGsolver(SGSPcCL(), inner_iter, inner_tol)
        {}
    PCG_SgsCL& GetPoissonSolver() { return _PCGsolver; }
};

template<class StokesProblemT>
void Strategy(StokesProblemT& Stokes, double inner_iter_tol)
// flow control
{
    MultiGridCL& MG= Stokes.GetMG();

    IdxDescCL  lidx;
    IdxDescCL* vidx= &Stokes.vel_idx;
    IdxDescCL* pidx= &Stokes.pr_idx;
    VelVecDescCL* v= &Stokes.v;
    VecDescCL*    p= &Stokes.p;
    VelVecDescCL* b= &Stokes.b;
    VecDescCL* c= &Stokes.c;
    MatDescCL* A= &Stokes.A;
    MatDescCL* B= &Stokes.B;
    MatDescCL* M= &Stokes.M;
    MatDescCL prM;

    LevelsetP2CL lset( MG, &sigmaf, /*grad sigma*/ 0, 0.5, 0.1);

    vidx->Set( 3, 3);
    pidx->Set( 1);
    lidx.Set(  1, 1);

    TimerCL time;
    Stokes.CreateNumberingVel(MG.GetLastLevel(), vidx);
    Stokes.CreateNumberingPr(MG.GetLastLevel(), pidx);
    lset.CreateNumbering( MG.GetLastLevel(), &lidx);
    lset.Phi.SetIdx( &lidx);
    lset.Init( DistanceFct);

    MG.SizeInfo( std::cerr);
    b->SetIdx( vidx);
    c->SetIdx( pidx);
    p->SetIdx( pidx);
    v->SetIdx( vidx);
    std::cerr << "Anzahl der Druck-Unbekannten: " << p->Data.size() << std::endl;
    std::cerr << "Anzahl der Geschwindigkeitsunbekannten: " << v->Data.size() << std::endl;
    A->Reset();
    B->Reset();
    M->Reset();
    A->SetIdx(vidx, vidx);
    B->SetIdx(pidx, vidx);
    M->SetIdx(vidx, vidx);
    Stokes.N.SetIdx(vidx, vidx);
    prM.SetIdx( pidx, pidx);
    time.Reset();
    time.Start();
    Stokes.SetupPrMass( &prM, lset);
    time.Stop();
    std::cerr << time.GetTime() << " seconds for setting up all systems!" << std::endl;

    Stokes.InitVel( v, ZeroVel);
    lset.SetupSystem( Stokes.GetVelSolution() );
/*
{ // output of surface force
VecDescCL curv;
curv.SetIdx( vidx);
lset.AccumulateBndIntegral( curv);
std::cerr << "min/max = " << curv.Data.min() <<", " << curv.Data.max() << std::endl;
std::cerr << curv.Data.size() <<"="<<(M->Data*Stokes.v.Data).size()<<std::endl;
SSORPcCL ssor;
PCG_SsorCL sol(ssor,1000,1e-12);
sol.Solve( M->Data, Stokes.v.Data, curv.Data);
std::cerr << "res= " << sol.GetResid() << "\titer= "<<sol.GetIter()<<std::endl;
std::cerr << "min/max = " << Stokes.v.Data.min() <<", " << Stokes.v.Data.max() << std::endl;

EnsightP2SolOutCL tmp( MG, &lidx);
tmp.CaseBegin( "sf.case");
tmp.DescribeGeom( "cube", "sf.geo");
tmp.DescribeVector( "SurfForce", "sf.vec");
tmp.DescribeVector( "CurvTerm", "sf.crv");
tmp.putGeom( "sf.geo");
tmp.putVector( "sf.vec", Stokes.GetVelSolution() );
typename StokesProblemT::DiscVelSolCL csol( &curv, &Stokes.GetBndData().Vel, &Stokes.GetMG(), Stokes.t);
tmp.putVector( "sf.crv", csol);
tmp.CaseEnd();
v->Clear();
}
*/
    Uint meth;
    std::cerr << "\nwhich method? 0=Uzawa, 1=Schur > "; std::cin >> meth;
    time.Reset();

    double outer_tol;
    std::cerr << "tol = "; std::cin >> outer_tol;

    EnsightP2SolOutCL ensight( MG, &lidx);

    const char datgeo[]= "ensight/curvdriv.geo",
               datpr[] = "ensight/curvdriv.pr",
               datvec[]= "ensight/curvdriv.vec",
               datscl[]= "ensight/curvdriv.scl";
    ensight.CaseBegin( "curvdriv.case", num_steps+1);
    ensight.DescribeGeom( "curvature driven flow", datgeo);
    ensight.DescribeScalar( "Levelset", datscl, true);
    ensight.DescribeScalar( "Pressure", datpr,  true);
    ensight.DescribeVector( "Velocity", datvec, true);
    ensight.putGeom( datgeo);
    ensight.putVector( datvec, Stokes.GetVelSolution(), 0);
    ensight.putScalar( datpr,  Stokes.GetPrSolution(), 0);
    ensight.putScalar( datscl, lset.GetSolution(), 0);

    if (meth)
    {
        typedef PSchur_GSPCG_CL StokesSolverT;
        PSchur_GSPCG_CL StokesSolver( prM.Data, 200, outer_tol, 200, inner_iter_tol);
        typedef NSSolverBaseCL<StokesProblemT> SolverT;
        SolverT dummyFP( Stokes, StokesSolver);
        LinThetaScheme2PhaseCL<StokesProblemT, SolverT>
            cpl( Stokes, lset, dummyFP, /*theta*/ 0.5, /*nonlinear*/ 0.);
        cpl.SetTimeStep( delta_t);

        for (Uint step= 1; step<=num_steps; ++step)
        {
            std::cerr << "======================================================== Schritt " << step << ":\n";
            cpl.DoStep( FPsteps);
            ensight.putScalar( datpr, Stokes.GetPrSolution(), step*delta_t);
            ensight.putVector( datvec, Stokes.GetVelSolution(), step*delta_t);
            ensight.putScalar( datscl, lset.GetSolution(), step*delta_t);
        }
    }
    else // Uzawa
    {
        double tau;
        Uint inner_iter;
        tau=  0.5*delta_t;
        std::cerr << "#PCG steps = "; std::cin >> inner_iter;
        typedef Uzawa_PCG_CL StokesSolverT;
        StokesSolverT uzawaSolver( prM.Data, 5000, outer_tol, inner_iter, inner_iter_tol, tau);
        typedef NSSolverBaseCL<StokesProblemT> SolverT;
        SolverT dummyFP( Stokes, uzawaSolver);
        LinThetaScheme2PhaseCL<StokesProblemT, SolverT>
            cpl( Stokes, lset, dummyFP, /*theta*/ 0.5, /*nonlinear*/ 0.);
        cpl.SetTimeStep( delta_t);

        for (Uint step= 1; step<=num_steps; ++step)
        {
            std::cerr << "============= Schritt " << step << ":\n";
            cpl.DoStep( FPsteps);
            ensight.putScalar( datpr, Stokes.GetPrSolution(), step*delta_t);
            ensight.putVector( datvec, Stokes.GetVelSolution(), step*delta_t);
            ensight.putScalar( datscl, lset.GetSolution(), step*delta_t);
        }
        std::cerr << "Iterationen: " << uzawaSolver.GetIter()
                  << "\tNorm des Res.: " << uzawaSolver.GetResid() << std::endl;
    }

    ensight.CaseEnd();

    std::cerr << std::endl;
}

} // end of namespace DROPS


int main (int argc, char** argv)
{
  try
  {
    if (argc<4)
    {
        std::cerr << "You have to specify at least three parameters:\n\t"
                  << argv[0] << " <inner_iter_tol> <num_subdiv> <surf.tension> [<dt> <num_steps>]" << std::endl;
        return 1;
    }
    double inner_iter_tol= std::atof(argv[1]);
    int sub_div= std::atoi(argv[2]);
    sigma= std::atof(argv[3]);
    if (argc>4) delta_t= std::atof(argv[4]);
    if (argc>5) num_steps= std::atoi(argv[5]);

    std::cerr << "inner iter tol:  " << inner_iter_tol << std::endl;
    std::cerr << "sub divisions:   " << sub_div << std::endl;
    std::cerr << "surface tension: " << sigma << std::endl;
    std::cerr << num_steps << " time steps of size " << delta_t << std::endl;
    DROPS::Point3DCL null(0.0);
    DROPS::Point3DCL e1(0.0), e2(0.0), e3(0.0);
    e1[0]= e2[1]= e3[2]= 1.;

    typedef DROPS::InstatNavierStokes2PhaseP2P1CL<ZeroFlowCL> MyStokesCL;

    DROPS::BrickBuilderCL brick(null, e1, e2, e3, sub_div, sub_div, sub_div);

    const bool IsNeumann[6]=
        {false, false, false, false, false, false};
    const DROPS::StokesVelBndDataCL::bnd_val_fun bnd_fun[6]=
        { &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel };

    MyStokesCL prob(brick, ZeroFlowCL(), DROPS::StokesBndDataCL(6, IsNeumann, bnd_fun));
    DROPS::MultiGridCL& mg = prob.GetMG();
    Strategy(prob, inner_iter_tol);
    std::cerr << DROPS::SanityMGOutCL(mg) << std::endl;
    double min= prob.p.Data.min(),
           max= prob.p.Data.max();
    std::cerr << "pressure min/max: "<<min<<", "<<max<<std::endl;

    return 0;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }
}
