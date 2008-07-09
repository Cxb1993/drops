//**************************************************************************
// File:    risingBubbleAdap.cpp                                           *
// Content: gravity driven flow of a rising bubble, grid adaptivity        *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//**************************************************************************

#include "geom/multigrid.h"
#include "geom/builder.h"
#include "navstokes/instatnavstokes2phase.h"
#include "stokes/integrTime.h"
#include "num/stokessolver.h"
#include "num/nssolver.h"
#include "out/output.h"
#include "out/ensightOut.h"
#include "levelset/coupling.h"
#include "levelset/adaptriang.h"
#include "levelset/params.h"
#include "num/MGsolver.h"
#include "levelset/mzelle_hdr.h"
#include <fstream>
DROPS::ParamMesszelleCL C;

// rho*du/dt - mu*laplace u + Dp = f + rho*g - okn
//                        -div u = 0
//                             u = u0, t=t0

enum StokesMethod {
	minres                   =  1, // Minres without PC

        pminresmgpcg             =  2, // MG-PC for A, PCG for S
                                       // <SolverAsPreCL<MGSolverCL>, ISBBTPreCL>

        pminrespcgpcg            =  3, // PCG for A, PCG for S
                                       // <SolverAsPreCL<PCGSolverCL<SSORPcCL>>, ISBBTPreCL>

        inexactuzawamgpcg        = 10, // MG-PC for A, PCG for S
                                       // <SolverAsPreCL<MGSolverCL>, ISBBTPreCL>

        inexactuzawapcgpcg       = 11  // PCG for A, PCG for S
                                       // <SolverAsPreCL<PCGSolverCL<SSORPcCL>>, ISBBTPreCL>

};


