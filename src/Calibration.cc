#include "Calibration.hh"

ClassImp(FebexMWD)
ClassImp(MiniballCalibration)

void FebexMWD::DoMWD() {
	
	// For now use James' naming convention and switch later
	unsigned int M = rise_time;
	unsigned int L = window;
	unsigned int torr = decay_time;
	unsigned int cfd_delay = delay_time;

	// Get the trace length
	unsigned int trace_length = trace.size();
	
	// Baseline energy estimation
	float baseline_energy = 0.0;
	
	// resize vectors
	stage1.resize( trace_length, 0.0 );
	stage2.resize( trace_length, 0.0 );
	stage3.resize( trace_length, 0.0 );
	stage4.resize( trace_length, 0.0 );
	shaper.resize( trace_length, 0.0 );
	cfd.resize( trace_length, 0.0 );

	// skip first few samples?
	unsigned int skip = 5;
	
	// Loop over trace and analyse
	for( unsigned int i = skip; i < trace_length; ++i ) {
		
		// CFD trace, do triggering later
		if( i > cfd_delay + skip ) {
			
			// James
			//cfd[i] = (int)trace[i] - (int)trace[i-cfd_delay];

			// Liam
			shaper[i] = trace[i] - trace[i-delay_time];
			cfd[i]  = fraction * shaper[i];
			cfd[i] -= shaper[i-delay_time];

		}
		
		// Now we need to be longer than the gap
		if( i >= M + skip ) {
			
			// MWD stage 1 - difference
			// this is 'D' in James' MATLAB code
			stage1[i]  = (int)trace[i];
			stage1[i] -= (int)trace[i-M];
			
		
			// MWD stage 2 - remove decay and average
			// this is 'MA' in James' MATLAB code
			stage2[i] = 0;
			for( unsigned int j = 0; j < M; ++j )
				stage2[i] += (int)trace[i-j];
			stage2[i] /= torr;
			
			// MWD stage 3 - moving average
			// this is 'MWD' in James' MATLAB code
			stage3[i] = stage1[i] + stage2[i];
			
		}
		
		// MWD stage 4 - energy averaging
		// This is 'T' in James' MWD code
		if( i >= L + skip ){
			
			for( unsigned int j = 0; j < L; ++j )
				stage4[i] += stage3[i-j];
			
			stage4[i] /= L;
			
		}
				
	} // loop over trace
	
	
	// Loop now over the CFD trace until we trigger
	// This is not the same as James' trigger, but it's better
	// plus he has updated his CFD and I don't have the new one
	for( unsigned int i = skip; i < trace_length; ++i ) {
		
		// Do some baseline estimation at the same time
		// Rolling average over the baseline window
		// NB: Not required now because we have an averaged trace
		//baseline_energy += stage4[i] / baseline_length;
		//if( i >= baseline_length ) baseline_energy -= stage4[i-baseline_length] / baseline_length;
		
		// Baseline estimation comes from averaged trace
		// Just in case a trigger comes before the baseline length, use whatever we can
		if( i >= baseline_length ) baseline_energy = stage4[i-baseline_length];
		else baseline_energy = stage4[0];
		
		// Trigger when we pass the threshold on the CFD
		if( i >= cfd_delay &&
		   ( ( cfd[i] > threshold && threshold > 0 ) ||
			( cfd[i] < threshold && threshold < 0 ) ) ) {
			
			// Find zero crossing
			while( cfd[i] * cfd[i-1] > 0 && i < trace_length ) i++;
			
			// Reject incorrect polarity
			if( threshold < 0 && cfd[i-1] > 0 ) continue;
			if( threshold > 0 && cfd[i-1] < 0 ) continue;

			// Check we have enough trace left to analyse
			if( trace_length - i < M )
				break;
			
			// Mark the CFD time
			float cfd_time = (float)i / TMath::Abs(cfd[i]);
			cfd_time += (float)(i-1) / TMath::Abs(cfd[i-1]);
			cfd_time /= 1.0 / TMath::Abs(cfd[i]) + 1.0 / TMath::Abs(cfd[i-1]);
			cfd_list.push_back( cfd_time );
			
			// move to peak of the flat top
			// James just uses the averaging window to find the flat top
			// this always puts it at the very start of the flat top.
			// This is probably correct, but there is sometime an additional
			// paramater to get the centre of the flat top.
			// That parameter (flat_top) is available in this code, but not used yet
			//i += M + cfd_delay + flat_top;
			i += M + cfd_delay;

			// assess the energy from stage 4 and push back
			energy_list.push_back( stage4[i] - baseline_energy );
			
			// Move to the end of the whole thing (flat_top not used)
			//i += L + baseline_length - flat_top;
			i += L + baseline_length;

		} // threshold passed
		
	} // loop over CFD
	
	return;
	
}

