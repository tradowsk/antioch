//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// Antioch - A Gas Dynamics Thermochemistry Library
//
// Copyright (C) 2014 Paul T. Bauman, Benjamin S. Kirk, Sylvain Plessis,
//                    Roy H. Stonger
// Copyright (C) 2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

#ifndef ANTIOCH_MIXTURE_DIFFUSION_H
#define ANTIOCH_MIXTURE_DIFFUSION_H

#include "antioch/mixture_transport_base.h"
#include "antioch/diffusion_traits.h"
#include "antioch/species_diffusion_base.h"
#include "antioch/binary_diffusion_base.h"

namespace Antioch
{

  //! Container class for species binary diffusion models
  /*! For the given set of chemical species in the input TransportMixture,
   *  this contains the diffusion models for each of those species and provides
   *  an interface for computing the species diffusion coefficients/binary diffusion
   *  matrix, depending on the Diffusion model template parameter.
   *  Total/mixture diffusion coefficients are computed by a mixing model,
   *  e.g. WilkeTransportMixture. This class is templated on the diffusion model,
   *  so an inherent assumption is that all species diffusions have the same model.
   */
  template<typename Diffusion, class CoeffType>
  class MixtureDiffusion : public MixtureTransportBase<CoeffType>
  {
  public:

    MixtureDiffusion( const TransportMixture<CoeffType>& transport_mixture );

    virtual ~MixtureDiffusion();

    //! Computes the binary diffusion matrix
    /*!
     * Should only be called when using a binary diffusion model.
     */
    template<typename StateType, typename MatrixStateType>
    void compute_binary_diffusion_matrix( const StateType& T,
                                          const StateType& molar_density,
                                          MatrixStateType& D ) const;

    //! Computes species diffusivity for species s
    template<typename StateType>
    void compute_species_diffusivity( unsigned int s, const StateType& rho,
                                      const StateType& cp, const StateType& k,
                                      StateType& D ) const;

    //! Add species diffusion model for species s
    /*
     * Methods for building species diffusion models call this.
     */
    void add_species_diffusion( unsigned int s, const std::vector<CoeffType>& coeffs );

  protected:

    //! Stores species diffusivity models
    /*! This only gets built if Diffusion is a species diffusion model.
     *  Actually populating this is done in a separate function that calls
     *  the add_species_diffusivity method in this class since it depends
     *  on user-prescribed coefficients. However, if Diffusion is of
     *  species type, then the constructor will allocate space to add
     *  species diffusivities later. */
    std::vector<SpeciesDiffusionBase<Diffusion,CoeffType>*> _species_diffusivities;

    //! Stores binary diffusion model for all species pairs
    /*!
     *  This only gets built if Diffusion is a binary diffusion model.
     *  \todo This will always be symmetric so we should make a smarter
     *  container that only stores the upper (lower) triangle of the
     *  species pairs and use accordingly. */
    std::vector<std::vector<BinaryDiffusionBase<Diffusion,CoeffType>*> > _binary_diffusivities;

  private:

    void private_init_impl( AntiochPrivate::diffusion_tag<SpeciesDiffusionBase<Diffusion,CoeffType> >& tag )
    {
#ifdef ANTIOCH_HAVE_CXX_STATIC_ASSERT
      static_assert( DiffusionTraits<Diffusion>::is_species_diffusion,
                     "This shouldn't happen!" );
#endif

      _species_diffusivities.resize( this->_transport_mixture.n_species(), NULL );
    }

    void private_init_impl( AntiochPrivate::diffusion_tag<BinaryDiffusionBase<Diffusion,CoeffType> >& tag )
    {
#ifdef ANTIOCH_HAVE_CXX_STATIC_ASSERT
      static_assert( DiffusionTraits<Diffusion>::is_binary_diffusion,
                     "This shouldn't happen!" );
#endif

      // Build up binary diffusion species models
      _binary_diffusivities.resize( this->_transport_mixture.n_species() );
      for( unsigned int i = 0; i < this->_transport_mixture.n_species(); i++ )
        {
          _binary_diffusivities[i].resize( this->_transport_mixture.n_species(), NULL );

          for (unsigned int j = 0; j < this->_transport_mixture.n_species(); j++)
            {
              const TransportSpecies<CoeffType>& s_i = this->_transport_mixture.transport_species(i);
              const TransportSpecies<CoeffType>& s_j = this->_transport_mixture.transport_species(j);

              _binary_diffusivities[i][j] = new Diffusion( s_i, s_j );
          }
        }
    }

    template<typename StateType, typename MatrixStateType>
    void private_diff_matrix_impl( const StateType& T,
                                   const StateType& molar_density,
                                   MatrixStateType& D,
                                   AntiochPrivate::diffusion_tag<BinaryDiffusionBase<Diffusion,CoeffType> >& tag ) const
    {
      for(unsigned int i = 0; i < D.size(); i++)
      {
        for(unsigned int j = 0; j < D.size(); j++)
          {
            antioch_assert(_binary_diffusivities[i][j]);

            D[i][j] = (*_binary_diffusivities[i][j])(T,molar_density);
          }
      }
    }

