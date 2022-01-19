#ifndef __REACTION_HH
#define __REACTION_HH

#include <iostream>
#include <vector>
#include <string>
#include <map>

#include "TSystem.h"
#include "TEnv.h"
#include "TMath.h"
#include "TObject.h"
#include "TString.h"
#include "TFile.h"
#include "TCutG.h"
#include "TVector3.h"
#include "TF1.h"
#include "TError.h"

// Settings header
#ifndef __SETTINGS_HH
# include "Settings.hh"
#endif

// Miniball Events tree
#ifndef __MINIBALLEVTS_HH
# include "MiniballEvts.hh"
#endif

// Header for geometry calibration
#ifndef __MINIBALLGEOMETRY_HH
# include "MiniballGeometry.hh"
#endif


const double p_mass = 938272.08816;	///< mass of the proton in keV/c^2
const double n_mass = 939565.42052;	///< mass of the neutron in keV/c^2
const double u_mass = 931494.10242;	///< atomic mass unit in keV/c^2

// Element names
const std::vector<std::string> gElName = {
	"n","H","He","Li","Be","B","C","N","O","F","Ne","Na","Mg",
	"Al","Si","P","S","Cl","Ar","K","Ca","Sc","Ti","V","Cr",
	"Mn","Fe","Co","Ni","Cu","Zn","Ga","Ge","As","Se","Br","Kr",
	"Rb","Sr","Y","Zr","Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd",
	"In","Sn","Sb","Te","I","Xe","Cs","Ba","La","Ce","Pr","Nd",
	"Pm","Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb","Lu","Hf",
	"Ta","W","Re","Os","Ir","Pt","Au","Hg","Tl","Pb","Bi","Po",
	"At","Rn","Fr","Ra","Ac","Th","Pa","U","Np","Pu","Am","Cm",
	"Bk","Cf","Es","Fm","Md","No","Lr","Rf","Db","Sg","Bh","Hs",
	"Mt","Ds","Rg","Cn","Nh","Fl","Ms","Lv","Ts","Og","Uue","Ubn"
};


/// A class to read in the reaction file in ROOT's TConfig format.
/// And also to do the physics stuff for the reaction

class Particle : public TObject {
	
public:
	
	// setup functions
	Particle();
	~Particle();
	
	// Get properties
	inline double		GetMass_u(){
		return GetMass() / u_mass;
	};			// returns mass in u
	inline double		GetMass(){
		double mass = (double)GetN() * n_mass;
		mass += (double)GetZ() * p_mass;
		mass -= (double)GetA() * bindingE;
		return mass;
	};		// returns mass in keV/c^2
	inline int			GetA(){ return A; };	// returns mass number
	inline int			GetZ(){ return Z; };
	inline int			GetN(){ return A-Z; };
	inline std::string	GetIsotope(){
		return std::to_string( GetA() ) + gElName.at( GetZ() );
	};
	inline double		GetBindingEnergy(){ return bindingE; };
	inline double		GetEnergyTot(){ return GetEnergy() + GetMass(); };
	inline double		GetBeta(){
		double beta2 = 0.25 * GetMass() + 1.5 * GetEnergy();
		beta2  = TMath::Sqrt( beta2 * GetMass() );
		beta2 -= 0.5 * GetMass();
		return TMath::Sqrt( beta2 / 0.75 * GetMass() );
	};
	inline double		GetGamma(){
		return 1.0 / TMath::Sqrt( 1.0 - TMath::Power( GetBeta(), 2.0 ) );
	};
	inline double		GetEnergy(){ return Elab; };
	inline double		GetEx(){ return Ex; };
	inline double		GetTheta(){ return Theta; };
	inline double		GetThetaCoM(){ return ThetaCoM; };
	inline double		GetPhi(){ return Phi; };
	inline TVector3		GetVector(){
		TVector3 vec(1,0,0);
		vec.SetTheta( GetTheta() );
		vec.SetPhi( GetPhi() );
		return vec;
	};


