#include "stdafx.h"
#include "FEMicroMaterial.h"
#include "FECore/FEElemElemList.h"
#include "FECore/log.h"
#include "FESolidSolver2.h"
#include "FEElasticSolidDomain.h"
#include "FECore/FEAnalysis.h"
#include "FEBioXML/FEBioImport.h"
#include "FEBioPlot/FEBioPlotFile.h"
#include <FECore/mat6d.h>
#include <FECore/BC.h>
#include <sstream>

//=============================================================================
FERVEProbe::FERVEProbe(FEModel& fem, FEModel& rve, const char* szfile) : FECallBack(&fem, CB_ALWAYS), m_rve(rve), m_file(szfile) 
{
	m_bdebug = false;
}

bool FERVEProbe::Execute(FEModel& fem, int nwhen)
{
	if (nwhen == CB_INIT)	// initialize the plot file
	{
		// create a plot file
		m_xplt = new FEBioPlotFile(m_rve);
		if (m_xplt->Open(m_rve, m_file.c_str()) == false)
		{
			felog.printf("Failed creating probe.\n\n");
			delete m_xplt; m_xplt = 0;
		}

		// write the initial state
		Save();
	}
	else if (nwhen == CB_MINOR_ITERS)
	{
		if (m_bdebug) Save();
	}
	else if (nwhen == CB_MAJOR_ITERS)	// store the current state
	{
		Save();
	}
	else if (nwhen == CB_SOLVED)	// clean up
	{
		if (m_xplt) delete m_xplt;
		m_xplt = 0;
	}

	return true;
}

void FERVEProbe::Save()
{
	if (m_xplt) m_xplt->Write(m_rve);
}

//=============================================================================
FEMicroMaterialPoint::FEMicroMaterialPoint(FEMaterialPoint* mp) : FEMaterialPoint(mp)
{
	m_macro_energy = 0.;
	m_micro_energy = 0.;
	m_energy_diff = 0.;
	
	m_macro_energy_inc = 0.;
	m_micro_energy_inc = 0.;
}

//-----------------------------------------------------------------------------
//! Initialize material point data
void FEMicroMaterialPoint::Init()
{
	FEMaterialPoint::Init();
	m_F_prev.unit();
}

//-----------------------------------------------------------------------------
//! Initialize material point data
void FEMicroMaterialPoint::Update(const FETimeInfo& timeInfo)
{
	FEMaterialPoint::Update(timeInfo);
	FEElasticMaterialPoint& pt = *ExtractData<FEElasticMaterialPoint>();
	m_F_prev = pt.m_F;
}

//-----------------------------------------------------------------------------
//! create a shallow copy
FEMaterialPoint* FEMicroMaterialPoint::Copy()
{
	FEMicroMaterialPoint* pt = new FEMicroMaterialPoint(m_pNext?m_pNext->Copy():0);
	return pt;
}

//-----------------------------------------------------------------------------
//! serialize material point data
void FEMicroMaterialPoint::Serialize(DumpStream& ar)
{
	FEMaterialPoint::Serialize(ar);
	if (ar.IsSaving())
	{
	}
	else
	{
	}
}

//=============================================================================
BEGIN_PARAMETER_LIST(FEMicroProbe, FEMaterial)
	ADD_PARAMETER(m_neid  , FE_PARAM_INT   , "element_id");
	ADD_PARAMETER(m_ngp   , FE_PARAM_INT   , "gausspt"   );
	ADD_PARAMETER(m_szfile, FE_PARAM_STRING, "file"      );
	ADD_PARAMETER(m_bdebug, FE_PARAM_BOOL  , "debug"     );
END_PARAMETER_LIST();

//-----------------------------------------------------------------------------
FEMicroProbe::FEMicroProbe(FEModel* pfem) : FEMaterial(pfem)
{
	m_neid = -1;	// invalid element - this must be defined by user
	m_ngp = 1;		// by default, first gauss point (note is one-based!)
	sprintf(m_szfile, "rve.xplt");
	m_probe = 0;
}

FEMicroProbe::~FEMicroProbe()
{
	if (m_probe) delete m_probe;
}