namespace DROPS // for Strategy
{

template<class Coeff>
void Strategy( InstatNavierStokes2PhaseP2P1CL<Coeff>& Stokes, AdapTriangCL& adap)
// flow control
{
    const double kA=1.0/C.dt;
    const double kM=C.theta;
    typedef InstatNavierStokes2PhaseP2P1CL<Coeff> StokesProblemT;
    sigma=C.sigma;
    MultiGridCL& MG= Stokes.GetMG();
    LevelsetP2CL lset( MG, &sigmaf, /*grad sigma*/ 0, C.lset_theta, C.lset_SD, 0, C.lset_iter, C.lset_tol, C.CurvDiff); // impl. Euler
    lset.SetSurfaceForce( SF_ImprovedLB);
    IdxDescCL* lidx= &lset.idx;
    IdxDescCL* vidx= &Stokes.vel_idx;
    IdxDescCL* pidx= &Stokes.pr_idx;
    TimerCL time;

    lset.CreateNumbering(      MG.GetLastLevel(), lidx);
    lset.Phi.SetIdx( lidx);
    lset.Init( EllipsoidCL::DistanceFct);

    Stokes.CreateNumberingVel( MG.GetLastLevel(), vidx);
    Stokes.CreateNumberingPr ( MG.GetLastLevel(), pidx, NULL, &lset);
    Stokes.b.SetIdx( vidx);
    Stokes.c.SetIdx( pidx);
    Stokes.p.SetIdx( pidx);
    Stokes.v.SetIdx( vidx);
    std::cerr << Stokes.p.Data.size() << " pressure unknowns,\n";
    std::cerr << Stokes.v.Data.size() << " velocity unknowns,\n";
    std::cerr << lset.Phi.Data.size() << " levelset unknowns.\n";
    Stokes.A.SetIdx(vidx, vidx);
    Stokes.B.SetIdx(pidx, vidx);
    Stokes.M.SetIdx(vidx, vidx);
    Stokes.N.SetIdx(vidx, vidx);
    Stokes.prM.SetIdx( pidx, pidx);
    Stokes.prA.SetIdx( pidx, pidx);
    Stokes.SetupPrMass( &Stokes.prM, lset);
    Stokes.SetupPrStiff( &Stokes.prA, lset); // makes no sense for P0
    Stokes.InitVel( &Stokes.v, ZeroVel);
    time.Reset();
    IdxDescCL ens_idx( 1, 1);
    lset.CreateNumbering( MG.GetLastLevel(), &ens_idx);
    EnsightP2SolOutCL ensight( MG, lidx);
    const string filename= C.EnsDir + "/" + C.EnsCase;
    const string datgeo= filename+".geo",
    datpr = filename+".pr" ,
    datvec= filename+".vel",
    datscl= filename+".scl";
    ensight.CaseBegin( string(C.EnsCase+".case").c_str(), C.num_steps+1);
    ensight.DescribeGeom( "Cube", datgeo, true);
    ensight.DescribeScalar( "Levelset", datscl, true);
    ensight.DescribeScalar( "Pressure", datpr,  true);
    ensight.DescribeVector( "Velocity", datvec, true);
    ensight.putGeom( datgeo, 0);
    ensight.putVector( datvec, Stokes.GetVelSolution(), 0);
    ensight.putScalar( datpr,  Stokes.GetPrSolution(), 0);
    ensight.putScalar( datscl, lset.GetSolution(), 0);
    ensight.Commit();

    // Preconditioner for A
        //Multigrid
    MGDataCL velMG;
    SSORsmoothCL smoother(1.0);
    PCG_SsorCL   coarsesolver(SSORPcCL(1.0), 500, C.inner_tol);
    MGSolverCL<SSORsmoothCL, PCG_SsorCL> mgc (velMG, smoother, coarsesolver, 1, -1.0, false);
    typedef SolverAsPreCL<MGSolverCL<SSORsmoothCL, PCG_SsorCL> > MGPCT;
    MGPCT MGPC (mgc);
        //PCG
    typedef SSORPcCL APcPcT;
    APcPcT Apcpc;
    typedef PCGSolverCL<APcPcT> ASolverT;        // CG-based APcT
    ASolverT Asolver( Apcpc, 500, 0.02, true);
    typedef SolverAsPreCL<ASolverT> APcT;
    APcT Apc( Asolver);

    // Preconditioner for instat. Schur complement
    typedef ISBBTPreCL ISBBT;
    ISBBT isbbt (Stokes.B.Data, Stokes.prM.Data, Stokes.M.Data, kA, kM);

    // Preconditioner for PMINRES
    typedef BlockPreCL<MGPCT, ISBBT> Lanczos2PCT;
    typedef PLanczosONBCL<BlockMatrixCL, VectorCL, Lanczos2PCT> Lanczos2T;

    typedef BlockPreCL<APcT, ISBBT>  Lanczos3PCT;
    typedef PLanczosONBCL<BlockMatrixCL, VectorCL, Lanczos3PCT> Lanczos3T;

    Lanczos2PCT lanczos2pc (MGPC, isbbt);
    Lanczos2T lanczos2 (lanczos2pc);

    Lanczos3PCT lanczos3pc (Apc, isbbt);
    Lanczos3T lanczos3 (lanczos3pc);

    // available Stokes/NavStokes Solver
    typedef NSSolverBaseCL<StokesProblemT> SolverT;
    SolverT* solver = 0;

    MResSolverCL minressolver (C.outer_iter, C.outer_tol);
    typedef BlockMatrixSolverCL<MResSolverCL> OseenSolver1T;
    OseenSolver1T oseensolver1( minressolver);

    typedef PMResSolverCL<Lanczos2T> PMinres2T; // PMinRes - MG-ISBBTCL
    PMinres2T pminresmgpcgsolver (lanczos2, C.outer_iter, C.outer_tol);
    typedef BlockMatrixSolverCL<PMinres2T> OseenSolver2T;
    OseenSolver2T oseensolver2( pminresmgpcgsolver);

    typedef PMResSolverCL<Lanczos3T> PMinres3T; // PMinRes - PCG-ISBBT
    PMinres3T pminrespcgpcgsolver (lanczos3, C.outer_iter, C.outer_tol);
    typedef BlockMatrixSolverCL<PMinres3T> OseenSolver3T;
    OseenSolver3T oseensolver3( pminrespcgpcgsolver);

    typedef InexactUzawaCL<MGPCT, ISBBT, APC_SYM> InexactUzawa10T;
    InexactUzawa10T inexactuzawamgpcgsolver( MGPC, isbbt, C.outer_iter, C.outer_tol, 0.6);

    typedef InexactUzawaCL<APcT, ISBBT, APC_SYM> InexactUzawa11T;
    InexactUzawa11T inexactuzawapcgpcgsolver( Apc, isbbt, C.outer_iter, C.outer_tol, 0.6);

    // Coupling for all of the NavStokes-Solver
    TimeDisc2PhaseCL<StokesProblemT>* cpl=0;
    switch (C.StokesMethod) {
        case minres:
            solver = new SolverT(Stokes, oseensolver1); break;
        case pminresmgpcg:
            solver = new SolverT(Stokes, oseensolver2); break;
        case pminrespcgpcg:
            solver = new SolverT(Stokes, oseensolver3); break;
        case inexactuzawamgpcg:
            solver = new SolverT(Stokes, inexactuzawamgpcgsolver); break;
        case inexactuzawapcgpcg:
            solver = new SolverT(Stokes, inexactuzawapcgpcgsolver); break;
        default: throw DROPSErrCL("Unknown StokesMethod");
    }
    if (C.StokesMethod == 2 || C.StokesMethod == 4) // MultiGrid used
        cpl = new ThetaScheme2PhaseCL<StokesProblemT, SolverT>
            ( Stokes, lset, *solver, C.theta, /*nonlinear*/ 0., /*proj.*/ false, C.cpl_stab, true, &velMG);
    else
        cpl = new ThetaScheme2PhaseCL<StokesProblemT, SolverT>
            ( Stokes, lset, *solver, C.theta, /*nonlinear*/ 0., /*proj.*/ false, C.cpl_stab);

    cpl->SetTimeStep (C.dt);

    const double Vol= 4./3.*M_PI*C.Radius[0]*C.Radius[1]*C.Radius[2];

    for (int step= 1; step<=C.num_steps; ++step)
    {
        std::cerr << "======================================================== Schritt " << step << ":\n";
	cpl->DoStep (C.cpl_iter);

        if (C.VolCorr)
        {
            double dphi= lset.AdjustVolume( Vol, 1e-6);//9
            std::cerr << "volume correction is " << dphi << std::endl;
            lset.Phi.Data+= dphi;
            std::cerr << "new rel. Volume: " << lset.GetVolume()/Vol << std::endl;
        }


        if (C.RepFreq && step%C.RepFreq==0) // reparam levelset function
        {
            lset.ReparamFastMarching( C.RepMethod);

            if (C.ref_freq != 0)
                adap.UpdateTriang( Stokes, lset);
            if (adap.WasModified()) {
                cpl->Update();
            }

            std::cerr << "rel. Volume: " << lset.GetVolume()/Vol << std::endl;

            if (C.VolCorr)
            {
                double dphi= lset.AdjustVolume( Vol, 1e-6); //9
                std::cerr << "volume correction is " << dphi << std::endl;
                lset.Phi.Data+= dphi;
                std::cerr << "new rel. Volume: " << lset.GetVolume()/Vol << std::endl;
            }
        }

        ensight.putGeom( datgeo, step*C.dt);
        ensight.putScalar( datpr, Stokes.GetPrSolution(), step*C.dt);
        ensight.putVector( datvec, Stokes.GetVelSolution(), step*C.dt);
        ensight.putScalar( datscl, lset.GetSolution(), step*C.dt);
        ensight.Commit();
    }
    ensight.CaseEnd();

    std::cerr << std::endl;
    delete cpl;
    delete solver;
}

} // end of namespace DROPS


