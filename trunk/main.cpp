/*************************************************************
* Copyright (c) 2012 by Egor N. Chashchin. All Rights Reserved.          *
**************************************************************/

/*
*       main.cpp - VRAY_Beams - volumetric line enveloper fx
*
*       Version: 0.9
*       Authors: Egor N. Chashchin
*       Contact: iqcook@gmail.com
*
*/

// CRT
#include <iostream>
using namespace std;

// STDDEF

#ifdef LINUX
#define DLLEXPORT
#define SIZEOF_VOID_P 8
#else
#define DLLEXPORT __declspec(dllexport)
#define MAKING_DSO
#endif

// H
#include <CVEX/CVEX_Context.h>

#include <GU/GU_Detail.h>

#include <OP/OP_OperatorTable.h>

#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>

#include <VRAY/VRAY_Volume.h>
#include <VRAY/VRAY_Procedural.h>

//////////////////////////////////////////////////////////////////////////
// VOLUME FX
//////////////////////////////////////////////////////////////////////////

class VRAY_BeamVolume : public VRAY_Volume
{
public:
	VRAY_BeamVolume(UT_String,UT_String);
	virtual ~VRAY_BeamVolume();
	virtual void getBoxes(UT_RefArray<UT_BoundingBox> &boxes, float radius, float dbound, float zerothreshold) const;
	virtual void getAttributeBinding(UT_StringArray &names, UT_IntArray &sizes) const;
	virtual void evaluate(const UT_Vector3 &pos, const UT_Filter &filter, float radius, float time, int idx, float *data) const;

	virtual void evaluateMulti(const UT_Vector3 *pos,
		const UT_Filter &filter, float radius, const float *time,
		int idx, float *data, int size, int stride) const;

private:
	int m_valid;
	
	UT_String m_shading;

	UT_FloatArray m_l,m_w,m_W;

	UT_Vector3Array m_P,m_D,m_N,m_G;
	
	UT_Vector3Array m_Cd0,m_Cd1;

	UT_RefArray<UT_BoundingBox> m_BB;

	struct rbound {
		UT_BoundingBox bb;
		int beam;
	};

	UT_RefArray<rbound> m_RB;

	int m_argc;
	char* m_argv[4096];
};