//=============================================================================

//-----------------------------------------------------------------------------
// define the material parameters
BEGIN_PARAMETER_LIST(FEMicroMaterial, FEElasticMaterial)
	ADD_PARAMETER(m_szrve    , FE_PARAM_STRING, "RVE"     );
	ADD_PARAMETER(m_szbc     , FE_PARAM_STRING, "bc_set"  );
	ADD_PARAMETER(m_bperiodic, FE_PARAM_BOOL  , "periodic");
END_PARAMETER_LIST();

//-----------------------------------------------------------------------------
FEMicroMaterial::FEMicroMaterial(FEModel* pfem) : FEElasticMaterial(pfem)
{
	// initialize parameters
	m_szrve[0] = 0;
	m_szbc[0] = 0;
	m_bperiodic = false;	// use displacement BCs by default

	AddProperty(&m_probe, "probe", false);
}

//-----------------------------------------------------------------------------
FEMicroMaterial::~FEMicroMaterial(void)
{
}

//-----------------------------------------------------------------------------
FEMaterialPoint* FEMicroMaterial::CreateMaterialPointData()
{
	return new FEMicroMaterialPoint(new FEElasticMaterialPoint);
}

//-----------------------------------------------------------------------------
bool FEMicroMaterial::Init()
{
	if (FEElasticMaterial::Init() == false) return false;

	// load the RVE model
	FEBioImport fim;
	if (fim.Load(m_mrve, m_szrve) == false)
	{
		return MaterialError("An error occured trying to read the RVE model from file %s.", m_szrve);
	}

	// the logfile is a shared resource between the master FEM and the RVE
	// in order not to corrupt the logfile we don't print anything for
	// the RVE problem.
	Logfile::MODE nmode = felog.GetMode();
	felog.SetMode(Logfile::NEVER);

	// initialize the RVE model
	// This also creates the necessary boundary conditions
	bool bret = m_mrve.InitRVE(m_bperiodic, m_szbc); 

	// reset the logfile mode
	felog.SetMode(nmode);

	if (bret == false) return MaterialError("An error occurred preparing RVE model");

	return true;
}

//-----------------------------------------------------------------------------
//! Assign the prescribed displacement to the boundary nodes.
void FEMicroMaterial::UpdateBC(FEModel& rve, mat3d& F)
{
	// get the mesh
	FEMesh& m = rve.GetMesh();

	// displacement gradient
	mat3d U = F - mat3dd(1.0);

	// assign new DC's for the boundary nodes
	FEPrescribedDOF& dx = dynamic_cast<FEPrescribedDOF&>(*rve.PrescribedBC(0));
	FEPrescribedDOF& dy = dynamic_cast<FEPrescribedDOF&>(*rve.PrescribedBC(1));
	FEPrescribedDOF& dz = dynamic_cast<FEPrescribedDOF&>(*rve.PrescribedBC(2));

	int n = dx.Items();
	for (int j=0; j<n; ++j)
	{
		int nid = dx.NodeID(j);
		FENode& node = m.Node(nid);

		vec3d& r0 = node.m_r0;
		vec3d dr = U*r0;

		dx.SetNodeScale(j, dr.x);
		dy.SetNodeScale(j, dr.y);
		dz.SetNodeScale(j, dr.z);
	}

	if (m_bperiodic)
	{
		// loop over periodic boundaries
		for (int i=0; i<3; ++i)
		{
			FEPeriodicBoundary1O* pc = dynamic_cast<FEPeriodicBoundary1O*>(rve.SurfacePairInteraction(i));
			assert(pc);

			pc->m_Fmacro = F;
		}
	}
}