    template<typename StateType, typename MatrixStateType>
    void private_diff_matrix_impl( const StateType& /*T*/,
                                   const StateType& /*molar_density*/,
                                   MatrixStateType& /*D*/,
                                   AntiochPrivate::diffusion_tag<SpeciesDiffusionBase<Diffusion,CoeffType> >& tag ) const
    {
      antioch_error();
    }

    template<typename StateType>
    void private_species_diff_impl( unsigned int s,
                                    const StateType& rho,
                                    const StateType& cp,
                                    const StateType& k,
                                    StateType& D,
                                    AntiochPrivate::diffusion_tag<SpeciesDiffusionBase<Diffusion,CoeffType> >& tag ) const
    {
      (*_species_diffusivities[s])(rho,cp,k,D);
    }

    template<typename StateType>
    void private_species_diff_impl( unsigned int s,
                                    const StateType& rho,
                                    const StateType& cp,
                                    const StateType& k,
                                    StateType& D,
                                    AntiochPrivate::diffusion_tag<BinaryDiffusionBase<Diffusion,CoeffType> >& tag ) const
    {
      antioch_error();
    }

    MixtureDiffusion();

  };

  template<typename Diffusion, class CoeffType>
  MixtureDiffusion<Diffusion,CoeffType>::MixtureDiffusion( const TransportMixture<CoeffType>& transport_mixture )
    :  MixtureTransportBase<CoeffType>(transport_mixture)

  {
    // This class currenltly only supports species or binary diffusion models
    if( !DiffusionTraits<Diffusion>::is_species_diffusion ||
        !DiffusionTraits<Diffusion>::is_binary_diffusion )
      {
#ifdef ANTIOCH_HAVE_CXX_STATIC_ASSERT
        static_assert( DiffusionTraits<Diffusion>::is_species_diffusion ||
                       DiffusionTraits<Diffusion>::is_binary_diffusion,
                       "Can only instantiate MixtureDiffusion with a species or binary diffusion model!" );
#endif

        std::string error = "ERROR: You're trying to construct an object\n";
        error += "       with an unknown diffusion model!\n";
        error += "       Does your compiler not support static_assert?";
        antioch_msg_error(error);
      }

    AntiochPrivate::diffusion_tag<Diffusion> tag;
    this->private_init_impl( tag );

  }

  template<typename Diffusion, class CoeffType>
  MixtureDiffusion<Diffusion,CoeffType>::~MixtureDiffusion()
  {
    // Clean up species diffusion models
    if( DiffusionTraits<Diffusion>::is_species_diffusion )
      {
        for( typename std::vector<SpeciesDiffusionBase<Diffusion,CoeffType>*>::iterator it = _species_diffusivities.begin();
         it != _species_diffusivities.end(); ++it )
          delete *it;
      }

    // Clean up binary diffusion models
    if( DiffusionTraits<Diffusion>::is_binary_diffusion )
      {
        for( typename std::vector<std::vector<BinaryDiffusionBase<Diffusion,CoeffType>*> >::iterator it_outer = _binary_diffusivities.begin();
             it_outer != _binary_diffusivities.end(); ++it_outer )
          for( typename std::vector<BinaryDiffusionBase<Diffusion,CoeffType>*>::iterator it_inner = it_outer->begin();
               it_inner != it_outer->end(); ++it_inner )
            delete *it_inner;
      }
  }

  template<typename Diffusion, class CoeffType>
  template<typename StateType, typename MatrixStateType>
  inline
  void MixtureDiffusion<Diffusion,CoeffType>::compute_binary_diffusion_matrix( const StateType& T,
                                                                               const StateType& molar_density,
                                                                               MatrixStateType& D ) const
  {
    const unsigned int n_cols = D.size();
    antioch_assert_greater(n_cols,0);

    // Make sure it's a square matrix
#ifndef NDEBUG
    for( unsigned int r = 0; r < n_cols; r++ )
      antioch_assert_equal_to(D[r].size(),n_cols);
#endif

    AntiochPrivate::diffusion_tag<Diffusion> tag;
    this->private_diff_matrix_impl(T,molar_density,D,tag);
  }

  template<typename Diffusion, class CoeffType>
  template<typename StateType>
  inline
  void MixtureDiffusion<Diffusion,CoeffType>::compute_species_diffusivity( unsigned int s,
                                                                           const StateType& rho,
                                                                           const StateType& cp,
                                                                           const StateType& k,
                                                                           StateType& D ) const
  {
    antioch_assert_less( s, _species_diffusivities.size());
    antioch_assert(_species_diffusivities[s]);

    AntiochPrivate::diffusion_tag<Diffusion> tag;
    this->private_species_diff_impl(s,rho,cp,k,D,tag);
  }

  template<typename Diffusion, class CoeffType>
  void MixtureDiffusion<Diffusion,CoeffType>::add_species_diffusion( unsigned int s,
                                                                     const std::vector<CoeffType>& coeffs )
  {
    antioch_assert_less( s, _species_diffusivities.size() );

    _species_diffusivities[s] = new Diffusion( coeffs );
  }

} // end namespace Antioch

#endif // ANTIOCH_MIXTURE_DIFFUSION_H
