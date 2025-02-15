﻿#include "optitrk.h"

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <set>
#include <queue>
#include <iostream>

#include "./motive/NPTrackingTools.h"

using namespace optitrk;
using namespace std;

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

template<size_t sz> struct bitset_comparer {
	bool operator() (const bitset<sz>& b1, const bitset<sz>& b2) const {
		return b1.to_ulong() < b2.to_ulong();
	}
};

// Local function prototypes
void CheckResult(NPRESULT result);

#define _USE_OPTI_MATH
#ifdef _USE_OPTI_MATH
// Local constants
const float kRadToDeg = 0.0174532925f;

class Point4
{
public:
	Point4(float x, float y, float z, float w);

	float           operator[](int idx) const { return mData[idx]; }
	const float* Data() const { return mData; }

private:
	float           mData[4];
};

class TransformMatrix
{
public:
	TransformMatrix();

	TransformMatrix(float m11, float m12, float m13, float m14,
		float m21, float m22, float m23, float m24,
		float m31, float m32, float m33, float m34,
		float m41, float m42, float m43, float m44);

	void            SetTranslation(float x, float y, float z);
	void            Invert();

	TransformMatrix operator*(const TransformMatrix& rhs);
	Point4          operator*(const Point4& v);

	static TransformMatrix RotateX(float rads);
	static TransformMatrix RotateY(float rads);
	static TransformMatrix RotateZ(float rads);

private:
	float           mData[4][4];
};
#endif

class myListener : public cTTAPIListener
{
public:
	virtual void TTAPICameraConnected(int serialNumber)
	{
		printf("Camera Connected: Serial #%d\n", serialNumber);
	}
	virtual void TTAPICameraDisconnected(int serialNumber)
	{
		printf("Camera Disconnected: Serial #%d\n", serialNumber);
	}
};

bool is_initialized = false;
myListener listener;
bool optitrk::InitOptiTrackLib()
{
	if (is_initialized) return false;
	//printf("== NaturalPoint Tracking Tools API Marker Sample =======---\n");
	//printf("== (C) NaturalPoint, Inc.\n\n");
	//
	//printf("Initializing NaturalPoint Devices\n\n");

	if (TT_Initialize() != NPRESULT_SUCCESS)
	{
		printf("Unable to license Motive API\n");
		return false;
	}

	// Attach listener for camera notifications ==--
	TT_AttachListener(&listener);

	//TT_StreamTrackd(true);

	// Do an update to pick up any recently-arrived cameras.
	TT_Update();

	is_initialized = true;
	return true;
}

bool optitrk::SetCameraSettings(int cam_idx, int video_type, int exposure, int threshold, int intensity)
{
	return TT_SetCameraSettings(cam_idx, video_type, exposure, threshold, intensity);
}

bool optitrk::SetCameraFrameRate(int cam_idx, int frameRate)
{
	return TT_SetCameraFrameRate(cam_idx, frameRate);
}

bool optitrk::LoadProfileAndCalibInfo(const std::string& file_profile, const std::string& file_calib)
{
	// Do an update to pick up any recently-arrived cameras.
	TT_Update();

	// Load a project file from the executable directory.
	printf("Loading Profile: UserProfile.motive\n");
	CheckResult(TT_LoadProfile(file_profile.c_str()));

	// Load a calibration file from the executable directory
	if (file_calib != "") {
		printf("Loading Calibration: Calibration.cal\n\n");
		CheckResult(TT_LoadCalibration(file_calib.c_str()));
	}
	return true;
}

bool optitrk::StoreProfile(const std::string& file_profile)
{
	// Load a project file from the executable directory.
	printf("Loading Profile: UserProfile.motive\n");
	CheckResult(TT_SaveProfile(file_profile.c_str()));

	return true;
}

bool LoadRigidBodies(const std::string& file_rbs)
{
	if (is_initialized) return false;
	if (TT_LoadRigidBodies(file_rbs.c_str()) != NPRESULT_SUCCESS) return false;
	return true;
}

bool SaveRigidBodies(const std::string& file_rbs)
{
	if (is_initialized) return false;
	if (TT_SaveRigidBodies(file_rbs.c_str()) != NPRESULT_SUCCESS) return false;
	return true;
}