//-----------------------------------------------------------------------------
// LTE - Note that this function is not used in the first-order implemenetation
mat3ds FEMicroMaterial::Stress(FEMaterialPoint &mp)
{
	// get the deformation gradient
	FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();
	FEMicroMaterialPoint& mmpt = *mp.ExtractData<FEMicroMaterialPoint>();
	mat3d F = pt.m_F;
	
	// update the BC's
	UpdateBC(mmpt.m_rve, F);

	// solve the RVE
	Logfile::MODE nmode = felog.GetMode(); felog.SetMode(Logfile::NEVER);
	bool bret = mmpt.m_rve.Solve();
	felog.SetMode(nmode);

	// make sure it converged
	if (bret == false) throw FEMultiScaleException(-1, -1);

	// calculate the averaged Cauchy stress
	mat3ds sa = AveragedStress(mmpt.m_rve, mp);
	
	// calculate the difference between the macro and micro energy for Hill-Mandel condition
	mmpt.m_micro_energy = micro_energy(mmpt.m_rve);	
	
	return sa;
}

//-----------------------------------------------------------------------------
// The stiffness is evaluated at the same time the stress is evaluated so we 
// can just return it here. Note that this assumes that the stress function 
// is always called prior to the tangent function.
tens4ds FEMicroMaterial::Tangent(FEMaterialPoint &mp)
{
	FEMicroMaterialPoint& mmpt = *mp.ExtractData<FEMicroMaterialPoint>();
	return AveragedStiffness(mmpt.m_rve, mp);
}

//-----------------------------------------------------------------------------
//! Calculate the average stress from the RVE solution.
mat3ds FEMicroMaterial::AveragedStress(FEModel& rve, FEMaterialPoint &mp)
{
	FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();
	double J = pt.m_J;

	// get the RVE mesh
	FEMesh& m = rve.GetMesh();

	mat3d T; T.zero();

	// for periodic BC's we take the reaction forces directly from the periodic constraints
	// TODO: Figure out a way to store all the reaction forces on the nodes
	//       That way, we don't need to do this anymore.
	if (m_bperiodic)
	{
		// get the reaction for from the periodic constraints
		for (int i=0; i<3; ++i)
		{
			FEPeriodicBoundary1O* pbc = dynamic_cast<FEPeriodicBoundary1O*>(rve.SurfacePairInteraction(i));
			assert(pbc);
			FEPeriodicSurface& ss = pbc->m_ss;
			int N = ss.Nodes();
			for (int i=0; i<N; ++i)
			{
				FENode& node = ss.Node(i);
				vec3d f = ss.m_Fr[i];

				// We multiply by two since the reaction forces are only stored at the slave surface 
				// and we also need to sum over the master nodes (NOTE: should I figure out a way to 
				// store the reaction forces on the master nodes as well?)
				T += (f & node.m_rt)*2.0;
			}
		}
	}

	// get the reaction force vector from the solid solver
	// (We also need to do this for the periodic BC, since at the prescribed nodes,
	// the contact forces will be zero). 
	const int dof_X = rve.GetDOFIndex("x");
	const int dof_Y = rve.GetDOFIndex("y");
	const int dof_Z = rve.GetDOFIndex("z");
	FEAnalysis* pstep = rve.GetCurrentStep();
	FESolidSolver2* ps = dynamic_cast<FESolidSolver2*>(pstep->GetFESolver());
	assert(ps);
	vector<double>& R = ps->m_Fr;

	// calculate the averaged stress
	// TODO: This might be more efficient if we keep a list of boundary nodes
	int NN = m.Nodes();
	for (int j=0; j<NN; ++j)
	{
		if (m_mrve.IsBoundaryNode(j))
		{
			FENode& n = m.Node(j);
			vec3d f;
			f.x = R[-n.m_ID[dof_X]-2];
			f.y = R[-n.m_ID[dof_Y]-2];
			f.z = R[-n.m_ID[dof_Z]-2];
			T += f & n.m_rt;
		}
	}

	double V0 = m_mrve.InitialVolume();
	return T.sym() / (J*V0);
}

