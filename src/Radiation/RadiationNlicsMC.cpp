// ----------------------------------------------------------------------------
//! \file RadiationNlicsMC.cpp
//
//! \brief This class performs the Nonlinear Inverse Compton Scattering
//! on particles using a Monte-Carlo approach.
//
//! The implementation is adapted from the thesis results of M. Lobet
//! See http://www.theses.fr/2015BORD0361
//
// ----------------------------------------------------------------------------

#include "RadiationNlicsMC.h"

#include <cstring>
#include <fstream>

#include <cmath>

// ---------------------------------------------------------------------------------------------------------------------
//! Constructor for RadiationNlicsMC
//! Inherit from Radiation
// ---------------------------------------------------------------------------------------------------------------------
RadiationNlicsMC::RadiationNlicsMC(Params& params, Species * species)
      : Radiation(params, species)
{
}

// ---------------------------------------------------------------------------------------------------------------------
//! Destructor for RadiationNlicsMC
// ---------------------------------------------------------------------------------------------------------------------
RadiationNlicsMC::~RadiationNlicsMC()
{
}

// ---------------------------------------------------------------------------------------------------------------------
//! Overloading of the operator (): perform the Discontinuous radiation reaction
//! induced by the nonlinear inverse Compton scattering
//
//! \param particles   particle object containing the particle properties
//! \param smpi        MPI properties
//! \param nlicsTables Cross-section data tables and useful functions
//                     for nonlinear inverse Compton scattering
//! \param istart      Index of the first particle
//! \param iend        Index of the last particle
//! \param ithread     Thread index
// ---------------------------------------------------------------------------------------------------------------------
void RadiationNlicsMC::operator() (Particles &particles,
        SmileiMPI* smpi,
        NlicsTables &nlicsTables,
        int istart,
        int iend,
        int ithread)
{

    // _______________________________________________________________
    // Parameters
    std::vector<LocalFields> *Epart = &(smpi->dynamics_Epart[ithread]);
    std::vector<LocalFields> *Bpart = &(smpi->dynamics_Bpart[ithread]);
    std::vector<double> *invgf = &(smpi->dynamics_invgf[ithread]);

    // Charge divided by the square of the mass
    double charge_over_mass2;

    // Temporary quantum parameter
    double chipa;

    // Temporary Lorentz factor
    double gamma;

    // Radiated energy
    double rad_energy;

    // Temporary double parameter
    double temp;

    // Time to emission
    double emission_time;

    // time spent in the iteration
    double local_it_time;

    // Number of Monte-Carlo iteration
    int mc_it_nb;

    // Momentum shortcut
    double* momentum[3];
    for ( int i = 0 ; i<3 ; i++ )
        momentum[i] =  &( particles.momentum(i,0) );

    // Charge shortcut
    short* charge = &( particles.charge(0) );

    // Optical depth for the Monte-Carlo process
    double* tau = &( particles.tau(0));

    // Optical depth for the Monte-Carlo process
    double* chi = &( particles.chi(0));

    // _______________________________________________________________
    // Computation

    for (int ipart=istart ; ipart<iend; ipart++ ) {
        charge_over_mass2 = (double)(charge[ipart])*pow(one_over_mass_,2.);

        // Init local variables
        emission_time = 0;
        local_it_time = 0;
        mc_it_nb = 0;

        // Monte-Carlo Manager inside the time step
        while ((local_it_time < dt)
             &&(mc_it_nb < mc_it_nb_max))
        {

            // Gamma
            gamma = sqrt(1.0 + momentum[0][ipart]*momentum[0][ipart]
                             + momentum[1][ipart]*momentum[1][ipart]
                             + momentum[2][ipart]*momentum[2][ipart]);

            // Computation of the Lorentz invariant quantum parameter
            chipa = Radiation::compute_chipa(charge_over_mass2,
                     momentum[0][ipart],momentum[1][ipart],momentum[2][ipart],
                     gamma,
                     (*Epart)[ipart].x,(*Epart)[ipart].y,(*Epart)[ipart].z,
                     (*Bpart)[ipart].x,(*Bpart)[ipart].y,(*Bpart)[ipart].z);

            // Debugging
            /*std::cerr << "mc_it_nb: " << mc_it_nb << " "
                      << "chipa: " << chipa
                      << std::endl;*/

            // Discontinuous emission: New emission
            // If tau[ipart] <= 0, this is a new emission
            // We also check that chipa > chipa_threshold,
            // else chipa is too low to induce a discontinuous emission
            if ((chipa > chipa_disc_threshold)
            && (tau[ipart] <= epsilon_tau) )
            {
                // New final optical depth to reach for emision
                tau[ipart] = -log( -Rand::uniform() + 1.0);

                /*if (ipart == 1)
                {
                    std::cerr << "New discontinuous emission" << std::endl;
                    std::cerr << "Tau: " << tau[ipart] << std::endl;
                }*/

            }

            // Discontinuous emission: emission under progress
            // If epsilon_tau > 0
            if (tau[ipart] > epsilon_tau)
            {

                /*if (ipart == 1)
                {
                    std::cerr << "Continue discontinuous emission - "
                              << "tau: " << tau[ipart]
                              << std::endl;
                }*/

                // from the cross section
                temp = nlicsTables.compute_dNphdt(chipa,gamma);

                // Time to discontinuous emission
                // If this time is > the remaining iteration time,
                // we have a synchronization
                emission_time = std::min(tau[ipart]/temp, dt - local_it_time);

                // Update of the optical depth
                tau[ipart] -= temp*emission_time;

                /*if (ipart == 1)
                {
                    std::cerr << "tau: " << tau[ipart] << " "
                              << "temp: " << temp << " "
                              << "tau[ipart]/temp: " << tau[ipart]/temp << " "
                              << "emission_time: " << emission_time << " "
                              << "dt - local_it_time: " << dt - local_it_time << " "
                              << std::endl;
                }*/

                // If the final optical depth is reached
                if (tau[ipart] <= epsilon_tau)
                {

                    // Emission of a photon
                    //if (ipart == 1) {

                        RadiationNlicsMC::photon_emission(chipa,gamma,
                                           momentum[0][ipart],
                                           momentum[1][ipart],
                                           momentum[2][ipart],
                                           nlicsTables);

                        /*std::cerr << "gammapa: " << gamma << " "
                                  << "Px: " << momentum[0][ipart] << " "
                                  << "Py: " << momentum[1][ipart] << " "
                                  << std::endl;*/

                    //}

                    // Optical depth becomes negative meaning
                    // that a new drawing is possible
                    // at the next Monte-Carlo iteration
                    tau[ipart] = -1.;
                }

                // Incrementation of the Monte-Carlo iteration counter
                mc_it_nb ++;
                // Update of the local time
                local_it_time += emission_time;

            }

            // Continuous emission
            // chipa needs to be below the discontinuous threshold
            // chipa needs to be above the continuous threshold
            // No discontunous emission is in progress:
            // tau[ipart] <= epsilon_tau
            else if ((chipa <= chipa_disc_threshold)
            &&  (tau[ipart] <= epsilon_tau)
            &&  (chipa > chipa_cont_threshold))
            {

                /*if (ipart == 1) {
                    std::cerr << "Continuous - "
                              << "chipa: " << chipa << std::endl;
                }*/


                // Remaining time of the iteration
                emission_time = dt - local_it_time;

                // Radiated energy during emission_time
                rad_energy =
                nlicsTables.compute_cont_rad_energy_Ridgers(chipa,
                                                             emission_time);

                // Effect on the momentum
                temp = rad_energy/gamma;
                for ( int i = 0 ; i<3 ; i++ )
                    momentum[i][ipart] -= temp*momentum[i][ipart];

                local_it_time = dt;
            }
            // No emission since chipa is too low
            else if (chipa < chipa_cont_threshold)
            {
                local_it_time = dt;
            }

        }

    }
}