MiniballCalibration::MiniballCalibration( std::string filename, std::shared_ptr<MiniballSettings> myset ) {

	SetFile( filename );
	set = myset;
	ReadCalibration();
	fRand = std::make_unique<TRandom>();
		
}

void MiniballCalibration::ReadCalibration() {

	std::unique_ptr<TEnv> config = std::make_unique<TEnv>( fInputFile.data() );
	
	default_MWD_Decay		= 50000;
	default_MWD_Rise		= 100; // M
	default_MWD_Top			= 150; // unused at the moment
	default_MWD_Baseline	= 30;
	default_MWD_Window		= 200; // L
	default_CFD_Delay		= 16;
	default_CFD_Threshold	= 200;
	default_CFD_Fraction	= 0.25; // unused at the moment

	
	// FEBEX initialisation
	fFebexOffset.resize( set->GetNumberOfFebexSfps() );
	fFebexGain.resize( set->GetNumberOfFebexSfps() );
	fFebexGainQuadr.resize( set->GetNumberOfFebexSfps() );
	fFebexThreshold.resize( set->GetNumberOfFebexSfps() );
	fFebexType.resize( set->GetNumberOfFebexSfps() );
	fFebexTime.resize( set->GetNumberOfFebexSfps() );
	fFebexMWD_Decay.resize( set->GetNumberOfFebexSfps() );
	fFebexMWD_Rise.resize( set->GetNumberOfFebexSfps() );
	fFebexMWD_Top.resize( set->GetNumberOfFebexSfps() );
	fFebexMWD_Baseline.resize( set->GetNumberOfFebexSfps() );
	fFebexMWD_Window.resize( set->GetNumberOfFebexSfps() );
	fFebexCFD_Delay.resize( set->GetNumberOfFebexSfps() );
	fFebexCFD_Threshold.resize( set->GetNumberOfFebexSfps() );
	fFebexCFD_Fraction.resize( set->GetNumberOfFebexSfps() );

	// FEBEX parameter read
	for( unsigned char i = 0; i < set->GetNumberOfFebexSfps(); i++ ){

		fFebexOffset[i].resize( set->GetNumberOfFebexBoards() );
		fFebexGain[i].resize( set->GetNumberOfFebexBoards() );
		fFebexGainQuadr[i].resize( set->GetNumberOfFebexBoards() );
		fFebexThreshold[i].resize( set->GetNumberOfFebexBoards() );
		fFebexType[i].resize( set->GetNumberOfFebexBoards() );
		fFebexTime[i].resize( set->GetNumberOfFebexBoards() );
		fFebexMWD_Decay[i].resize( set->GetNumberOfFebexBoards() );
		fFebexMWD_Rise[i].resize( set->GetNumberOfFebexBoards() );
		fFebexMWD_Top[i].resize( set->GetNumberOfFebexBoards() );
		fFebexMWD_Baseline[i].resize( set->GetNumberOfFebexBoards() );
		fFebexMWD_Window[i].resize( set->GetNumberOfFebexBoards() );
		fFebexCFD_Delay[i].resize( set->GetNumberOfFebexBoards() );
		fFebexCFD_Threshold[i].resize( set->GetNumberOfFebexBoards() );
		fFebexCFD_Fraction[i].resize( set->GetNumberOfFebexBoards() );

		for( unsigned char j = 0; j < set->GetNumberOfFebexBoards(); j++ ){

			fFebexOffset[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexGain[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexGainQuadr[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexThreshold[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexType[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexTime[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexMWD_Decay[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexMWD_Rise[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexMWD_Top[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexMWD_Baseline[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexMWD_Window[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexCFD_Delay[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexCFD_Threshold[i][j].resize( set->GetNumberOfFebexChannels() );
			fFebexCFD_Fraction[i][j].resize( set->GetNumberOfFebexChannels() );

			for( unsigned char k = 0; k < set->GetNumberOfFebexChannels(); k++ ){
				
				fFebexOffset[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.Offset", i, j, k ), 0. );
				fFebexGain[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.Gain", i, j, k ), 0.0015 );
				fFebexGainQuadr[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.GainQuadr", i, j, k ), 0. );
				fFebexThreshold[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.Threshold", i, j, k ), 15000 );
				fFebexType[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.Type", i, j, k ), "Qshort" );
				fFebexTime[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.Time", i, j, k ), (double)0 );
				fFebexMWD_Decay[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.MWD.DecayTime", i, j, k ), (int)default_MWD_Decay );
				fFebexMWD_Rise[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.MWD.RiseTime", i, j, k ), (int)default_MWD_Rise );
				fFebexMWD_Top[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.MWD.FlatTop", i, j, k ), (int)default_MWD_Top );
				fFebexMWD_Baseline[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.MWD.Baseline", i, j, k ), (int)default_MWD_Baseline );
				fFebexMWD_Window[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.MWD.Window", i, j, k ), (int)default_MWD_Window );
				fFebexCFD_Delay[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.CFD.DelayTime", i, j, k ), (int)default_CFD_Delay );
				fFebexCFD_Threshold[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.CFD.Threshold", i, j, k ), (int)default_CFD_Threshold );
				fFebexCFD_Fraction[i][j][k] = config->GetValue( Form( "febex_%d_%d_%d.CFD.Fraction", i, j, k ), default_CFD_Fraction );

			} // k: channel
			
		} // j: board
		
	} // i: sfp

}

float MiniballCalibration::FebexEnergy( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int raw ) {
	
	float energy, raw_rand;
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	    board < set->GetNumberOfFebexBoards() &&
	       ch < set->GetNumberOfFebexChannels() ) {

		raw_rand = raw + 0.5 - fRand->Uniform();

		energy  = fFebexGainQuadr[sfp][board][ch] * raw_rand * raw_rand;
		energy += fFebexGain[sfp][board][ch] * raw_rand;
		energy += fFebexOffset[sfp][board][ch];

		// Check if we have defaults
		if( TMath::Abs( fFebexGainQuadr[sfp][board][ch] ) < 1e-6 &&
		    TMath::Abs( fFebexGain[sfp][board][ch] - 1.0 ) < 1e-6 &&
		    TMath::Abs( fFebexOffset[sfp][board][ch] ) < 1e-6 )
			
			return raw;
		
		else return energy;
		
	}
	
	return -1;
	
}

FebexMWD MiniballCalibration::DoMWD( unsigned char sfp, unsigned char board, unsigned char ch, std::vector<unsigned short> trace ) {
	
	// Create a FebexMWD class to hold the info
	FebexMWD mwd;
	
	// Check if it's a valid event first
	if(   sfp < set->GetNumberOfFebexSfps() &&
	    board < set->GetNumberOfFebexBoards() &&
	       ch < set->GetNumberOfFebexChannels() ) {

		// Set the parameters of the MWD
		mwd.SetTrace( trace );
		mwd.SetRiseTime( fFebexMWD_Rise[sfp][board][ch] );
		mwd.SetDecayTime( fFebexMWD_Decay[sfp][board][ch] );
		mwd.SetFlatTop( fFebexMWD_Top[sfp][board][ch] );
		mwd.SetBaseline( fFebexMWD_Baseline[sfp][board][ch] );
		mwd.SetWindow( fFebexMWD_Window[sfp][board][ch] );
		mwd.SetDelayTime( fFebexCFD_Delay[sfp][board][ch] );
		mwd.SetThreshold( fFebexCFD_Threshold[sfp][board][ch] );
		mwd.SetFraction( fFebexCFD_Fraction[sfp][board][ch] );

		// Run the MWD
		mwd.DoMWD();
		
	}

	return mwd;
	
}

unsigned int MiniballCalibration::FebexThreshold( unsigned char sfp, unsigned char board, unsigned char ch ) {
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	    board < set->GetNumberOfFebexBoards() &&
	       ch < set->GetNumberOfFebexChannels() ) {

		return fFebexThreshold[sfp][board][ch];
		
	}
	
	return -1;
	
}

long MiniballCalibration::FebexTime( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	    board < set->GetNumberOfFebexBoards() &&
	       ch < set->GetNumberOfFebexChannels() ) {

		return fFebexTime[sfp][board][ch];
		
	}
	
	return 0;
	
}

std::string MiniballCalibration::FebexType( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexType[sfp][board][ch];
		
	}
	
	return 0;
	
}

void MiniballCalibration::SetMWDDecay( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int decay ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexMWD_Decay[sfp][board][ch] = decay;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetMWDRise( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int rise ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexMWD_Rise[sfp][board][ch] = rise;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetMWDTop( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int top ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexMWD_Top[sfp][board][ch] = top;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetMWDBaseline( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int baseline_length ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexMWD_Baseline[sfp][board][ch] = baseline_length;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetMWDWindow( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int window ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexMWD_Window[sfp][board][ch] = window;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetCFDFraction( unsigned char sfp, unsigned char board, unsigned char ch, float fraction ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexCFD_Fraction[sfp][board][ch] = fraction;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetCFDDelay( unsigned char sfp, unsigned char board, unsigned char ch, unsigned int delay ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexCFD_Delay[sfp][board][ch] = delay;
		return;
		
	}
	
	else return;
	
}

void MiniballCalibration::SetCFDThreshold( unsigned char sfp, unsigned char board, unsigned char ch, int threshold ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		fFebexCFD_Threshold[sfp][board][ch] = threshold;
		return;
		
	}
	
	else return;
	
}

unsigned int MiniballCalibration::GetMWDDecay( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexMWD_Decay[sfp][board][ch];
		
	}
	
	else return 0;
	
}

unsigned int MiniballCalibration::GetMWDRise( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexMWD_Rise[sfp][board][ch];
		
	}
	
	else return 0;
	
}

unsigned int MiniballCalibration::GetMWDTop( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexMWD_Top[sfp][board][ch];
		
	}
	
	else return 0;
	
}

unsigned int MiniballCalibration::GetMWDBaseline( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexMWD_Baseline[sfp][board][ch];
		
	}
	
	else return 0;
	
}

unsigned int MiniballCalibration::GetMWDWindow( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexMWD_Window[sfp][board][ch];
		
	}
	
	else return 0;
	
}

float MiniballCalibration::GetCFDFraction( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexCFD_Fraction[sfp][board][ch];
		
	}
	
	else return 0;
	
}

unsigned int MiniballCalibration::GetCFDDelay( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexCFD_Delay[sfp][board][ch];
		
	}
	
	else return 0;
	
}

int MiniballCalibration::GetCFDThreshold( unsigned char sfp, unsigned char board, unsigned char ch ){
	
	if(   sfp < set->GetNumberOfFebexSfps() &&
	   board < set->GetNumberOfFebexBoards() &&
	   ch < set->GetNumberOfFebexChannels() ) {
		
		return fFebexCFD_Threshold[sfp][board][ch];
		
	}
	
	else return 0;
	
}