//-----------------------------------------------------------------------------
//! Calculate the average stiffness from the RVE solution.
tens4ds FEMicroMaterial::AveragedStiffness(FEModel& rve, FEMaterialPoint &mp)
{
	FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();

	// get the mesh
	FEMesh& m = rve.GetMesh();

	// the element's stiffness matrix
	matrix ke;

	// element's residual
	vector<double> fe;

	// calculate the center point
	vec3d rc(0,0,0);
	for (int k=0; k<m.Nodes(); ++k) rc += m.Node(k).m_rt;
	rc /= (double) m.Nodes();

	// elasticity tensor
	tens4ds c(0.);
		
	// calculate the stiffness matrix and residual
	for (int k=0; k<m.Domains(); ++k)
	{
		FEElasticSolidDomain& bd = dynamic_cast<FEElasticSolidDomain&>(m.Domain(k));
		int NS = bd.Elements();
		for (int n=0; n<NS; ++n)
		{
			FESolidElement& e = bd.Element(n);

			// create the element's stiffness matrix
			int ne = e.Nodes();
			int ndof = 3*ne;
			ke.resize(ndof, ndof);
			ke.zero();

			// calculate the element's stiffness matrix
			bd.ElementStiffness(rve.GetTime(), n, ke);

			// create the element's residual
			fe.assign(ndof, 0);

			// calculate the element's residual
			bd.ElementInternalForce(e, fe);

			// loop over the element's nodes
			for (int i=0; i<ne; ++i)
			{
				FENode& ni = m.Node(e.m_node[i]);
				for (int j=0; j<ne; ++j)
				{
					FENode& nj = m.Node(e.m_node[j]);
					if (m_mrve.IsBoundaryNode(e.m_node[i]) && m_mrve.IsBoundaryNode(e.m_node[j]))
					{
						// both nodes are boundary nodes
						// so grab the element's submatrix
						mat3d K;
						ke.get(3*i, 3*j, K);

						// get the nodal positions
						vec3d ri = ni.m_rt - rc;
						vec3d rj = nj.m_rt - rc;

						// create the elasticity tensor
						c += dyad4s(ri, K, rj);
					}
				}
			}
		}
	}

	// divide by volume
	double V0 = m_mrve.InitialVolume();
	double Vi = 1.0/(pt.m_J * V0);
	c *= Vi; 

	return c;
}

//-----------------------------------------------------------------------------
//! Calculate the "energy" of the RVE model, i.e. the volume averaged of PK1:F
double FEMicroMaterial::micro_energy(FEModel& rve)
{
	double E_avg = 0.0;
	double V0 = 0.0;
	FEMesh& m = rve.GetMesh();
	for (int k=0; k<m.Domains(); ++k)
	{
		FESolidDomain& dom = static_cast<FESolidDomain&>(m.Domain(k));
		for (int i=0; i<dom.Elements(); ++i)
		{
			FESolidElement& el = dom.Element(i);
			int nint = el.GaussPoints();
			double* w = el.GaussWeights();
			
			for (int n=0; n<nint; ++n)
			{
				FEMaterialPoint& mp = *el.GetMaterialPoint(n);
				FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();

				mat3d& F = pt.m_F;
				double J = F.det();
				mat3ds& s = pt.m_s;	// Cauchy stress

				// PK1 stress
				mat3d Pk1 = J*s*F.transinv();

				double energy = Pk1.dotdot(F);

				double J0 = dom.detJ0(el, n);		
				E_avg += energy*w[n]*J0;

				V0 += w[n]*J0;
			}
		}
	}

	return E_avg/V0;
}