VRAY_BeamVolume::VRAY_BeamVolume(UT_String file, UT_String shader) // ++
	: m_valid(0)
	, m_shading(shader)
{
	m_argc = m_shading.parse(m_argv,4096);

	GU_Detail gdp;
	UT_Options ops;
	
	GA_Detail::IOStatus st = gdp.load(file.buffer(),&ops);

	if(!st.success())
	{
		cout << "VR_BEAMS: NOT LOADED GEO:" << endl;
		return;
	};


	int pcnt = gdp.primitives().entries(); 

	m_l.resize(pcnt);
	m_P.resize(pcnt);

	m_D.resize(pcnt);
	m_N.resize(pcnt);
	m_G.resize(pcnt);

	m_BB.resize(pcnt);

	m_w.resize(pcnt);
	m_W.resize(pcnt);

	m_Cd0.resize(pcnt);
	m_Cd1.resize(pcnt);

	GA_ROHandleV3 hp(gdp.getP());
	GA_ROHandleV3 hn(&gdp, GEO_POINT_DICT, "N");
	GA_ROHandleV3 hc(&gdp, GEO_POINT_DICT, "Cd");

	GA_ROHandleF hw(&gdp, GEO_POINT_DICT, "width");

	for (GA_Iterator it(gdp.getPrimitiveRange()); !it.atEnd(); ++it)
	{		
		GEO_Primitive* p = gdp.getGEOPrimitive(it.getOffset());

		if(p->getVertexCount() < 2) continue;

		GA_Offset p0 = p->getPointOffset(0);
		GA_Offset p1 = p->getPointOffset(1);

		UT_Vector3 P0 = hp.get(p0);
		UT_Vector3 P1 = hp.get(p1);

		m_P[m_valid] = P0;
		m_D[m_valid] = P1-P0;
		m_l[m_valid] = m_D[m_valid].normalize();

		// W
		if(hw.isValid())
		{
			m_w[m_valid] = hw.get(p0);;
			m_W[m_valid] = hw.get(p1);
		}
		else
		{
			m_w[m_valid] = 1;
			m_W[m_valid] = 1;
		};

		// CD
		if(hc.isValid())
		{
			m_Cd0[m_valid] = hc.get(p0);
			m_Cd1[m_valid] = hc.get(p1);
		}
		else
		{
			m_Cd0[m_valid] = UT_Vector3(1,1,1);
			m_Cd1[m_valid] = UT_Vector3(1,1,1);
		};

		// N-G
		UT_Vector3 _N;
		if(hn.isValid())
		{
			_N = hn.get(p0);
		}
		else
		{
			_N = UT_Vector3(0,1,0);
		};
		
		_N.normalize();
		UT_Vector3 _G = m_D[m_valid];
		_G.cross(_N);
		_G.normalize();

		_N = _G;
		_N.cross(m_D[m_valid]);

		m_N[m_valid]=_N;
		m_G[m_valid]=_G;

		// BBB
		UT_BoundingBox B(P0,P1);

		float _w = m_w[m_valid];
		float _W = m_W[m_valid];

		UT_BoundingBox B0(P0-UT_Vector3(_w,_w,_w),P0+UT_Vector3(_w,_w,_w));
		UT_BoundingBox B1(P1-UT_Vector3(_W,_W,_W),P1+UT_Vector3(_W,_W,_W));

		B.enlargeBounds(B0);
		B.enlargeBounds(B1);
		m_BB[m_valid] = B;

		UT_Vector3 pos = P0;
		float ll = 0;

		float dw = _W-_w;

		do 
		{
			if(ll >= m_l[m_valid])
			{
				rbound B = {B1,m_valid};
				m_RB.append(B);
				break;
			};

			float w = ll/m_l[m_valid]*dw+_w;

			UT_BoundingBox bb(pos-UT_Vector3(w,w,w),pos+UT_Vector3(w,w,w));

			rbound B = {bb,m_valid};
			m_RB.append(B);

			pos = pos+m_D[m_valid]*w;
			ll += w;
		} 
		while(true);

		m_valid++;
	};
};

VRAY_BeamVolume::~VRAY_BeamVolume() {}; //++

void VRAY_BeamVolume::getBoxes(UT_RefArray<UT_BoundingBox> &boxes, float radius, float dbound, float zerothreshold) const //++
{
	if(m_valid==0) return;

	float expand = radius+dbound;

	//for(int i=0;i<m_valid;i++)
	for(int i=0;i<m_RB.entries();i++)
	{
		UT_BoundingBox B = m_RB[i].bb;
		B.expandBounds(expand,expand,expand);
		boxes.append(B);
	};
};

void VRAY_BeamVolume::getAttributeBinding(
	UT_StringArray &names,
	UT_IntArray &sizes) const // ++
{
	if(m_valid==0) return;

	UT_String* Bs = new UT_String("Bs");
	names.append(*Bs);
	sizes.append(4);
};

//#define COMP_MAX 4

void VRAY_BeamVolume::evaluate(const UT_Vector3 &pos,
	const UT_Filter &filter, float radius, float time,
	int idx, float *data) const
{
	if(m_valid==0) return;
	evaluateMulti(&pos,filter,radius,&time,idx,data,1,0);
	return;
};