	// Set properties
	inline void		SetA( int myA ){ A = myA; };
	inline void		SetZ( int myZ ){ Z = myZ; };
	inline void		SetBindingEnergy( double myBE ){ bindingE = myBE; };
	inline void		SetEnergy( double myElab ){ Elab = myElab; };
	inline void		SetEx( double myEx ){ Ex = myEx; };
	inline void		SetTheta( double mytheta ){ Theta = mytheta; };
	inline void		SetThetaCoM( double mytheta ){ ThetaCoM = mytheta; };
	inline void		SetPhi( double myphi ){ Phi = myphi; };


private:
	
	// Properties of reaction particles
	int		A;			///< mass number, A of the particle, obviously
	int		Z; 			///< The Z of the particle, obviously
	double	bindingE;	///< binding energy per nucleon in keV/c^2
	double	Elab;		///< energy in the laboratory system
	double	Ex;			///< excitation energy of the nucleus
	double	Theta;		///< theta in the laboratory system in radians
	double	ThetaCoM;	///< theta in the centre-of-mass system in radians
	double	Phi;		///< phi in the laboratory system in radians

	
	ClassDef( Particle, 1 )
	
};

class Reaction {
	
public:
	
	// setup functions
	Reaction( std::string filename, Settings *myset );
	~Reaction() {};
	
	// Main functions
	void AddBindingEnergy( short Ai, short Zi, TString ame_be_str );
	void ReadMassTables();
	void ReadReaction();
	void SetFile( std::string filename ){
		fInputFile = filename;
	}
	const std::string InputFile(){
		return fInputFile;
	}
	
	// Get values for geometry
	inline float			GetCDDistance( unsigned char det ){
		if( det < cd_dist.size() ) return cd_dist.at(det);
		else return 0.0;
	};
	inline float			GetCDPhiOffset( unsigned char det ){
		if( det < cd_offset.size() ) return cd_offset.at(det);
		else return 0.0;
	};
	inline unsigned int		GetNumberOfParticleThetas() {
		return set->GetNumberOfCDPStrips() * set->GetNumberOfCDDetectors();
	};
	inline double*			GetParticleThetas(){
		std::vector<double> cd_angles;
		for( unsigned char i = 0; i < set->GetNumberOfCDDetectors(); i++ )
			for( unsigned char j = 0; j < set->GetNumberOfCDPStrips(); j++ )
				cd_angles.push_back( GetCDVector(i,0,j,0).Theta() * TMath::RadToDeg() );
		return cd_angles.data();
	};
	TVector3		GetCDVector( unsigned char det, unsigned char sec, unsigned char pid, unsigned char nid );
	TVector3		GetParticleVector( unsigned char det, unsigned char sec, unsigned char pid, unsigned char nid );
	inline float	GetParticleTheta( unsigned char det, unsigned char sec, unsigned char pid, unsigned char nid ){
		return GetParticleVector( det, sec, pid, nid ).Theta();
	};
	inline float	GetParticlePhi( unsigned char det, unsigned char sec, unsigned char pid, unsigned char nid ){
		return GetParticleVector( det, sec, pid, nid ).Phi();
	};
	inline TVector3	GetCDVector( ParticleEvt *p ){
		return GetCDVector( p->GetDetector(), p->GetSector(), p->GetStripP(), p->GetStripN() );
	};
	inline TVector3	GetParticleVector( ParticleEvt *p ){
		return GetParticleVector( p->GetDetector(), p->GetSector(), p->GetStripP(), p->GetStripN() );
	};
	inline float	GetParticleTheta( ParticleEvt *p ){
		return GetParticleTheta( p->GetDetector(), p->GetSector(), p->GetStripP(), p->GetStripN() );
	};
	inline float	GetParticlePhi( ParticleEvt *p ){
		return GetParticlePhi( p->GetDetector(), p->GetSector(), p->GetStripP(), p->GetStripN() );
	};