//-----------------------------------------------------------------------------
//! Calculate the average stress from the RVE solution.
mat3d FEMicroMaterial::AveragedStressPK1(FEModel& rve, FEMaterialPoint &mp)
{
	FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();
	mat3d F = pt.m_F;
	double J = pt.m_J;
	
	// get the RVE mesh
	FEMesh& m = rve.GetMesh();

	mat3d PK1; PK1.zero();

	// for periodic BC's we take the reaction forces directly from the periodic constraints
	if (m_bperiodic)
	{
		// get the reaction for from the periodic constraints
		for (int i=0; i<3; ++i)
		{
			FEPeriodicBoundary1O* pbc = dynamic_cast<FEPeriodicBoundary1O*>(rve.SurfacePairInteraction(i));
			assert(pbc);
			FEPeriodicSurface& ss = pbc->m_ss;
			int N = ss.Nodes();
			for (int i=0; i<N; ++i)
			{
				FENode& node = ss.Node(i);
				vec3d f = ss.m_Fr[i];

				// We multiply by two since the reaction forces are only stored at the slave surface 
				// and we also need to sum over the master nodes (NOTE: should I figure out a way to 
				// store the reaction forces on the master nodes as well?)
				PK1 += (f & node.m_r0)*2.0;
			}
		}
	}

	// get the reaction force vector from the solid solver
	// (We also need to do this for the periodic BC, since at the prescribed nodes,
	// the contact forces will be zero). 
	const int dof_X = rve.GetDOFIndex("x");
	const int dof_Y = rve.GetDOFIndex("y");
	const int dof_Z = rve.GetDOFIndex("z");
	FEAnalysis* pstep = rve.GetCurrentStep();
	FESolidSolver2* ps = dynamic_cast<FESolidSolver2*>(pstep->GetFESolver());
	assert(ps);
	vector<double>& R = ps->m_Fr;
	FEPrescribedDOF& dc = dynamic_cast<FEPrescribedDOF&>(*rve.PrescribedBC(0));
	int nitems = dc.Items();
	for (int i=0; i<nitems; ++i)
	{
		FENode& n = m.Node(dc.NodeID(i));
		vec3d f;
		f.x = R[-n.m_ID[dof_X]-2];
		f.y = R[-n.m_ID[dof_Y]-2];
		f.z = R[-n.m_ID[dof_Z]-2];
		PK1 += f & n.m_r0;
	}

	double V0 = m_mrve.InitialVolume();
	return PK1 / V0;
}

//-----------------------------------------------------------------------------
//! Calculate the average stress from the RVE solution.
mat3ds FEMicroMaterial::AveragedStressPK2(FEModel& rve, FEMaterialPoint &mp)
{
	FEElasticMaterialPoint& pt = *mp.ExtractData<FEElasticMaterialPoint>();
	mat3d F = pt.m_F;
	double J = pt.m_J;
	mat3d Finv = F.inverse();

	// get the RVE mesh
	FEMesh& m = rve.GetMesh();

	mat3d S; S.zero();

	// for periodic BC's we take the reaction forces directly from the periodic constraints
	if (m_bperiodic)
	{
		// get the reaction for from the periodic constraints
		for (int i=0; i<3; ++i)
		{
			FEPeriodicBoundary1O* pbc = dynamic_cast<FEPeriodicBoundary1O*>(rve.SurfacePairInteraction(i));
			assert(pbc);
			FEPeriodicSurface& ss = pbc->m_ss;
			int N = ss.Nodes();
			for (int i=0; i<N; ++i)
			{
				FENode& node = ss.Node(i);
				vec3d f = ss.m_Fr[i];
				vec3d f0 = Finv*f;

				// We multiply by two since the reaction forces are only stored at the slave surface 
				// and we also need to sum over the master nodes (NOTE: should I figure out a way to 
				// store the reaction forces on the master nodes as well?)
				S += (f0 & node.m_r0)*2.0;
			}
		}
	}

	// get the reaction force vector from the solid solver
	// (We also need to do this for the periodic BC, since at the prescribed nodes,
	// the contact forces will be zero). 
	const int dof_X = rve.GetDOFIndex("x");
	const int dof_Y = rve.GetDOFIndex("y");
	const int dof_Z = rve.GetDOFIndex("z");
	FEAnalysis* pstep = rve.GetCurrentStep();
	FESolidSolver2* ps = dynamic_cast<FESolidSolver2*>(pstep->GetFESolver());
	assert(ps);
	vector<double>& R = ps->m_Fr;
	FEPrescribedDOF& dc = dynamic_cast<FEPrescribedDOF&>(*rve.PrescribedBC(0));
	int nitems = dc.Items();
	for (int i=0; i<nitems; ++i)
	{
		FENode& n = m.Node(dc.NodeID(i));
		vec3d f;
		f.x = R[-n.m_ID[dof_X]-2];
		f.y = R[-n.m_ID[dof_Y]-2];
		f.z = R[-n.m_ID[dof_Z]-2];
		vec3d f0 = Finv*f;
		S += f0 & n.m_r0;
	}

	double V0 = m_mrve.InitialVolume();
	return S.sym() / V0;
}