bool CreateRigidBody(const std::string& name_rb, const int num_markers, const float* markerlist, int& id)
{
	if (!is_initialized) return false;
	return true;
}

int optitrk::GetMarkersLocation(std::vector<float>* mk_xyz_list, std::vector<float>* mk_residual_list, std::vector<std::bitset<128>>* mk_cid_list)
{
	if (!is_initialized) return false;

	int num_mks = TT_FrameMarkerCount();
	mk_xyz_list->assign(num_mks * 3, 0);
	if (mk_residual_list) mk_residual_list->assign(num_mks, 0);
	if (mk_cid_list) mk_cid_list->assign(num_mks, 0);
	for (int i = 0; i < num_mks; i++)
	{
		mk_xyz_list->at(3 * i + 0) = TT_FrameMarkerX(i);
		mk_xyz_list->at(3 * i + 1) = TT_FrameMarkerY(i);
		mk_xyz_list->at(3 * i + 2) = TT_FrameMarkerZ(i);
		if (mk_residual_list) mk_residual_list->at(i) = TT_FrameMarkerResidual(i);
		if (mk_cid_list)
		{
			Core::cUID cid = TT_FrameMarkerLabel(i);
			std::bitset<128> cid_bs;
			unsigned __int64 lbs = cid.LowBits();
			unsigned __int64 hbs = cid.HighBits();
			cid_bs |= hbs;
			cid_bs <<= 64;
			cid_bs |= lbs;
			//memcpy(&cid_bs[0], &lbs, sizeof(unsigned __int64));
			//memcpy(&cid_bs[64], &hbs, sizeof(unsigned __int64));
			//cout << "mk " << i << " : low " << lbs << ", high " << hbs << endl;
			mk_cid_list->at(i) = cid_bs;
		}
	}

	return num_mks;
}

int optitrk::GetRigidBodies(std::vector<std::string>* rb_names)
{
	int num_rbs = TT_RigidBodyCount();
	if (rb_names)
	{
		for (int i = 0; i < num_rbs; i++)
			rb_names->push_back(TT_RigidBodyName(i));
	}
	return num_rbs;
}

struct rb_tr_smooth_info
{
	int fr_count;
	std::deque<glm::fquat> qt_que;
	std::deque<glm::fvec3> xyz_que;
	rb_tr_smooth_info() {}
	rb_tr_smooth_info(const int _fr_count) {
		fr_count = _fr_count;
	}
};
std::map<int, rb_tr_smooth_info> rb_smooth_frcount;
bool optitrk::SetRigidBodyPropertyByIdx(const int rb_idx, const float smooth_term, const int test_smooth_term)
{
	if (!is_initialized) return false;
	if (TT_RigidBodyName(rb_idx) == NULL) return false;

	RigidBodySolver::cRigidBodySettings rb_settings;
	TT_RigidBodySettings(rb_idx, rb_settings);
	std::cout << "target param of " << rb_idx << " : " << rb_settings.Smoothing << std::endl;
	rb_settings.Smoothing = (double)smooth_term;
	rb_smooth_frcount[rb_idx] = rb_tr_smooth_info(test_smooth_term);
	return TT_SetRigidBodySettings(rb_idx, rb_settings) == NPRESULT_SUCCESS;
}

bool optitrk::SetRigidBodyPropertyByName(const std::string& name, const float smooth_term, const int test_smooth_term)
{
	if (!is_initialized) return false;
	std::vector<std::string> rbNames;
	int numRBs = optitrk::GetRigidBodies(&rbNames);
	int rb_idx = 0;
	for (; rb_idx < numRBs; rb_idx++)
		if (name == rbNames[rb_idx]) break;
	if (rb_idx == numRBs)
		return false;
	return SetRigidBodyPropertyByIdx(rb_idx, smooth_term, test_smooth_term);
}

bool optitrk::SetRigidBodyEnabledbyIdx(const int rb_idx, const bool enabled)
{
	if (!is_initialized) return false;
	if (TT_RigidBodyName(rb_idx) == NULL) return false;

	TT_SetRigidBodyEnabled(rb_idx, enabled);
	return true;
}