	// Miniball geometry functions
	inline float	GetGammaTheta( unsigned char clu, unsigned char cry, unsigned char seg ){
		return mb_geo[clu].GetSegTheta( cry, seg );
	};
	inline float	GetGammaPhi( unsigned char clu, unsigned char cry, unsigned char seg ){
		return mb_geo[clu].GetSegPhi( cry, seg );
	};
	inline float	GetGammaTheta( GammaRayEvt *g ){
		return GetGammaTheta( g->GetCluster(), g->GetCrystal(), g->GetSegment() );
	};
	inline float	GetGammaTheta( GammaRayAddbackEvt *g ){
		return GetGammaTheta( g->GetCluster(), g->GetCrystal(), g->GetSegment() );
	};
	inline float	GetGammaPhi( GammaRayEvt *g ){
		return GetGammaPhi( g->GetCluster(), g->GetCrystal(), g->GetSegment() );
	};
	inline float	GetGammaPhi( GammaRayAddbackEvt *g ){
		return GetGammaPhi( g->GetCluster(), g->GetCrystal(), g->GetSegment() );
	};
	
	// SPEDE and electron geometry
	inline float	GetSpedeDistance(){ return spede_dist; };
	inline float	GetSpedePhiOffset(){ return spede_offset; };
	TVector3		GetSpedeVector( unsigned char seg );
	TVector3		GetElectronVector( unsigned char seg );
	inline float	GetElectronTheta( unsigned char seg ){
		return GetElectronVector(seg).Theta();
	};
	inline float	GetElectronTheta( SpedeEvt *s ){
		return GetElectronTheta( s->GetSegment() );
	};
	inline float	GetElectronPhi( unsigned char seg ){
		return GetElectronVector(seg).Phi();
	};
	inline float	GetElectronPhi( SpedeEvt *s ){
		return GetElectronPhi( s->GetSegment() );
	};

	
	// Identify the ejectile and recoil and calculate
	void	IdentifyEjectile( ParticleEvt *p, bool kinflag = false );
	void	IdentifyRecoil( ParticleEvt *p, bool kinflag = false );
	void	CalculateEjectile();
	void	CalculateRecoil();


	// Reaction calculations
	inline double GetQvalue(){
		return Beam.GetMass() + Target.GetMass() -
			Ejectile.GetMass() - Recoil.GetMass();
	};
	inline double GetEnergyTotLab(){
		return Beam.GetEnergyTot() + Target.GetEnergyTot();
	};
	inline double GetEnergyTotCM(){
		double etot = TMath::Power( Beam.GetMass(), 2.0 );
		etot += TMath::Power( Target.GetMass(), 2.0 );
		etot += 2.0 * Beam.GetEnergyTot() * Target.GetMass();
		etot = TMath::Sqrt( etot );
		return etot;
	};
	inline double GetBeta(){
		return TMath::Sqrt( 2.0 * Beam.GetEnergy() / Beam.GetMass() );
	};
	inline double GetGamma(){
		return 1.0 / TMath::Sqrt( 1.0 - TMath::Power( GetBeta(), 2.0 ) );
	};
	inline double GetTau(){
		return Beam.GetMass() / Target.GetMass();
	};
	inline double GetEnergyPrime(){
		return Beam.GetEnergy() - ( Ejectile.GetEx() + Recoil.GetEx() ) * ( 1 + GetTau() );
	};
	inline double GetEpsilon(){
		return TMath::Sqrt( Beam.GetEnergy() / GetEnergyPrime() );
	};

	
	
	// Doppler correction
	double DopplerCorrection( GammaRayEvt *g, bool ejectile );
	double CosTheta( GammaRayEvt *g, bool ejectile );


	// Get EBIS times
	inline double GetEBISOnTime(){ return EBIS_On; };
	inline double GetEBISOffTime(){ return EBIS_Off; };
	inline double GetEBISRatio(){ return EBIS_On / ( EBIS_Off - EBIS_On ); };

