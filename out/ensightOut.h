//**************************************************************************
// File:    ensightOut.h                                                   *
// Content: solution output in Ensight6 Case format                        *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//          Oliver Fortmeier, SC RWTH Aachen                               *
//**************************************************************************

#ifndef DROPS_ENSIGHTOUT_H
#define DROPS_ENSIGHTOUT_H

#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <map>
#include "geom/multigrid.h"
#include "misc/problem.h"

namespace DROPS
{

/// \brief Helper union for writing integers in binary files
union showInt
{
    int i;
    char s[sizeof(int)];
};

/// \brief Helper union for writing floats in binary files
union showFloat
{
    float f;
    char s[sizeof(float)];
};


class Ensight6VariableCL; //forward declaration

/// \brief Class for writing out results of a simulation in Ensight6 Case format.
///
/// Register subclasses of Ensight6VariableCL to output the geometry and scalar/vector-valued functions
/// in Ensight6-Case format.
class Ensight6OutCL
{
  private:
    char               decDigits_; ///< Number of digits in the decimal representation of numsteps_
    int                timestep_;  ///< Current timestep
    int                numsteps_;  ///< Total number of timesteps (required for the names of transient output files)
    double             time_;      ///< Time of current timestep
    std::ostringstream geomdesc_,  ///< Geometry-section of the Case-file
                       vardesc_,   ///< Variable-section of the Case-file
                       timestr_;   ///< Time-section of the Case-file
    std::string        casefile_;  ///< Name of the Case-file
    const bool         binary_;    ///< type of output
    bool               timedep_;   ///< true, if there are time-dependent variables
    std::map<std::string, Ensight6VariableCL*> vars_;        ///< The variables and geometry stored by varName.

    /// \brief Internal helper
    ///@{
    void OpenFile       (std::ofstream& of, std::string varName); ///< Append timecode for transient output and check the stream
    bool putTime        (double t);                               ///< Advance timestep_, time_, timestr_, if t_> time_ and set time_= t_; returns true, if t_>time_.
    void CheckFile      (const std::ofstream&) const;
    void CommitCaseFile ();                                       ///< (Re)write case file
    ///@}

  public:
    Ensight6OutCL  (std::string casefileName, Uint numsteps= 0, bool binary= true);
    ~Ensight6OutCL ();

    /// \brief Register a variable or the geometry for output with Write().
    ///
    /// The class takes ownership of the objects, i. e. it destroys them with delete in its destructor.
    void Register (Ensight6VariableCL& var);
    /// \brief Write the registered Ensight6-variables
    ///
    /// For t==0, write all registered objects to their files;if t>0 and t has increased with regard
    /// to the last call, write all time-dependent objects. Only the first of multiple calls with identical t has an effect.
    void Write (double t= 0.);

    /// \brief Append the current timestep-value with the required number of leading '0' to str.
    void AppendTimecode(std::string& str) const;
 
    /// \brief Interface for classes that implement the Ensight6VariableCL-interface, i.e. output of specific varibles; should probably be private.
    ///@{
    /// \brief Describe a geometry model
    void DescribeGeom (std::string geoName);
    /// \brief Describe a finite element function
    void DescribeVariable (std::string varName, bool isscalar);

    /// \brief Write the geometry into a file
    void putGeom   (MultiGridCL& mg, int lvl, std::string geoName);
    /// \brief Write a scalar value finite element function into a file
    template<class DiscScalT>
    void putScalar (const DiscScalT& v, std::string varName);
    /// \brief Write a vector value finite element function into a file
    template<class DiscVecT>
    void putVector (const DiscVecT& v, std::string varName);
    ///@}
};

/// \brief Base-class for the output of a single function in Ensight6 Case format.
///
/// We employ the command pattern: 'Describe' is the interface for registration in Ensight6OutCL.
/// 'put' is called for the output of the function at time t. The command objects are stored in Ensight6OutCL.
class Ensight6VariableCL
{
  private:
    std::string varName_,
                fileName_;
    bool        timedep_;

  public:
    Ensight6VariableCL (std::string varName, std::string fileName, bool timedep)
        : varName_( varName), fileName_( fileName), timedep_( timedep) {}
    virtual ~Ensight6VariableCL () {}

