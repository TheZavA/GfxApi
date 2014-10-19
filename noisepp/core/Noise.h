// Noise++ Library
// Copyright (c) 2008, Urs C. Hanselmann
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef NOISEPP_H
#define NOISEPP_h

#include "NoiseStdHeaders.h"
#include "NoiseExceptions.h"
#include "NoiseMath.h"
#include "NoisePipeline.h"
#include "NoisePipelineJobs.h"
#include "NoiseModule.h"
#include "NoisePerlin.h"
#include "NoiseBillow.h"
#include "NoiseAddition.h"
#include "NoiseAbsolute.h"
#include "NoiseBlend.h"
#include "NoiseCheckerboard.h"
#include "NoiseClamp.h"
#include "NoiseConstant.h"
#include "NoiseCurve.h"
#include "NoiseExponent.h"
#include "NoiseInvert.h"
#include "NoiseInverseMul.h"
#include "NoiseMaximum.h"
#include "NoiseMinimum.h"
#include "NoiseMultiply.h"
#include "NoisePower.h"
#include "NoiseRidgedMulti.h"
#include "NoiseScaleBias.h"
#include "NoiseSelect.h"
#include "NoiseScalePoint.h"
#include "NoiseTurbulence.h"
#include "NoiseTerrace.h"
#include "NoiseTranslatePoint.h"
#include "NoiseVoronoi.h"
#include "NoiseY.h"

#if NOISEPP_ENABLE_THREADS
#include "NoiseThreadedPipeline.h"
#endif

#endif
