#include "stdafx.h"
#include "NodeDataRecord.h"
#include "FEAnalysisStep.h"
#include "FEBioMech/FESolidSolver.h"

//-----------------------------------------------------------------------------
void NodeDataRecord::Parse(const char* szexpr)
{
	char szcopy[MAX_STRING] = {0};
	strcpy(szcopy, szexpr);
	char* sz = szcopy, *ch;
	m_data.clear();
	do
	{
		ch = strchr(sz, ';');
		if (ch) *ch++ = 0;
		if      (strcmp(sz, "x" ) == 0) m_data.push_back(X );
		else if (strcmp(sz, "y" ) == 0) m_data.push_back(Y );
		else if (strcmp(sz, "z" ) == 0) m_data.push_back(Z );
		else if (strcmp(sz, "ux") == 0) m_data.push_back(UX);
		else if (strcmp(sz, "uy") == 0) m_data.push_back(UY);
		else if (strcmp(sz, "uz") == 0) m_data.push_back(UZ);
		else if (strcmp(sz, "vx") == 0) m_data.push_back(VX);
		else if (strcmp(sz, "vy") == 0) m_data.push_back(VY);
		else if (strcmp(sz, "vz") == 0) m_data.push_back(VZ);
		else if (strcmp(sz, "Rx") == 0) m_data.push_back(RX);
		else if (strcmp(sz, "Ry") == 0) m_data.push_back(RY);
		else if (strcmp(sz, "Rz") == 0) m_data.push_back(RZ);
		else if (strcmp(sz, "T" ) == 0) m_data.push_back(T );
		else if (strcmp(sz, "p" ) == 0) m_data.push_back(P );
		else if (strcmp(sz, "c" ) == 0) m_data.push_back(C );
		else if (strcmp(sz, "c1" ) == 0) m_data.push_back(C1);
		else if (strcmp(sz, "c2" ) == 0) m_data.push_back(C2);
		else if (strcmp(sz, "c3" ) == 0) m_data.push_back(C3);
		else if (strcmp(sz, "c4" ) == 0) m_data.push_back(C4);
		else if (strcmp(sz, "c5" ) == 0) m_data.push_back(C5);
		else if (strcmp(sz, "c6" ) == 0) m_data.push_back(C6);
		else if (strcmp(sz, "c7" ) == 0) m_data.push_back(C7);
		else if (strcmp(sz, "c8" ) == 0) m_data.push_back(C8);
		else throw UnknownDataField(sz);
		sz = ch;
	}
	while (ch);
}

//-----------------------------------------------------------------------------
double NodeDataRecord::Evaluate(int item, int ndata)
{
	FEMesh& mesh = m_pfem->GetMesh();
	int nnode = item - 1;
	if ((nnode < 0) || (nnode >= mesh.Nodes())) return 0;
	FENode& node = mesh.Node(nnode);
	int* id = node.m_ID;

	double val = 0;
	switch (ndata)
	{
	case X : val = node.m_rt.x; break;
	case Y : val = node.m_rt.y; break;
	case Z : val = node.m_rt.z; break;
	case UX: val = node.m_rt.x - node.m_r0.x; break;
	case UY: val = node.m_rt.y - node.m_r0.y; break;
	case UZ: val = node.m_rt.z - node.m_r0.z; break;
	case VX: val = node.m_vt.x; break;
	case VY: val = node.m_vt.y; break;
	case VZ: val = node.m_vt.z; break;
	case T : val = node.m_T; break;
	case P : val = node.m_pt; break;
	case C : val = node.m_ct[0]; break;
	case C1: val = node.m_ct[0]; break;
	case C2: val = node.m_ct[1]; break;
	case C3: val = node.m_ct[2]; break;
	case C4: val = node.m_ct[3]; break;
	case C5: val = node.m_ct[4]; break;
	case C6: val = node.m_ct[5]; break;
	case C7: val = node.m_ct[6]; break;
	case C8: val = node.m_ct[7]; break;
	default:
		{
			FESolidSolver* psolid_solver = dynamic_cast<FESolidSolver*>(m_pfem->GetCurrentStep()->m_psolver);
			if (psolid_solver)
			{
				vector<double>& Fr = psolid_solver->m_Fr;
				switch (ndata)
				{
				case RX: val = (-id[0] - 2 >= 0 ? Fr[-id[0]-2] : 0);  break;
				case RY: val = (-id[1] - 2 >= 0 ? Fr[-id[1]-2] : 0);  break;
				case RZ: val = (-id[2] - 2 >= 0 ? Fr[-id[2]-2] : 0);  break;
				}
			}
		}
	}
	return val;
}

//-----------------------------------------------------------------------------
void NodeDataRecord::SelectAllItems()
{
	int n = m_pfem->GetMesh().Nodes();
	m_item.resize(n);
	for (int i=0; i<n; ++i) m_item[i] = i+1;
}

//-----------------------------------------------------------------------------
void NodeDataRecord::SetItemList(FENodeSet* pns)
{
	int n = pns->size();
	assert(n);
	m_item.resize(n);
	for (int i=0; i<n; ++i) m_item[i] = (*pns)[i];
}