void VRAY_BeamVolume::evaluateMulti(const UT_Vector3 *pos,
	const UT_Filter &filter, float radius, const float *time,
	int idx, float *data, int size, int stride) const
{
	if(m_valid==0) return;

	// COLLECT
	UT_Vector3Array aP, al, aCd;
	UT_FloatArray ar, aid, aBl;

	UT_Int32Array aI;

	float p, v, r=1, L=1, _W=1, id=0;
	UT_Vector3 cd0, cd1, O, _N, _G, _D;

	for(int j=0;j<size;j++)
	{
		int ccnt = 0;
		for(int i=0;i<m_valid;i++)
		{
			if(!m_BB[i].isInside(pos[j])) continue;

			id=float(i);

			_D=m_D[i]; _N=m_N[i]; _G=m_G[i];

			O = pos[j]-m_P[i];

			p = O.dot(m_D[i]);

			if(p<-m_w[i]) continue;
			if(p>(m_W[i]+m_l[i])) continue;

			L = m_l[i];

			v = p/m_l[i];

			cd0 = m_Cd0[i]; cd1 = m_Cd1[i];

			if(p<0)
			{
				if(O.length2() > m_w[i]*m_w[i]) continue;
				r = 1-O.length()/m_w[i];

				aP.append(pos[j]);
				aCd.append(cd0);
				ar.append(r);
				aBl.append(L);

				aid.append(id);

				UT_Vector3 CC(0,_N.dot(O)/m_w[i],_G.dot(O)/m_w[i]);
				al.append(CC);

				aI.append(j);
				ccnt++;
				//if(ccnt == COMP_MAX) break;

				continue;
			};

			if(p>m_l[i])
			{
				UT_Vector3 oo = pos[j]-(m_P[i]+m_D[i]*m_l[i]);
				if(oo.length2() > m_W[i]*m_W[i]) continue;
				r = 1-oo.length()/m_W[i];

				aP.append(pos[j]);
				aCd.append(cd1);
				ar.append(r);
				aBl.append(L);

				aid.append(id);

				UT_Vector3 CC(1,_N.dot(O)/m_W[i],_G.dot(O)/m_W[i]);
				al.append(CC);

				aI.append(j);
				ccnt++;
				//if(ccnt == COMP_MAX) break;

				continue;
			};

			O = O-m_D[i]*p;

			_W = m_w[i]+p/m_l[i]*(m_W[i]-m_w[i]);

			if(O.length2()>_W*_W) continue;

			r = 1-O.length()/_W;

			aP.append(pos[j]); // ++
			ar.append(r); //  ++
			aBl.append(L); // ++

			aid.append(id); // ++

			UT_Vector3 CC(v,_N.dot(O)/_W,_G.dot(O)/_W);
			al.append(CC); // ++

			UT_Vector3 dev = cd1-cd0;

			UT_Vector3 C = cd0+v*dev;
			aCd.append(C); // ++

			aI.append(j);
			ccnt++;
			//if(ccnt == COMP_MAX) break;

			continue;
		};
	};

	// CVEX
	CVEX_Context context;

	CVEX_Value* varBs=NULL;
	CVEX_Value *varP=NULL, *varl=NULL, *varCd=NULL, *varr=NULL, *varBl=NULL, *varid=NULL;

	context.addInput("P",CVEX_TYPE_VECTOR3,true);
	context.addInput("Cd",CVEX_TYPE_VECTOR3,true);
	context.addInput("l",CVEX_TYPE_VECTOR3,true);
	context.addInput("r",CVEX_TYPE_FLOAT,true);
	context.addInput("Bl",CVEX_TYPE_FLOAT,true);
	context.addInput("id",CVEX_TYPE_FLOAT,true);

	char* _argv[4096];

	memcpy(_argv,m_argv,sizeof(char*)*4096);

	context.load(m_argc,_argv);

	bool vexvalid = context.isLoaded(); //if(vexvalid) cout << "CONTEXT OK!" << endl;

	if(vexvalid)
	{
		varBs = context.findOutput("Bs",CVEX_TYPE_VECTOR4); vexvalid &= (varBs!=NULL);

		varP = context.findInput("P",CVEX_TYPE_VECTOR3);
		varl = context.findInput("l",CVEX_TYPE_VECTOR3);
		varCd = context.findInput("Cd",CVEX_TYPE_VECTOR3);
		varr = context.findInput("r",CVEX_TYPE_FLOAT);
		varBl = context.findInput("Bl",CVEX_TYPE_FLOAT);
		varid = context.findInput("id",CVEX_TYPE_FLOAT);
	};

	if(!vexvalid)
	{
		cout << "VR_BEAMS: VEX SHADER LOADING FAILED!!!" << endl;
	};

	UT_Vector4Array resBs;
	UT_Int32Array resI;

	int cnt = aCd.entries();

	do
	{
		if(!vexvalid) break;
		if(cnt < 1) break;

		// VEX COMPUTE
		float* _P = reinterpret_cast<float*>(aP.array());
		if(varP!=NULL) varP->setData(_P,cnt);

		float* _Cd = reinterpret_cast<float*>(aCd.array());
		if(varCd!=NULL) varCd->setData(_Cd,cnt);

		float* _l = reinterpret_cast<float*>(al.array());
		if(varl!=NULL) varl->setData(_l,cnt);

		float* _id = reinterpret_cast<float*>(aid.array());
		if(varid!=NULL) varl->setData(_id,cnt);

		float* _r = reinterpret_cast<float*>(ar.array());
		if(varr!=NULL) varr->setData(_r,cnt);

		float* _Bl = reinterpret_cast<float*>(aBl.array());
		if(varBl!=NULL) varBl->setData(_Bl,cnt);

		float* outBs = new float[4*cnt];

		varBs->setData(outBs,cnt);

		const_cast<CVEX_Context&>(context).run(cnt,false);

		int curr = aI[0];

		int index = 0;

		UT_Vector4 rBs(0,0,0,0);

		while(index<cnt)
		{
			if(curr != aI[index])
			{
				resBs.append(rBs);
				resI.append(curr);
				curr = aI[index];

				rBs = UT_Vector4(0,0,0,0);
			};
			rBs = rBs
				+UT_Vector4(outBs[4*index+0],outBs[4*index+1],outBs[4*index+2],outBs[4*index+3])
				-UT_Vector4(outBs[4*index+0]*rBs[0],outBs[4*index+1]*rBs[1],outBs[4*index+2]*rBs[2],outBs[4*index+3]*rBs[3]);
			index++;
		};

		delete [] outBs;
	}
	while(false);

	// RETURN RESULT
	for(int i=0;i<size;i++)
	{
		int index = i*stride;
		data[index+0] = 0; data[index+1] = 0; data[index+2] = 0;; data[index+3] = 0;
	}
	switch (idx)
	{
	case 0: // "Cd"
		for(int i=0;i<resI.entries();i++)
		{
			int index = resI[i]*stride;
			data[index+0] = resBs[i][0]; data[index+1] = resBs[i][1]; data[index+2] = resBs[i][2];; data[index+3] = resBs[i][3];
		};
		break;
	default: 
		UT_ASSERT(0 && "Invalid attribute evaluation");
	};
};