    std::string varName  () const { return varName_; }  ///< Name of the variable in einsight; also used as identifier in Ensight6OutCL.
    std::string fileName () const { return fileName_; } ///< Name of the file; for time-dependent objects, the timecode is attached by Ensight6OutCL.
    bool        Timedep  () const { return timedep_; }  ///< Is the object time-dependent?

    /// \brief Called by Ensight6OutCL::Register().
    virtual void Describe (Ensight6OutCL&) const= 0;
    /// \brief Called by Ensight6OutCL::Write().
    virtual void put      (Ensight6OutCL&) const= 0;
};

///\brief Output a geometry.
///
/// This outputs a triangulation of a multigrid.
class Ensight6GeomCL : public Ensight6VariableCL
{
  private:
    MultiGridCL* mg_;  ///< The multigrid
    int          lvl_; ///< Level of the triangulation

  public:
    Ensight6GeomCL (MultiGridCL& mg, int lvl, std::string varName, std::string fileName, bool timedep= false)
        : Ensight6VariableCL( varName, fileName, timedep), mg_( &mg), lvl_( lvl) {}

    void Describe (Ensight6OutCL& cf) const { cf.DescribeGeom( this->varName());  }
    void put      (Ensight6OutCL& cf) const { cf.putGeom( *mg_, lvl_, varName()); }
};

///\brief Create an Ensight6GeomCL with operator new.
///
/// This is just for uniform code; the analoguous functions for scalars and vectors are more useful because
/// they help to avoid template parameters in user code.
inline Ensight6GeomCL&
make_Ensight6Geom (MultiGridCL& mg, int lvl, std::string varName, std::string fileName, bool timedep= false)
{
    return *new Ensight6GeomCL( mg, lvl, varName, fileName, timedep);
}

///\brief Represents a scalar Drops-function (P1 or P2, given as PXEvalCL) as Ensight6 variable.
template <class DiscScalarT>
class Ensight6ScalarCL : public Ensight6VariableCL
{
  private:
    const DiscScalarT f_;

  public:
    Ensight6ScalarCL (const DiscScalarT& f, std::string varName, std::string fileName, bool timedep= false)
        : Ensight6VariableCL( varName, fileName, timedep), f_( f) {}

    void Describe (Ensight6OutCL& cf) const { cf.DescribeVariable( this->varName(), true); }
    void put      (Ensight6OutCL& cf) const { cf.putScalar( f_, varName()); }
};

///\brief Create an Ensight6ScalarCL<> with operator new.
///
/// This function does the template parameter deduction for user code.
template <class DiscScalarT>
  Ensight6ScalarCL<DiscScalarT>&
    make_Ensight6Scalar (const DiscScalarT& f, std::string varName, std::string fileName, bool timedep= false)
{
    return *new Ensight6ScalarCL<DiscScalarT>( f, varName, fileName, timedep);
}

///\brief Represents a vector Drops-function (P1 or P2, given as PXEvalCL) as Ensight6 variable.
template <class DiscVectorT>
class Ensight6VectorCL : public Ensight6VariableCL
{
  private:
    const DiscVectorT f_;

  public:
    Ensight6VectorCL (const DiscVectorT& f, std::string varName, std::string fileName, bool timedep= false)
        : Ensight6VariableCL( varName, fileName, timedep), f_( f) {}