bool optitrk::SetRigidBodyEnabledbyName(const std::string& name, const bool enabled)
{
	if (!is_initialized) return false;

	std::vector<std::string> rbNames;
	int numRBs = optitrk::GetRigidBodies(&rbNames);
	int rb_idx = 0;
	for (; rb_idx < numRBs; rb_idx++)
		if (name == rbNames[rb_idx]) break;
	if (rb_idx == numRBs)
		return false;

	TT_SetRigidBodyEnabled(rb_idx, enabled);
	return true;
}

bool optitrk::SetRigidBody(const std::string& name, const int numMKs, const float* mkPosArray)
{
	if (!is_initialized) return false;

	std::vector<std::string> rbNames;
	int numRBs = optitrk::GetRigidBodies(&rbNames);
	int rb_idx = 0;
	for (; rb_idx < numRBs; rb_idx++)
		if (name == rbNames[rb_idx]) break;

	int userId = 1;
	bool isNew = rb_idx == numRBs;
	if (!isNew) {
		int numPrevMKs = TT_RigidBodyMarkerCount(rb_idx);
		userId = TT_RigidBodyUserData(rb_idx);
		isNew = numPrevMKs != numMKs;
		if (isNew) {
			//std::string __rbName = TT_RigidBodyName(rb_idx);
			TT_RemoveRigidBody(rb_idx);
		}
	}

	vector<glm::fvec3> mkMkPos(numMKs);
	memcpy(&mkMkPos[0], mkPosArray, sizeof(glm::fvec3) * numMKs);

	glm::fvec3 mkPivotPos = glm::fvec3(0, 0, 0);
	for (int i = 0; i < numMKs; i++) {
		mkPivotPos += mkMkPos[i];
	}
	mkPivotPos /= (float)numMKs;
	for (int i = 0; i < numMKs; i++) {
		mkMkPos[i] -= mkPivotPos;
	}

	if (isNew) {
		//static int id = 0;
		TT_CreateRigidBody(name.c_str(), userId, numMKs, (float*)&mkMkPos[0]);
	}
	else {
		for (int i = 0; i < numMKs; i++) {
			glm::fvec3 mkPos = mkMkPos[i];
			TT_RigidBodyUpdateMarker(rb_idx, i, &mkPos.x, &mkPos.y, &mkPos.z);
		}
	}

	//std::wstring name_w;
	//name_w.assign(name.begin(), name.end());
	//
	//RigidBodySolver::cRigidBodySettings setting;
	//if (name.length() > RigidBodySolver::kRigidBodyNameMaxLen)
	//	return false;
	//
	//ZeroMemory(setting.mName, RigidBodySolver::kRigidBodyNameMaxLen * sizeof(wchar_t));
	//memcpy(setting.mName, name_w.c_str(), name_w.length() * sizeof(wchar_t));
	//
	//setting.ColorR = (float)(rand() % 100) / 100.f;
	//setting.ColorG = (float)(rand() % 100) / 100.f;
	//setting.ColorB = (float)(rand() % 100) / 100.f;
	//setting.Enabled = true;
	//if (TT_SetRigidBodySettings(rb_idx, setting) != NPRESULT_SUCCESS)
	//	return false;
	//
	cout << "\n" << name << " error : " << TT_RigidBodyMeanError(rb_idx) << endl;
	return true;
}

bool optitrk::Test(float* v) {
	float   yaw, pitch, roll;
	float   x, y, z;
	float   qx, qy, qz, qw;
	return true;
}