// ---------------------------------------------------------------------------------------------------------------------
//! Perform the phoon emission (creation of a super-photon
//! and slow down of the emitting particle)
//! \param chipa          particle quantum parameter
//! \param gammapa          particle gamma factor
//! \param px             particle momentum in x
//! \param py             particle momentum in y
//! \param pz             particle momentum in z
//! \param nlicsTables    Cross-section data tables and useful functions
//                        for nonlinear inverse Compton scattering
// ---------------------------------------------------------------------------------------------------------------------
void RadiationNlicsMC::photon_emission(double &chipa,
                            double & gammapa,
                            double & px,
                            double & py,
                            double & pz,
                            NlicsTables &nlicsTables)
{
    // ____________________________________________________
    // Parameters
    double chiph;      // Photon quantum parameter
    double gammaph;    // Photon gamma factor
    double inv_old_norm_p;
    double new_norm_p;

    // Get the photon quantum parameter from the table xip
    chiph = nlicsTables.compute_chiph_emission(chipa);

    // compute the photon gamma factor
    gammaph = chiph/chipa*(gammapa-1);

    // ____________________________________________________
    // Creation of the new photon

    // ____________________________________________________
    // Update of the particle properties
    // direction d'emission // direction de l'electron (1/gamma << 1)
    // With momentum conservation
    // kx = px*inv_old_norm_p*gammaph;
    // ky = py*inv_old_norm_p*gammaph;
    // kz = pz*inv_old_norm_p*gammaph;
    // px -= kx
    // py -= ky
    // pz -= kz

    // With energy conservation
    inv_old_norm_p = 1./sqrt(gammapa*gammapa - 1.0);
    gammapa -= gammaph;
    new_norm_p = sqrt(abs(gammapa*gammapa - 1.0));
    px *= new_norm_p * inv_old_norm_p;
    py *= new_norm_p * inv_old_norm_p;
    pz *= new_norm_p * inv_old_norm_p;

    // Debugging
    /*std::cerr << "chipa: " << chipa << " "
              << "chiph: " << chiph << " "
              << "gammapa: " << gammapa << " "
              << "gammaph: " << gammaph << " "
              << "inv_old_norm_p: " << inv_old_norm_p << " "
              << "new_norm_p: " << new_norm_p << " "
              << "" << sqrt(1 + px*px + py*py + pz*pz) << " "
              << std::endl;*/

}