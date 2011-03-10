/// \file quadrature.tpp
/// \brief numerical integration at the interface
/// \author LNM RWTH Aachen: Joerg Grande; SC RWTH Aachen:

/*
 * This file is part of DROPS.
 *
 * DROPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DROPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DROPS. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright 2011 LNM/SC RWTH Aachen, Germany
 */

#include "num/discretize.h"

namespace DROPS {

template <class GridFunT, class DomainT>
  typename ValueHelperCL<GridFunT>::value_type
  quad (const GridFunT& f, double absdet, const DomainT& dom, TetraSignEnum s=AllTetraC)
{
          Uint begin= dom.dof_begin( s);
    const Uint end=   dom.dof_end(   s);
    typename DomainT::const_weight_iterator w_iter= dom.weight_begin( s);

    typedef typename ValueHelperCL<GridFunT>::value_type value_type;
    value_type sum= value_type();
    while (begin != end)
       sum+= (*w_iter++)*f[begin++];
    return sum*absdet;
}

template <class GridFunT, class DomainT>
  inline void
  quad (const GridFunT& f, double absdet, const DomainT& dom,
    typename ValueHelperCL<GridFunT>::value_type& neg_int,
    typename ValueHelperCL<GridFunT>::value_type& pos_int)
{
    neg_int= quad( f, absdet, dom, NegTetraC);
    pos_int= quad( f, absdet, dom, PosTetraC);
}

///\brief Helper to quad_{neg,pos}_integrand
/// Integrate a integrand, that is defined only on either the negative or the positive tetras. It does not work for standard integrands.
template <class GridFunT, class DomainT>
  typename ValueHelperCL<GridFunT>::value_type
  quad_single_domain_integrand (const GridFunT& f, double absdet, const DomainT& dom, TetraSignEnum s)
{
    typename DomainT::const_weight_iterator w_iter= dom.weight_begin( s);
          Uint begin= 0;
    const Uint end= dom.dof_end( s) - dom.dof_begin( s);

    typedef typename ValueHelperCL<GridFunT>::value_type value_type;
    value_type sum= value_type();
    while (begin != end)
       sum+= (*w_iter++)*f[begin++];
    return sum*absdet;
}

template <class GridFunT, class DomainT>
  inline typename ValueHelperCL<GridFunT>::value_type
  quad_neg_integrand (const GridFunT& f, double absdet, const DomainT& dom)
{
    return quad_single_domain_integrand( f, absdet, dom, NegTetraC);
}

template <class GridFunT, class DomainT>
  inline typename ValueHelperCL<GridFunT>::value_type
  quad_pos_integrand (const GridFunT& f, double absdet, const DomainT& dom)
{
    return quad_single_domain_integrand( f, absdet, dom, PosTetraC);
}


template <class VertexPartitionPolicyT, class VertexCutMergingPolicyT>
  void
  CompositeQuad2DomainCL::assign (const TetraPartitionCL<VertexPartitionPolicyT,VertexCutMergingPolicyT>& p)
{
    typedef TetraPartitionCL<VertexPartitionPolicyT,VertexCutMergingPolicyT> TetraPartitionT;

    neg_end_= p.vertex_size( NegTetraC) + p.tetra_size( NegTetraC);
    pos_begin_= p.vertex_begin( PosTetraC) - p.vertex_begin() + p.tetra_size( NegTetraC);

    neg_weights_.resize( p.vertex_size( NegTetraC) + p.tetra_size( NegTetraC));
    pos_weights_.resize( p.vertex_size( PosTetraC) + p.tetra_size( PosTetraC));

    VertexContT neg_tetra_bary;
    neg_tetra_bary.reserve( p.tetra_size( NegTetraC));
    VertexContT pos_tetra_bary;
    pos_tetra_bary.reserve( p.tetra_size( PosTetraC));

    const typename TetraPartitionT::const_vertex_iterator partition_vertexes= p.vertex_begin();
    QRDecompCL<4,4> qr;
    SMatrixCL<4,4>& T= qr.GetMatrix();
    for (typename TetraPartitionT::const_tetra_iterator it= p.tetra_begin(); it != p.tetra_end(); ++it) {
        for (Uint i= 0; i < NumVertsC; ++i)
            T.col( i, partition_vertexes[(*it)[i]]);
        const bool is_neg= p.sign( it) == -1;
        (is_neg ? neg_tetra_bary : pos_tetra_bary).push_back( T*Quad2DataCL::Node[4]);
        qr.prepare_solve();
        const double absdet= std::fabs( qr.Determinant_R());
        WeightContT& w= is_neg ? neg_weights_ : pos_weights_;
        const Uint vertex_weight_begin= is_neg ? p.tetra_size( NegTetraC) : 0;
        const Uint vertex_beg= is_neg ? 0 : p.vertex_begin( PosTetraC) - p.vertex_begin();
        for (int i= 0; i < 4; ++i)
            w[(*it)[i] - vertex_beg + vertex_weight_begin]+= absdet*Quad2DataCL::Wght[0];
        const Uint tetra_weight_begin= is_neg ? 0 : p.vertex_size( PosTetraC);
        const typename TetraPartitionT::const_tetra_iterator tetra_beg=
            p.tetra_begin( is_neg ? NegTetraC : PosTetraC);
        w[it - tetra_beg + tetra_weight_begin]+= absdet*Quad2DataCL::Wght[1];
    }

    vertexes_.resize( 0);
    vertexes_.reserve( p.vertex_size() + p.tetra_size());
    std::copy( neg_tetra_bary.begin(), neg_tetra_bary.end(), std::back_inserter( vertexes_));
    std::copy( p.vertex_begin(), p.vertex_end(), std::back_inserter( vertexes_));
    std::copy( pos_tetra_bary.begin(), pos_tetra_bary.end(), std::back_inserter( vertexes_));

    all_weights_.resize( p.vertex_size() + p.tetra_size());
    all_weights_[std::slice( dof_begin( NegTetraC), neg_weights_.size(), 1)] = neg_weights_;
    all_weights_[std::slice( dof_begin( PosTetraC), pos_weights_.size(), 1)]+= pos_weights_;
}


template <class VertexPartitionPolicyT, class VertexCutMergingPolicyT>
  void
  CompositeQuad5DomainCL::assign (const TetraPartitionCL<VertexPartitionPolicyT,VertexCutMergingPolicyT>& p)
{
    typedef TetraPartitionCL<VertexPartitionPolicyT,VertexCutMergingPolicyT> TetraPartitionT;
    const Uint num_nodes= Quad5DataCL::NumNodesC;

    vertexes_.resize( 0);
    vertexes_.reserve( num_nodes*p.tetra_size());
    pos_begin_= num_nodes*p.tetra_size( NegTetraC);
    weights_.resize( num_nodes*p.tetra_size());

    const typename TetraPartitionT::const_vertex_iterator partition_vertexes= p.vertex_begin();
    const WeightContT tetra_weights( Quad5DataCL::Weight, num_nodes);
    Uint w_begin= 0;
    QRDecompCL<4,4> qr;
    SMatrixCL<4,4>& T= qr.GetMatrix();
    for (typename TetraPartitionT::const_tetra_iterator it= p.tetra_begin(); it != p.tetra_end(); ++it, w_begin+= num_nodes) {
        for (int i= 0; i < 4; ++i)
            T.col( i, partition_vertexes[(*it)[i]]);
        for (Uint i= 0; i < num_nodes; ++i)
            vertexes_.push_back( T*Quad5DataCL::Node[i]);
        qr.prepare_solve();
        weights_[std::slice( w_begin, num_nodes, 1)]= std::fabs( qr.Determinant_R())*tetra_weights;
    }
}

} // end of namespace DROPS