bool optitrk::GetRigidBodyLocationByIdx(const int rb_idx, float* mat_ls2ws, std::bitset<128>* cid, float* rb_mse, std::vector<float>* rbmk_xyz_list, std::vector<bool>* mk_tracked_list, std::vector<float>* mk_quality_list, std::string* rb_name, float* qvec, float* tvec)
{
	if (!is_initialized) return false;
	if (rb_name) *rb_name = TT_RigidBodyName(rb_idx);
	if (!TT_IsRigidBodyTracked(rb_idx)) {
		return false;
	}

	float   yaw, pitch, roll;
	float   x, y, z;
	float   qx, qy, qz, qw;
	TT_RigidBodyLocation(rb_idx, &x, &y, &z, &qx, &qy, &qz, &qw, &yaw, &pitch, &roll); // frame info.

	auto it = rb_smooth_frcount.find(rb_idx);
	if (it != rb_smooth_frcount.end()) 
	{
		rb_tr_smooth_info& tr_smooth = it->second;
		if (tr_smooth.fr_count > 1)
		{
			if (tr_smooth.qt_que.size() < tr_smooth.fr_count)
			{
				tr_smooth.qt_que.push_back(glm::fquat(qw, qx, qy, qz));
				tr_smooth.xyz_que.push_back(glm::fvec3(x, y, z));
			}
			else
			{
				tr_smooth.qt_que.pop_front();
				tr_smooth.qt_que.push_back(glm::fquat(qw, qx, qy, qz));
				tr_smooth.xyz_que.pop_front();
				tr_smooth.xyz_que.push_back(glm::fvec3(x, y, z));
			}

			float _weight = 1.f / (float)tr_smooth.qt_que.size();
			glm::fvec3 __xyz_avr = tr_smooth.xyz_que[0];
			glm::fquat __qt_avr = tr_smooth.qt_que[0];
			for (int i = 1; i < (int)tr_smooth.qt_que.size(); i++)
			{
				__xyz_avr = (__xyz_avr + tr_smooth.xyz_que[i]) * 0.5f;
				//glm::fvec3& _xyz = tr_smooth.xyz_que[i];
				//x += _xyz.x * _weight;
				//y += _xyz.y * _weight;
				//z += _xyz.z * _weight;
				__qt_avr = glm::slerp(__qt_avr, tr_smooth.qt_que[i], 0.5f);
			}
			x = __xyz_avr.x;
			y = __xyz_avr.y;
			z = __xyz_avr.z;
			qx = __qt_avr.x;
			qy = __qt_avr.y;
			qz = __qt_avr.z;
			qw = __qt_avr.w;
		}
	}

	using namespace glm;
	glm::fquat qt(qw, qx, qy, qz);
	fmat4x4 mat_r = glm::toMat4(qt);
	fmat4x4 mat_t = glm::translate(fvec3(x, y, z));
	fmat4x4 _mat_ls2ws = mat_t * mat_r;
	//fmat4x4 mat_ws2ls = inverse(_mat_ls2ws);
	//_mat_ls2ws = inverse(_mat_ls2ws);
	*(fmat4x4*)mat_ls2ws = _mat_ls2ws;

	if (qvec)
		*(glm::fquat*)qvec = qt;
	if (tvec)
		*(glm::fvec3*)tvec = fvec3(x, y, z);

	//fvec4 localpt(1, 2, 3, 1);
	//fvec4 worldpt = _mat_ls2ws * localpt;
	//
	//
	//// local-to-world
	//// TEST
	//TransformMatrix xRot(TransformMatrix::RotateX(pitch * kRadToDeg));
	//TransformMatrix yRot(TransformMatrix::RotateY(yaw * kRadToDeg));
	//TransformMatrix zRot(TransformMatrix::RotateZ(roll * kRadToDeg));
	//TransformMatrix worldTransform = xRot * yRot * zRot;
	//worldTransform.SetTranslation(x, y, z);
	//
	//Point4  localPnt(1, 2, 3, 1.0f);
	//Point4  WorldPnt = worldTransform * localPnt;

	//fmat4x4 mat_ls2ws_0 = *(fmat4x4*) mat_ls2ws;
	//int gg = 0;

	int num_mks = TT_RigidBodyMarkerCount(rb_idx);
	std::vector<glm::fvec3> posMKs(num_mks);
	std::vector<bool> isTrackeds(num_mks);
	std::vector<float> qualities(num_mks);
	for (int i = 0; i < num_mks; i++)
	{
		//float mk_x, mk_y, mk_z;
		//// rb markers are defined in local frame 
		//TT_RigidBodyMarker(rb_idx, i, &mk_x, &mk_y, &mk_z); // Get the rigid body's local coordinate for each marker.
		//glm::fvec4 pos(mk_x, mk_y, mk_z, 1.f);
		//pos = _mat_ls2ws * pos;
		//(*rbmk_xyz_list)[i * 3 + 0] = pos.x / pos.w;
		//(*rbmk_xyz_list)[i * 3 + 1] = pos.y / pos.w;
		//(*rbmk_xyz_list)[i * 3 + 2] = pos.z / pos.w;

		//TT_RigidBodyMarker()

		float mx, my, mz;
		bool tracked;
		TT_RigidBodyPlacedMarker(rb_idx, i, tracked, mx, my, mz);
		posMKs[i] = glm::fvec3(mx, my, mz);
		isTrackeds[i] = tracked;

		if (tracked) {
			float _mx, _my, _mz;
			bool _tracked;
			TT_RigidBodyPointCloudMarker(rb_idx, i, _tracked, _mx, _my, _mz);
			qualities[i] = 1.f;
		}
		else
			qualities[i] = 0;
	}

	if (rbmk_xyz_list) {
		rbmk_xyz_list->assign(num_mks * 3, 0);
		memcpy(&rbmk_xyz_list->at(0), &posMKs[0], sizeof(glm::fvec3) * num_mks);
	}
	if (mk_tracked_list) {
		*mk_tracked_list = isTrackeds;
	}

	if (rb_mse) {
		*rb_mse = TT_RigidBodyMeanError(rb_idx);
	}
	
	if (mk_quality_list) {
		*mk_quality_list = qualities;
	}

	if (cid) {
		Core::cUID _cid = TT_RigidBodyID(rb_idx);
		unsigned __int64 lbs = _cid.LowBits();
		unsigned __int64 hbs = _cid.HighBits();
		std::bitset<128> cid_bs;
		cid_bs |= hbs;
		cid_bs <<= 64;
		cid_bs |= lbs;
		*cid = cid_bs;
	}

	return true;
}

