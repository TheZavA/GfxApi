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

#ifndef NOISEPP_Y_H
#define NOISEPP_Y_H

#include "NoisePipeline.h"
#include "NoiseModule.h"

namespace noisepp
{
	template <class PipelineElement>
	class YElement : public PipelineElement
	{
		
		public:
			YElement ()
			{
			}
			virtual Real getValue (Real x, Cache *cache) const
			{
				return 0;
			}
			virtual Real getValue (Real x, Real y, Cache *cache) const
			{
				return y;
			}
			virtual Real getValue (Real x, Real y, Real z, Cache *cache) const
			{
				return y;
			}
	};

	typedef YElement<PipelineElement1D> YElement1D;
	typedef YElement<PipelineElement2D> YElement2D;
	typedef YElement<PipelineElement3D> YElement3D;

	/** Module which returns a constant value.
		Returns the specified constant value.
	*/
	class YModule : public Module
	{
		
		public:
			/// Constructor.
			YModule() : Module(0) 
            {
			}
		
			/// @copydoc noisepp::Module::addToPipeline()
			ElementID addToPipeline (Pipeline1D *pipe) const
			{
				return pipe->addElement (this, new YElement1D());
			}
			/// @copydoc noisepp::Module::addToPipeline()
			ElementID addToPipeline (Pipeline2D *pipe) const
			{
				return pipe->addElement (this, new YElement2D());
			}
			/// @copydoc noisepp::Module::addToPipeline()
			ElementID addToPipeline (Pipeline3D *pipe) const
			{
				return pipe->addElement (this, new YElement3D());
			}
			/// @copydoc noisepp::Module::getType()
			ModuleTypeId getType() const { return MODULE_Y; }
	};
};

#endif