    void Describe (Ensight6OutCL& cf) const { cf.DescribeVariable( this->varName(), false); }
    void put      (Ensight6OutCL& cf) const { cf.putVector( f_, varName()); }
};

///\brief Create an Ensight6VectorCL<> with operator new.
///
/// This function does the template parameter deduction for user code.
template <class DiscVectorT>
  Ensight6VectorCL<DiscVectorT>&
    make_Ensight6Vector (const DiscVectorT& f, std::string varName, std::string fileName, bool timedep= false)
{
    return *new Ensight6VectorCL<DiscVectorT>( f, varName, fileName, timedep);
}

class ReadEnsightP2SolCL
// read solution from Ensight6 Case format
{
  private:
    const MultiGridCL* _MG;
    const bool         binary_;

    void CheckFile( const std::ifstream&) const;

  public:
    ReadEnsightP2SolCL( const MultiGridCL& mg, bool binary=true)
      : _MG(&mg), binary_(binary) {}

    template<class BndT>
    void ReadScalar( const std::string&, VecDescCL&, const BndT&) const;
    template<class BndT>
    void ReadVector( const std::string&, VecDescCL&, const BndT&) const;
};


//=====================================================
//              template definitions
//=====================================================

template<class DiscScalT>
void Ensight6OutCL::putScalar (const DiscScalT& v, std::string varName)
{
    const MultiGridCL& mg= v.GetMG();
    const Uint lvl= v.GetLevel();
    char buffer[80];
    std::memset(buffer,0,80);
    showFloat sFlo;

    std::ofstream os;
    OpenFile( os, varName);

    v.SetTime( time_);

    if(binary_)
    {
        std::strcpy(buffer,"DROPS data file, scalar variable:");
        os.write(buffer,80);

        DROPS_FOR_TRIANG_CONST_VERTEX( mg, lvl, it) {
            sFlo.f=v.val(*it);
            os.write(sFlo.s,sizeof(float));
        }

        DROPS_FOR_TRIANG_CONST_EDGE( mg, lvl, it) {
            sFlo.f=v.val(*it,0.5);
            os.write(sFlo.s,sizeof(float));
        }
    }
    else //ASCII-Ausgabe
    {
        int cnt=0;
        os.flags(std::ios_base::scientific);
        os.precision(5);
        os.width(12);

        os << "DROPS data file, scalar variable:\n";
        DROPS_FOR_TRIANG_CONST_VERTEX( mg, lvl, it) {
            os << std::setw(12) << v.val( *it);
            if ( (++cnt)==6)
            { // Ensight expects six real numbers per line
                cnt= 0;
                os << '\n';
            }
        }

        DROPS_FOR_TRIANG_CONST_EDGE( mg, lvl, it) {
            os << std::setw(12) << v.val( *it, 0.5);
            if ( (++cnt)==6)
            { // Ensight expects six real numbers per line
                cnt= 0;
                os << '\n';
            }
        }
        os << '\n';
    }
}

template<class DiscVecT>
void Ensight6OutCL::putVector (const DiscVecT& v, std::string varName)
{
    const MultiGridCL& mg= v.GetMG();
    const Uint lvl= v.GetLevel();
    char buffer[80];
    std::memset(buffer,0,80);
    showFloat sFlo;

    std::ofstream os;
    OpenFile( os, varName);

    v.SetTime( time_);

    if(binary_)
    {
        std::strcpy(buffer,"DROPS data file, vector variable:");
        os.write( buffer, 80);

        DROPS_FOR_TRIANG_CONST_VERTEX( mg, lvl, it) {
            for (int i=0; i<3; ++i)
            {
                sFlo.f=v.val( *it)[i];
                os.write(sFlo.s,sizeof(float));
            }
        }
        DROPS_FOR_TRIANG_CONST_EDGE( mg, lvl, it) {
            for (int i=0; i<3; ++i)
            {
                 sFlo.f=v.val( *it)[i];
                 os.write(sFlo.s,sizeof(float));
            }
        }
    }
    else
    { // ASCII
        int cnt=0;
        os.flags(std::ios_base::scientific);
        os.precision(5);
        os.width(12);

        os << "DROPS data file, vector variable:\n";

        DROPS_FOR_TRIANG_CONST_VERTEX( mg, lvl, it) {
            for (int i=0; i<3; ++i)
                os << std::setw(12) << v.val( *it)[i];
            if ( (cnt+=3)==6)
            { // Ensight expects six real numbers per line
                 cnt= 0;
                 os << '\n';
            }
        }
        DROPS_FOR_TRIANG_CONST_EDGE( mg, lvl, it) {
            for (int i=0; i<3; ++i)
                os << std::setw(12) << v.val( *it)[i];
            if ( (cnt+=3)==6)
            { // Ensight expects six real numbers per line
                 cnt= 0;
                 os << '\n';
            }
        }
        os << '\n';
    }
}


// ========== ReadEnsightP2SolCL ==========

template <class BndT>
void ReadEnsightP2SolCL::ReadScalar( const std::string& file, VecDescCL& v, const BndT& bnd) const
{
    const Uint lvl= v.GetLevel(),
               idx= v.RowIdx->GetIdx();
    std::ifstream is( file.c_str());
    CheckFile( is);

    if (binary_)
    {
        showFloat fl;
        char buffer[80];
        is.read( buffer, 80);           //ignore first 80 characters

        for (MultiGridCL::const_TriangVertexIteratorCL it= _MG->GetTriangVertexBegin(lvl),
             end= _MG->GetTriangVertexEnd(lvl); it!=end; ++it)
        {
            is.read( fl.s, sizeof(float));
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            v.Data[it->Unknowns(idx)]= (double)fl.f;
        }

        for (MultiGridCL::const_TriangEdgeIteratorCL it= _MG->GetTriangEdgeBegin(lvl),
            end= _MG->GetTriangEdgeEnd(lvl); it!=end; ++it)
        {
            is.read( fl.s, sizeof(float));
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            v.Data[it->Unknowns(idx)]= fl.f;
        }
    }
    else
    { // ASCII
        char buf[256];
        double d= 0;

        is.getline( buf, 256); // ignore first line

        for (MultiGridCL::const_TriangVertexIteratorCL it= _MG->GetTriangVertexBegin(lvl),
             end= _MG->GetTriangVertexEnd(lvl); it!=end; ++it)
        {
            is >> d;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            v.Data[it->Unknowns(idx)]= d;
        }
        for (MultiGridCL::const_TriangEdgeIteratorCL it= _MG->GetTriangEdgeBegin(lvl),
            end= _MG->GetTriangEdgeEnd(lvl); it!=end; ++it)
        {
            is >> d;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            v.Data[it->Unknowns(idx)]= d;
        }
    }

    CheckFile( is);
}

template <class BndT>
void ReadEnsightP2SolCL::ReadVector( const std::string& file, VecDescCL& v, const BndT& bnd) const
{
    const Uint lvl= v.GetLevel(),
               idx= v.RowIdx->GetIdx();
    std::ifstream is( file.c_str());
    CheckFile( is);

    double d0= 0, d1= 0, d2= 0;

    if(binary_)
    {
        showFloat fl;
        //std::cout<<"READVECTOR: "<<file.c_str()<<"\n";
        char buffer[80];
        is.read( buffer, 80);       //ignore first 80 characters

        for (MultiGridCL::const_TriangVertexIteratorCL it= _MG->GetTriangVertexBegin(lvl),
            end= _MG->GetTriangVertexEnd(lvl); it!=end; ++it)
        {
            is.read( fl.s, sizeof(float));
            d0=fl.f;
            is.read( fl.s, sizeof(float));
            d1=fl.f;
            is.read( fl.s, sizeof(float));
            d2=fl.f;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            const IdxT Nr= it->Unknowns(idx);
            v.Data[Nr]= d0;    v.Data[Nr+1]= d1;    v.Data[Nr+2]= d2;
        }

        for (MultiGridCL::const_TriangEdgeIteratorCL it= _MG->GetTriangEdgeBegin(lvl),
             end= _MG->GetTriangEdgeEnd(lvl); it!=end; ++it)
        {
            is.read( fl.s, sizeof(float));
            d0=fl.f;
            is.read( fl.s, sizeof(float));
            d1=fl.f;
            is.read( fl.s, sizeof(float));
            d2=fl.f;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;
            const IdxT Nr= it->Unknowns(idx);
            v.Data[Nr]= d0;    v.Data[Nr+1]= d1;    v.Data[Nr+2]= d2;
        }
    }
    else
    { // ASCII
        char buf[256];

        is.getline( buf, 256); // ignore first line

        for (MultiGridCL::const_TriangVertexIteratorCL it= _MG->GetTriangVertexBegin(lvl),
            end= _MG->GetTriangVertexEnd(lvl); it!=end; ++it)
        {
            is >> d0 >> d1 >> d2;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;

            const IdxT Nr= it->Unknowns(idx);
            v.Data[Nr]= d0;    v.Data[Nr+1]= d1;    v.Data[Nr+2]= d2;
        }

        for (MultiGridCL::const_TriangEdgeIteratorCL it= _MG->GetTriangEdgeBegin(lvl),
            end= _MG->GetTriangEdgeEnd(lvl); it!=end; ++it)
        {
            is >> d0 >> d1 >> d2;
            if (bnd.IsOnDirBnd( *it) || !(it->Unknowns.Exist(idx)) ) continue;

            const IdxT Nr= it->Unknowns(idx);
            v.Data[Nr]= d0;    v.Data[Nr+1]= d1;    v.Data[Nr+2]= d2;
        }
    }

    CheckFile( is);
}

} // end of namespace DROPS

#endif