bool optitrk::GetRigidBodyLocationByName(const string& name, float* mat_ls2ws, int* rb_idx, std::bitset<128>* cid, float* rb_mse, std::vector<float>* rbmk_xyz_list, std::vector<bool>* mk_tracked_list, std::vector<float>* mk_quality_list)
{
	std::vector<std::string> rbNames;
	int numRBs = optitrk::GetRigidBodies(&rbNames);
	int _rb_idx = 0;
	for (; _rb_idx < numRBs; _rb_idx++)
		if (name == rbNames[_rb_idx]) break;
	if (_rb_idx == numRBs)
		return false;

	if (rb_idx) *rb_idx = _rb_idx;
	return GetRigidBodyLocationByIdx(_rb_idx, mat_ls2ws, cid, rb_mse, rbmk_xyz_list, mk_tracked_list, mk_quality_list, nullptr);
}

bool optitrk::GetCameraLocation(const int cam_idx, float* mat_cam2ws)
{
	if (!is_initialized) return false;
	if (TT_CameraCount() == 0) return false;
	if (cam_idx >= TT_CameraCount() || cam_idx < 0) return false;

	float x = TT_CameraXLocation(cam_idx);
	float y = TT_CameraYLocation(cam_idx);
	float z = TT_CameraZLocation(cam_idx);

	float v[9];
	for (int j = 0; j < 9; j++)
		v[j] = TT_CameraOrientationMatrix(cam_idx, j);

	using namespace glm;
	fmat4x4 mat_t = translate(fvec3(x, y, z));
	fmat4x4 mat_r = fmat4x4(1);
	float* _v = value_ptr(mat_r);
	_v[0] = v[0];
	_v[4] = v[1];
	_v[8] = v[2];
	_v[1] = v[3];
	_v[5] = v[4];
	_v[9] = v[5];
	_v[2] = v[6];
	_v[6] = v[7];
	_v[10] = v[8];

	*(fmat4x4*)mat_cam2ws = mat_t * mat_r;
	return true;
}

bool optitrk::UpdateFrame(bool use_latest)
{
	if (!is_initialized) return false;
	//return use_latest ? TT_UpdateLastestFrame() == NPRESULT_SUCCESS : TT_Update() == NPRESULT_SUCCESS;
	return TT_Update() == NPRESULT_SUCCESS;
}

bool optitrk::DeinitOptiTrackLib()
{
	if (!is_initialized) return false;

	// Detach listener
	TT_DetachListener(&listener);

	// Shutdown API
	CheckResult(TT_Shutdown());
	printf("Bye Bye~ optitracker\n");

	return true;
}

void CheckResult(NPRESULT result)   //== CheckResult function will display errors and ---
									  //== exit application after a key is pressed =====---
{
	if (result != NPRESULT_SUCCESS)
	{
		// Treat all errors as failure conditions.
		printf("Error: %s\n\n(Press any key to continue)\n", TT_GetResultString(result));

		Sleep(20);
		exit(1);
	}
}


