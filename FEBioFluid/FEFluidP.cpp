#include "stdafx.h"
#include "FEFluidP.h"
#include "FECore/FECoreKernel.h"

// define the material parameters
BEGIN_FECORE_CLASS(FEFluidP, FEMaterial)
    // material properties
    ADD_PROPERTY(m_pFluid, "fluid");
END_FECORE_CLASS();

//============================================================================
// FEFluidP
//============================================================================

//-----------------------------------------------------------------------------
//! FEFluidP constructor

FEFluidP::FEFluidP(FEModel* pfem) : FEMaterial(pfem)
{
    m_pFluid = 0;
}

//-----------------------------------------------------------------------------
// returns a pointer to a new material point object
FEMaterialPoint* FEFluidP::CreateMaterialPointData()
{
    FEFluidMaterialPoint* fpt = new FEFluidMaterialPoint();
    return fpt;
}

//-----------------------------------------------------------------------------
// initialize
bool FEFluidP::Init()
{
    return FEMaterial::Init();
}