//////////////////////////////////////////////////////////////////////////
// PROCEDURAL
//////////////////////////////////////////////////////////////////////////

class VRAY_Beams: public VRAY_Procedural // ++
{
public:
	VRAY_Beams() : m_Box(0,0,0,0,0,0) {};
	virtual ~VRAY_Beams() {};

	virtual const char	*getClassName() { return "iqBeams"; };

	virtual int		 initialize(const UT_BoundingBox *);
	virtual void	 getBoundingBox(UT_BoundingBox &box) { box=m_Box; };
	virtual void	 render();

	static VRAY_ProceduralArg theArgs[];

private:
	UT_BoundingBox	 m_Box;

	UT_String m_Geo;
	UT_String m_Shader;
	fpreal m_Shutter;
};

int VRAY_Beams::initialize(const UT_BoundingBox *box)
{
	
	if(box!=NULL) m_Box = *box;

	import("file", m_Geo);
	import("beamshader", m_Shader);
	import("shutter", &m_Shutter, 1);

	return 1;
}

void VRAY_Beams::render()
{
	openVolumeObject();
	addVolume(new VRAY_BeamVolume(m_Geo,m_Shader), 0.0F);
	closeObject();
};

///////////////////////////////////////////////////////
// ATTRIBUTES
///////////////////////////////////////////////////////

VRAY_ProceduralArg VRAY_Beams::theArgs[] = {
	VRAY_ProceduralArg("file",		"string",	""),
	VRAY_ProceduralArg("beamshader","string",	""),
	VRAY_ProceduralArg("shutter",	"real",		"0"),
	VRAY_ProceduralArg()
};

// ENTRY POINT
extern "C" {
	VRAY_Procedural *allocProcedural(const char *)
	{
		//cout << "VRAY_Beams: allocProc" << endl;
		return new VRAY_Beams();
	};

	const VRAY_ProceduralArg *getProceduralArgs(const char *)
	{
		//cout << "DBG: VRAY_Beams: getPocArgs" << endl;
		return VRAY_Beams::theArgs;
	};
}