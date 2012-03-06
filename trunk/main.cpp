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

#include <UT/UT_Lock.h>

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

	UT_FloatArray l,w,W;

	UT_Vector3Array P,D,N,G;
	
	UT_Vector3Array Cd0,Cd1;

	UT_RefArray<UT_BoundingBox> BB;

	int argc;
	char* argv[4096];

	//CVEX_Context context;
	//bool vexvalid;

	//CVEX_Value *varCd, *varOs;
	//CVEX_Value *varP, *varl, *_varCd, *varr, *varBl, *varid;

	//UT_Lock* lock;
};

VRAY_BeamVolume::VRAY_BeamVolume(UT_String file, UT_String shader) // ++
	: m_valid(0)
	, m_shading(shader)
	//, vexvalid(false)
	//, varCd(NULL), varOs(NULL)
	//, varP(NULL), varl(NULL), _varCd(NULL), varr(NULL), varBl(NULL), varid(NULL)
	//, lock(new UT_Lock)
{
	//context.addInput("P",CVEX_TYPE_VECTOR3,true);
	//context.addInput("Cd",CVEX_TYPE_VECTOR3,true);
	//context.addInput("l",CVEX_TYPE_VECTOR3,true);
	//context.addInput("r",CVEX_TYPE_FLOAT,true);
	//context.addInput("Bl",CVEX_TYPE_FLOAT,true);
	//context.addInput("id",CVEX_TYPE_FLOAT,true);
		
	//char *argv[4096];
	argc = m_shading.parse(argv,4096);

	/*context.load(argc,&argv);

	vexvalid = context.isLoaded();
	
	if(vexvalid)
	{
		varCd = context.findOutput("Cd",CVEX_TYPE_VECTOR3); vexvalid &= (varCd!=NULL); //cout << (vexvalid ? "ok" : "fail") << endl;
		varOs = context.findOutput("Os",CVEX_TYPE_VECTOR3); vexvalid &= (varOs!=NULL); //cout << (vexvalid ? "ok" : "fail") << endl;

		varP = context.findInput("P",CVEX_TYPE_VECTOR3); //vexvalid &= (pvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varl = context.findInput("l",CVEX_TYPE_VECTOR3); //vexvalid &= (lvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		_varCd = context.findInput("Cd",CVEX_TYPE_VECTOR3); //vexvalid &= (_cdinvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varr = context.findInput("r",CVEX_TYPE_FLOAT); //vexvalid &= (rvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varBl = context.findInput("Bl",CVEX_TYPE_FLOAT); //vexvalid &= (blvar!=NULL); cout << (vexvalid ? "ok" : "fail") << endl;
		varid = context.findInput("id",CVEX_TYPE_FLOAT); //vexvalid &= (idvar!=NULL); cout << (vexvalid ? "ok" : "fail") << endl;
	};

	if(!vexvalid)
	{
		cout << "VR_BEAMS: VEX SHADER LOADING FAILED!!!" << endl;
	};*/

	GU_Detail gdp;
	UT_Options ops;
	
	GA_Detail::IOStatus st = gdp.load(file.buffer(),&ops);

	if(!st.success())
	{
		cout << "VR_BEAMS: NOT LOADED GEO:" << endl;
		return;
	};


	int pcnt = gdp.primitives().entries(); 

	l.resize(pcnt);
	P.resize(pcnt);

	D.resize(pcnt);
	N.resize(pcnt);
	G.resize(pcnt);

	BB.resize(pcnt);

	w.resize(pcnt);
	W.resize(pcnt);

	Cd0.resize(pcnt);
	Cd1.resize(pcnt);

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

		P[m_valid] = P0;
		D[m_valid] = P1-P0;
		l[m_valid] = D[m_valid].normalize();

		// W
		if(hw.isValid())
		{
			w[m_valid] = hw.get(p0);;
			W[m_valid] = hw.get(p1);
		}
		else
		{
			w[m_valid] = 1;
			W[m_valid] = 1;
		};

		// CD
		if(hc.isValid())
		{
			Cd0[m_valid] = hc.get(p0);
			Cd1[m_valid] = hc.get(p1);
		}
		else
		{
			Cd0[m_valid] = UT_Vector3(1,1,1);
			Cd1[m_valid] = UT_Vector3(1,1,1);
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
		UT_Vector3 _G = D[m_valid];
		_G.cross(_N);
		_G.normalize();

		_N = _G;
		_N.cross(D[m_valid]);

		N[m_valid]=_N;
		G[m_valid]=_G;

		// BBB
		UT_BoundingBox B(P0,P1);

		float _w = w[m_valid];
		float _W = W[m_valid];

		UT_BoundingBox B0(P0-UT_Vector3(_w,_w,_w),P0+UT_Vector3(_w,_w,_w));
		UT_BoundingBox B1(P1-UT_Vector3(_W,_W,_W),P1+UT_Vector3(_W,_W,_W));

		B.enlargeBounds(B0);
		B.enlargeBounds(B1);
		BB[m_valid] = B;

		m_valid++;
	};
};

