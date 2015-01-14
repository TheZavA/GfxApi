#ifndef	QEF_H_HAS_BEEN_INCLUDED
#define	QEF_H_HAS_BEEN_INCLUDED

#include "../MainClass.h"

struct QEF
{
	QEF()
	{
		clear();
	}

	void clear()
	{
		for (int i = 0; i < 6; i++)
		{
			AtA[i] = 0.f;
		}

		AtB = float3(0.0f);
		BtB = 0.f;
		masspoint = float3(0.0f);
		error = 0.f;
	}

	// construct the QR representation discussed in the paper using normals at the 
	// zero-crossing and the world space position of the zero-crossing 
	void initialise(int count, float normals[6][3], float positions[6][3])
	{
		for (int i = 0; i < count; i++)
		{
			const float3& n = float3(normals[i][0], normals[i][1], normals[i][2]);
			const float3& p = float3(positions[i][0], positions[i][1], positions[i][2]);

			AtA[0] += n.x * n.x;
			AtA[1] += n.x * n.y;
			AtA[2] += n.x * n.z;
			AtA[3] += n.y * n.y;
			AtA[4] += n.y * n.z;
			AtA[5] += n.z * n.z;
            
			const float pn = p.Dot(n);
			AtB += n * pn;
			BtB += pn * pn;
			masspoint += p;
		}

		if (count)
		{
			masspoint /= (float)count;
		}
	}

	// note that even with add operation the masspoint will need to be a sum and need to be
	// divided so that it is actually the masspoint (this is a bit clunky)
	void add(const QEF& other)
	{
		for (int j = 0; j < 6; j++)
		{
			AtA[j] += other.AtA[j];
		}

		AtB += other.AtB;
		BtB += other.BtB;
		masspoint += other.masspoint;
		error += other.error;
	}

	// wrapper around the "calcPoint" function in the original code, optionally get the error
	// as well as the minimising point for the QEF
	float3 solve();

	float			AtA[6];
	float3	    	AtB;
	float			BtB;
	float3	    	masspoint;
	float			error;
};

#endif	// QEF_H_HAS_BEEN_INCLUDED