	// Get particle gamma coincidence times
	inline double GetParticleGammaPromptTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return pg_prompt[i];
		else return 0;
	};
	inline double GetParticleGammaRandomTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return pg_random[i];
		else return 0;
	};
	inline double GetParticleGammaTimeRatio(){
		return ( pg_prompt[1] - pg_prompt[1] ) / ( pg_random[1] - pg_random[1] );
	};
	inline double GetParticleGammaFillRatio(){
		return pg_ratio;
	};
	inline double GetGammaGammaPromptTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return gg_prompt[i];
		else return 0;
	};
	inline double GetGammaGammaRandomTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return gg_random[i];
		else return 0;
	};
	inline double GetGammaGammaTimeRatio(){
		return ( gg_prompt[1] - gg_prompt[1] ) / ( gg_random[1] - gg_random[1] );
	};
	inline double GetGammaGammaFillRatio(){
		return gg_ratio;
	};
	inline double GetParticleParticlePromptTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return pg_prompt[i];
		else return 0;
	};
	inline double GetParticleParticleRandomTime( unsigned char i ){
		// i = 0 for lower limite and i = 1 for upper limit
		if( i < 2 ) return pp_random[i];
		else return 0;
	};
	inline double GetParticleParticleTimeRatio(){
		return ( pp_prompt[1] - pp_prompt[1] ) / ( pp_random[1] - pp_random[1] );
	};
	inline double GetParticleParticleFillRatio(){
		return pp_ratio;
	};

	
	// Get cuts
	inline TCutG* GetEjectileCut(){ return ejectile_cut; };
	inline TCutG* GetRecoilCut(){ return recoil_cut; };
	
	// Get particles
	inline Particle* GetBeam(){ return &Beam; };
	inline Particle* GetTarget(){ return &Target; };
	inline Particle* GetEjectile(){ return &Ejectile; };
	inline Particle* GetRecoil(){ return &Recoil; };


private:

	std::string fInputFile;
	
	// Settings file
	Settings *set;
	
	// Mass tables
	std::map< std::string, double > ame_be; ///< List of biniding energies from  AME2021

	// Reaction partners
	Particle Beam, Target, Ejectile, Recoil;
	
	// Initial properties from file
	double Eb;		///< laboratory beam energy in keV/u

	// EBIS time windows
	double EBIS_On;		///< beam on max time in ns
	double EBIS_Off;	///< beam off max time in ns
	
	// Particle and Gamma coincidences windows
	int pg_prompt[2];	// particle-gamma prompt
	int pg_random[2];	// particle-gamma random
	int gg_prompt[2];	// gamma-gamma prompt
	int gg_random[2];	// gamma-gamma random
	int pp_prompt[2];	// particle-particle prompt
	int pp_random[2];	// particle-particle random
	float pg_ratio, gg_ratio, pp_ratio; // fill ratios

	// Target offsets
	float x_offset;	///< horizontal offset of the target/beam position, with respect to the CD and Miniball in mm
	float y_offset;	///< vertical offset of the target/beam position, with respect to the CD and Miniball in mm
	float z_offset;	///< lateral offset of the target/beam position, with respect to the only Miniball in mm (cd_dist is independent)

	// CD detector things
	std::vector<float> cd_dist;		///< distance from target to CD detector in mm
	std::vector<float> cd_offset;	///< phi rotation of the CD in radians
	
	// Miniball detector things
	std::vector<MiniballGeometry> mb_geo;
	std::vector<float> mb_theta, mb_phi, mb_alpha, mb_r;

	// SPEDE things
	float spede_dist;	///< distance from target to SPEDE detector
	float spede_offset;	///< phi rotation of the SPEDE detector

	// Cuts
	std::string ejectilecutfile, ejectilecutname;
	std::string recoilcutfile, recoilcutname;
	TFile *cut_file;
	TCutG *ejectile_cut, *recoil_cut;
	
};

#endif