#ifdef _USE_OPTI_MATH
//
// Point4
//

Point4::Point4(float x, float y, float z, float w)
{
	mData[0] = x;
	mData[1] = y;
	mData[2] = z;
	mData[3] = w;
}

//
// TransformMatrix
//

TransformMatrix::TransformMatrix()
{
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if (i == j)
			{
				mData[i][j] = 1.0f;
			}
			else
			{
				mData[i][j] = 0.0f;
			}
		}
	}
}

TransformMatrix::TransformMatrix(float m11, float m12, float m13, float m14,
	float m21, float m22, float m23, float m24,
	float m31, float m32, float m33, float m34,
	float m41, float m42, float m43, float m44)
{
	mData[0][0] = m11;
	mData[0][1] = m12;
	mData[0][2] = m13;
	mData[0][3] = m14;
	mData[1][0] = m21;
	mData[1][1] = m22;
	mData[1][2] = m23;
	mData[1][3] = m24;
	mData[2][0] = m31;
	mData[2][1] = m32;
	mData[2][2] = m33;
	mData[2][3] = m34;
	mData[3][0] = m41;
	mData[3][1] = m42;
	mData[3][2] = m43;
	mData[3][3] = m44;
}

void TransformMatrix::SetTranslation(float x, float y, float z)
{
	mData[0][3] = x;
	mData[1][3] = y;
	mData[2][3] = z;
}

void TransformMatrix::Invert()
{
	// Exploit the fact that we are dealing with a rotation matrix + translation component.
	// http://stackoverflow.com/questions/2624422/efficient-4x4-matrix-inverse-affine-transform

	float   tmp;
	float   vals[3];

	// Transpose left-upper 3x3 (rotation) sub-matrix
	tmp = mData[0][1]; mData[0][1] = mData[1][0]; mData[1][0] = tmp;
	tmp = mData[0][2]; mData[0][2] = mData[2][0]; mData[2][0] = tmp;
	tmp = mData[1][2]; mData[1][2] = mData[2][1]; mData[2][1] = tmp;

	// Multiply translation component (last column) by negative inverse of upper-left 3x3.
	for (int i = 0; i < 3; ++i)
	{
		vals[i] = 0.0f;
		for (int j = 0; j < 3; ++j)
		{
			vals[i] += -mData[i][j] * mData[j][3];
		}
	}
	for (int i = 0; i < 3; ++i)
	{
		mData[i][3] = vals[i];
	}
}

TransformMatrix TransformMatrix::RotateX(float rads)
{
	return TransformMatrix(1.0, 0.0, 0.0, 0.0,
		0.0, cos(rads), -sin(rads), 0.0,
		0.0, sin(rads), cos(rads), 0.0,
		0.0, 0.0, 0.0, 1.0);
}

TransformMatrix TransformMatrix::RotateY(float rads)
{
	return TransformMatrix(cos(rads), 0.0, sin(rads), 0.0,
		0.0, 1.0, 0.0, 0.0,
		-sin(rads), 0.0, cos(rads), 0.0,
		0.0, 0.0, 0.0, 1.0);
}

TransformMatrix TransformMatrix::RotateZ(float rads)
{
	return TransformMatrix(cos(rads), -sin(rads), 0.0, 0.0,
		sin(rads), cos(rads), 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0);
}

TransformMatrix TransformMatrix::operator*(const TransformMatrix& rhs)
{
	TransformMatrix result;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			float rowCol = 0.0;
			for (int k = 0; k < 4; ++k)
			{
				rowCol += mData[i][k] * rhs.mData[k][j];
			}
			result.mData[i][j] = rowCol;
		}
	}
	return result;
}

Point4 TransformMatrix::operator*(const Point4& v)
{
	const float* pnt = v.Data();
	float   result[4];

	for (int i = 0; i < 4; ++i)
	{
		float rowCol = 0.0;
		for (int k = 0; k < 4; ++k)
		{
			rowCol += mData[i][k] * pnt[k];
		}
		result[i] = rowCol;
	}
	return Point4(result[0], result[1], result[2], result[3]);
}
#endif