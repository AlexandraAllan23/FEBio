#pragma once
#include "FEMaterial.h"

//-----------------------------------------------------------------------------
//! Elastic mixtures

//! This class describes a mixture of elastic solids.  The user must declare
//! elastic solids that can be combined within this class.  The stress and
//! tangent tensors evaluated in this class represent the sum of the respective
//! tensors of all the solids forming the mixture.

class FEElasticMixture : public FEElasticMaterial
{
public:
	FEElasticMixture() {}
		
public:
	enum				{NMAT = 6};		//!< maximum allowable number of materials
	int					m_nMat;			//!< number of elastic materials in mixture
	int					m_iMat[NMAT];	//!< material ID of elastic materials (one-based!)
	FEElasticMaterial*	m_pMat[NMAT];	//!< pointers to elastic materials
		
public:
	//! calculate stress at material point
	virtual mat3ds Stress(FEMaterialPoint& pt);
		
	//! calculate tangent stiffness at material point
	virtual tens4ds Tangent(FEMaterialPoint& pt);
		
	//! data initialization and checking
	void Init();
		
	//! return bulk modulus
	double BulkModulus();
		
	// declare as registered
	DECLARE_REGISTERED(FEElasticMixture);
		
	// declare the parameter list
	DECLARE_PARAMETER_LIST();
};