int main (int argc, char** argv)
{
  try
  {
    if (argc>2)
    {
        std::cerr << "You have to specify at most one parameter:\n\t"
                  << argv[0] << " [<param_file>]" << std::endl;
        return 1;
    }
    std::ifstream param;
    if (argc>1)
        param.open( argv[1]);
    else
        param.open( "risingBubbleAdap.param");
    if (!param)
    {
        std::cerr << "error while opening parameter file\n";
        return 1;
    }
    param >> C;
    param.close();
    std::cerr << C << std::endl;
    DROPS::Point3DCL null(0.0);
    DROPS::Point3DCL e1(0.0), e2(0.0), e3(0.0);
    e1[0]= e2[1]= 1.; e3[2]= 2.;

    typedef DROPS::InstatNavierStokes2PhaseP2P1CL<ZeroFlowCL>
            StokesOnBrickCL;

    const int sub_div= std::atoi( C.meshfile.c_str());
    DROPS::BrickBuilderCL brick(null, e1, e2, e3, sub_div, sub_div, DROPS::Uint(sub_div*e3[2]));

    const bool IsNeumann[6]=
        {false, false, false, false, false, false};
    const DROPS::StokesVelBndDataCL::bnd_val_fun bnd_fun[6]=
        { &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel, &DROPS::ZeroVel };

    DROPS::FiniteElementT prFE=DROPS::P1X_FE;
    if (C.XFEMStab<0) prFE=DROPS::P1_FE;

    StokesOnBrickCL prob(brick, ZeroFlowCL(C), DROPS::StokesBndDataCL(6, IsNeumann, bnd_fun), prFE, C.XFEMStab);
    DROPS::MultiGridCL& mg = prob.GetMG();

    EllipsoidCL::Init( C.Mitte, C.Radius );
    DROPS::AdapTriangCL adap( mg, C.ref_width, 0, C.ref_flevel);

    adap.MakeInitialTriang( EllipsoidCL::DistanceFct);

    Strategy( prob, adap);
    std::cerr << DROPS::SanityMGOutCL(mg) << std::endl;
    double min= prob.p.Data.min(),
           max= prob.p.Data.max();
    std::cerr << "pressure min/max: "<<min<<", "<<max<<std::endl;

    return 0;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }
}
