#include "DiagnosticPhaseSpace.h"

#include <iomanip>
#include <string>
#include <iomanip>

#include "PicParams.h"
#include "DiagParams.h"
#include "SmileiMPI.h"
#include "ElectroMagn.h"
#include "Field1D.h"
#include "Field.h"
#include "DiagnosticPhase2DxPx.h"

using namespace std;

DiagnosticPhaseSpace::~DiagnosticPhaseSpace() {
}

void DiagnosticPhaseSpace::close() {
	if (fileId != 0) {
		for (unsigned int i =0 ; i < vecDiagPhase.size(); i++) {
			vecDiagPhase[i]->close(smpi_);
			DEBUG("here");
		}
		H5Fclose(fileId);
	}
}



DiagnosticPhaseSpace::DiagnosticPhaseSpace(PicParams* params, DiagParams* diagParams, SmileiMPI* smpi) : smpi_(smpi), fileId(0), ndim(params->nDim_particle) {
	for (unsigned int i =0 ; i < diagParams->vecPhase.size(); i++) {
		DiagnosticPhase *diagPhase=NULL;
		
		hid_t gid=0;
		if (smpi_->isMaster()) {
			if (i==0) {
				ostringstream file_name("");
				file_name<<"PhaseSpace.h5";
				fileId = H5Fcreate( file_name.str().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
			}
			ostringstream groupName("");
			groupName << "ps_" << i << "_" << diagParams->vecPhase[i].kind;
			gid = H5Gcreate(fileId, groupName.str().c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		}
		if (params->geometry == "1d3v") {
			if (diagParams->vecPhase[i].kind == "xpx") {
				diagPhase =  new DiagnosticPhase2DxPx(diagParams->vecPhase[i],gid);
			}
		} else {
			ERROR("DiagnosticPhase not implemented for geometry " << params->geometry);
		}
		
		if (diagPhase) {
			vecDiagPhase.push_back(diagPhase);	
		}
	}
}

void DiagnosticPhaseSpace::run(int timestep, std::vector<Species*>& vecSpecies) {
	
	vector<DiagnosticPhase*> vecDiagPhaseToRun;	
	for (unsigned int i =0 ; i < vecDiagPhase.size(); i++) {
		if (timestep%vecDiagPhase[i]->every==0) vecDiagPhaseToRun.push_back(vecDiagPhase[i]);
	}
	
	if (vecDiagPhaseToRun.size()>0) {
		for (unsigned int j=0; j < vecSpecies.size(); j++) {
			
			vector<DiagnosticPhase*> vecDiagPhaseToRun2;	
			for (unsigned int i =0 ; i < vecDiagPhaseToRun.size(); i++) {
				if(find(vecDiagPhaseToRun[i]->my_species.begin(), vecDiagPhaseToRun[i]->my_species.end(), vecSpecies[j]->name_str) != vecDiagPhaseToRun[i]->my_species.end()) { 
					vecDiagPhaseToRun2.push_back(vecDiagPhaseToRun[i]);
				}
			}

			partStruct my_part;
			my_part.pos.resize(ndim);
			my_part.mom.resize(3);
			
			if (vecDiagPhaseToRun2.size()>0) {
				
				for (unsigned int ibin = 0 ; ibin < vecSpecies[j]->bmin.size() ; ibin++) {
					for (int iPart=vecSpecies[j]->bmin[ibin] ; iPart<vecSpecies[j]->bmax[ibin]; iPart++ ) {
						for (unsigned int i =0 ; i < vecDiagPhaseToRun2.size(); i++) {
							for(unsigned int k=0;k<ndim;k++) {
								my_part.pos[k]=vecSpecies[j]->particles.position(k,iPart);
							}
							for(unsigned int k=0;k<3;k++) {
								my_part.mom[k]=vecSpecies[j]->particles.momentum(k,iPart);
							}
							my_part.weight=vecSpecies[j]->particles.weight(iPart);
							my_part.charge=vecSpecies[j]->particles.charge(iPart);
							vecDiagPhaseToRun2[i]->doSomething(my_part);
						}						
					}
				}
				for (unsigned int i =0 ; i < vecDiagPhaseToRun2.size(); i++) {
					DEBUG(vecSpecies[j]->name_str << " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
					vecDiagPhaseToRun2[i]->writeData(timestep, vecSpecies[j]->name_str, smpi_);
				}				
			}
			
			
//			for (unsigned int i=0; i < vecDiagPhase.size(); i++) {
//				DEBUG("here we have to do something " << vecDiagPhaseToRun.size());
//			}
		}
	}
	

//	//! momentum min
//	std::vector< std::vector<double> > momentum_min;
//	
//	//! momentum max
//	std::vector< std::vector<double> > momentum_max;
//	
//	//! gamma min
//	std::vector<double> lorentz_factor_min;
//	
//	//! gamma max
//	std::vector<double> lorentz_factor_max;
//
//	//! momentum min
//	momentum_min.resize(vecSpecies.size());
//	momentum_max.resize(vecSpecies.size());
//	
//	
//	for (unsigned int i=0; i<vecSpecies.size(); i++) {
//		momentum_min[i].resize(3);
//		momentum_max[i].resize(3);
//	}
//	
//	lorentz_factor_min.resize(vecSpecies.size());
//	lorentz_factor_max.resize(vecSpecies.size());
//	
//	for (unsigned int j=0; j < vecSpecies.size(); j++) {
//		
//		// initialize momentum min/max for phase space diags
//		if (vecSpecies[j]->particles.size()>0) {
//			for (unsigned int i=0; i<3; i++) {
//				momentum_min[j][i] = momentum_max[j][i] = vecSpecies[j]->particles.momentum(i,0);
//			}
//			lorentz_factor_min[j]=lorentz_factor_max[j]=vecSpecies[j]->particles.lor_fac(0);
//		}
//		
//		for (unsigned int ibin = 0 ; ibin < vecSpecies[j]->bmin.size() ; ibin++) {
//			for (int iPart=vecSpecies[j]->bmin[ibin] ; iPart<vecSpecies[j]->bmax[ibin]; iPart++ ) {
//				
//				for (unsigned int i=0; i<3; i++) {
//					if (vecSpecies[j]->particles.momentum(i,iPart) < momentum_min[j][i]) momentum_min[j][i] = vecSpecies[j]->particles.momentum(i,iPart);
//					if (vecSpecies[j]->particles.momentum(i,iPart) > momentum_max[j][i]) momentum_max[j][i] = vecSpecies[j]->particles.momentum(i,iPart);					
//				}
//				if (vecSpecies[j]->particles.lor_fac(iPart) < lorentz_factor_min[j]) lorentz_factor_min[j] = vecSpecies[j]->particles.lor_fac(iPart);
//				if (vecSpecies[j]->particles.lor_fac(iPart) > lorentz_factor_max[j]) lorentz_factor_max[j] = vecSpecies[j]->particles.lor_fac(iPart);
//				
//			}
//		}
//	}
//	
//	for (unsigned int j=0; j < vecSpecies.size(); j++) {
//		for (unsigned int i=0; i<3; i++) {
//			double tempmin=momentum_min[j][i];
//			MPI_Allreduce(&tempmin,&momentum_min[j][i],1,MPI_DOUBLE,MPI_MIN,MPI_COMM_WORLD);
//			double tempmax=momentum_max[j][i];
//			MPI_Allreduce(&tempmax,&momentum_max[j][i],1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
//		}
//		double gammamin=lorentz_factor_min[j];
//		MPI_Allreduce(&gammamin,&lorentz_factor_min[j],1,MPI_DOUBLE,MPI_MIN,MPI_COMM_WORLD);
//		double gammamax=lorentz_factor_max[j];
//		MPI_Allreduce(&gammamax,&lorentz_factor_max[j],1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
//
//		if (smpi_->isMaster()) {
//			for (unsigned int k=0; k <momentum_min[j].size(); k++) {				
//				DEBUG(timestep << " spec. " << j << " min: " << momentum_min[j][k] << " max: "<< momentum_max[j][k]);
//			}
//			DEBUG(timestep << " spec. " << j << " Lmin: " << lorentz_factor_min[j] << " Lmax: "<< lorentz_factor_max[j]);
//		}
//	}

}