VRAY_BeamVolume::~VRAY_BeamVolume()
{
	//delete lock;
}; //++

void VRAY_BeamVolume::getBoxes(UT_RefArray<UT_BoundingBox> &boxes, float radius, float dbound, float zerothreshold) const //++
{
	if(m_valid==0) return;

	float expand = radius+dbound;

	for(int i=0;i<m_valid;i++)
	{
		UT_BoundingBox B = BB[i];
		B.expandBounds(expand,expand,expand);
		boxes.append(B);
	};
};

void VRAY_BeamVolume::getAttributeBinding(
	UT_StringArray &names,
	UT_IntArray &sizes) const // ++
{
	if(m_valid==0) return;

	UT_String Cd = "Cd";
	names.append(Cd);
	sizes.append(3);

	UT_String Os = "Os";
	names.append(Os);
	sizes.append(3);
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
			if(!BB[i].isInside(pos[j])) continue;

			id=float(i);

			_D=D[i]; _N=N[i]; _G=G[i];

			O = pos[j]-P[i];

			p = O.dot(D[i]);

			if(p<-w[i]) continue;
			if(p>(W[i]+l[i])) continue;

			L = l[i];

			v = p/l[i];

			cd0 = Cd0[i]; cd1 = Cd1[i];

			if(p<0)
			{
				if(O.length2() > w[i]*w[i]) continue;
				r = 1-O.length()/w[i];

				aP.append(pos[j]);
				aCd.append(cd0);
				ar.append(r);
				aBl.append(L);

				aid.append(id);

				UT_Vector3 CC(0,_N.dot(O)/w[i],_G.dot(O)/w[i]);
				al.append(CC);

				aI.append(j);
				ccnt++;
				//if(ccnt == COMP_MAX) break;

				continue;
			};

			if(p>l[i])
			{
				UT_Vector3 oo = pos[j]-(P[i]+D[i]*l[i]);
				if(oo.length2() > W[i]*W[i]) continue;
				r = 1-oo.length()/W[i];

				aP.append(pos[j]);
				aCd.append(cd1);
				ar.append(r);
				aBl.append(L);

				aid.append(id);

				UT_Vector3 CC(1,_N.dot(O)/W[i],_G.dot(O)/W[i]);
				al.append(CC);

				aI.append(j);
				ccnt++;
				//if(ccnt == COMP_MAX) break;

				continue;
			};

			O = O-D[i]*p;

			_W = w[i]+p/l[i]*(W[i]-w[i]);

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
	//bool vexvalid;

	CVEX_Value* varCd=NULL, *varOs=NULL;
	CVEX_Value *varP=NULL, *varl=NULL, *_varCd=NULL, *varr=NULL, *varBl=NULL, *varid=NULL;

	context.addInput("P",CVEX_TYPE_VECTOR3,true);
	context.addInput("Cd",CVEX_TYPE_VECTOR3,true);
	context.addInput("l",CVEX_TYPE_VECTOR3,true);
	context.addInput("r",CVEX_TYPE_FLOAT,true);
	context.addInput("Bl",CVEX_TYPE_FLOAT,true);
	context.addInput("id",CVEX_TYPE_FLOAT,true);

	char* _argv[4096];

	memcpy(_argv,argv,sizeof(char*)*4096);

	context.load(argc,_argv);

	bool vexvalid = context.isLoaded();

	if(vexvalid)
	{
		varCd = context.findOutput("Cd",CVEX_TYPE_VECTOR3); vexvalid &= (varCd!=NULL); //cout << (vexvalid ? "ok" : "fail") << endl;
		varOs = context.findOutput("Os",CVEX_TYPE_VECTOR3); vexvalid &= (varOs!=NULL); //cout << (vexvalid ? "ok" : "fail") << endl;

		varP = context.findInput("P",CVEX_TYPE_VECTOR3); //vexvalid &= (pvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varl = context.findInput("l",CVEX_TYPE_VECTOR3); //vexvalid &= (lvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		_varCd = context.findInput("Cd",CVEX_TYPE_VECTOR3); //vexvalid &= (_cdinvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varr = context.findInput("r",CVEX_TYPE_FLOAT); //vexvalid &= (rvar!=NULL);  cout << (vexvalid ? "ok" : "fail") << endl;
		varBl = context.findInput("Bl",CVEX_TYPE_FLOAT); //vexvalid &= (blvar!=NULL); cout << (vexvalid ? "ok" : "fail") << endl;
		varid = context.findInput("id",CVEX_TYPE_FLOAT); //vexvalid &= (idvar!=NULL); cout << (vexvalid ? "ok" : "fail") << endl;
	};

	if(!vexvalid)
	{
		cout << "VR_BEAMS: VEX SHADER LOADING FAILED!!!" << endl;
	};


	UT_Vector3Array resCd, resOs;
	UT_Int32Array resI;

	int cnt = aCd.entries();

	//static_cast<UT_Lock*>(lock)->lock();

	do
	{
		if(!vexvalid) break;
		if(cnt < 1) break;

		// VEX COMPUTE
		float* _P = reinterpret_cast<float*>(aP.array());
		if(varP!=NULL) varP->setData(_P,cnt);

		float* _Cd = reinterpret_cast<float*>(aCd.array());
		if(_varCd!=NULL) _varCd->setData(_Cd,cnt);

		float* _l = reinterpret_cast<float*>(al.array());
		if(varl!=NULL) varl->setData(_l,cnt);

		float* _id = reinterpret_cast<float*>(aid.array());
		if(varid!=NULL) varl->setData(_id,cnt);

		float* _r = reinterpret_cast<float*>(ar.array());
		if(varr!=NULL) varr->setData(_r,cnt);

		float* _Bl = reinterpret_cast<float*>(aBl.array());
		if(varBl!=NULL) varBl->setData(_Bl,cnt);

		float* outCd = new float[3*cnt];
		float* outOs = new float[3*cnt];

		varCd->setData(outCd,cnt);
		varOs->setData(outOs,cnt);

		const_cast<CVEX_Context&>(context).run(cnt,false);

		int curr = aI[0];

		int index = 0;

		UT_Vector3 rCd(0,0,0);
		UT_Vector3 rOs(0,0,0);

		while(index<cnt)
		{
			if(curr != aI[index])
			{
				resCd.append(rCd);
				resOs.append(rOs);
				resI.append(curr);
				curr = aI[index];

				rCd = UT_Vector3(0,0,0);
				rOs = UT_Vector3(0,0,0);
			};
			rCd = rCd
				+UT_Vector3(outCd[3*index+0],outCd[3*index+1],outCd[3*index+2])
				-UT_Vector3(outCd[3*index+0]*rCd[0],outCd[3*index+1]*rCd[1],outCd[3*index+2]*rCd[2]);
			rOs = rOs
				+UT_Vector3(outOs[3*index+0],outOs[3*index+1],outOs[3*index+2])
				-UT_Vector3(outOs[3*index+0]*rOs[0],outOs[3*index+1]*rOs[1],outOs[3*index+2]*rOs[2]);
			index++;
		};

		delete [] outCd;
		delete [] outOs;
	}
	while(false);

	//static_cast<UT_Lock*>(lock)->unlock();

	// RETURN RESULT
	for(int i=0;i<size;i++)
	{
		int index = i*stride;
		data[index+0] = 0; data[index+1] = 0; data[index+2] = 0;
	}
	switch (idx)
	{
	case 0: // "Cd"
		for(int i=0;i<resI.entries();i++)
		{
			int index = resI[i]*stride;
			data[index+0] = resCd[i][0]; data[index+1] = resCd[i][1]; data[index+2] = resCd[i][2];
		};
		break;
	case 1: // "Os"
		for(int i=0;i<resI.entries();i++)
		{
			int index = resI[i]*stride;
			data[index+0] = resOs[i][0]; data[index+1] = resOs[i][1]; data[index+2] = resOs[i][2];
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
	//UT_String surf = "****";
	//import("object:surface",surf);
	//cout << "SURF" << surf << endl;

	openVolumeObject();
	addVolume(new VRAY_BeamVolume(m_Geo,m_Shader), 0.0F);
	//changeSetting("surface", "constant", "object");
	closeObject();
}

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