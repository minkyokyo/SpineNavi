#include "ApiUtility.hpp"
#include "naviHelpers.hpp"

#include "CArmCalibration.h"

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

bool CalibrateIntrinsics(const std::vector<std::string>& imgFileNames, float& fx, float& fy, float& cx, float& cy, float& s, std::vector<float>& distCoeffs)
{
	return false;
}

bool CalibrateExtrinsics(const std::vector<std::string>& imgFileNames, glm::fmat4x4& matLS2WS)
{
	return false;
}


#define PAIR_MAKE(P2D, P3D) std::pair<cv::Point2f, cv::Point3f>(cv::Point2f(P2D.x, P2D.y), cv::Point3f(P3D.x, P3D.y, P3D.z))
#define double_vec3(D) ((double*)D.data)[0], ((double*)D.data)[1], ((double*)D.data)[2]
inline bool CalibrteCamLocalFrame(const vector<glm::fvec2>& points_2d, const vector<glm::fvec3>& points_3dws, const glm::fmat4x4& mat_ws2clf,
	const float fx, const float fy, const float cx, const float cy, glm::fmat4x4& mat_rscs2clf, float* err, int* num_samples,
	vector<pair<Point2f, Point3f>>& pair_pts)//, const int img_w, const int img_h

{
	if (points_2d.size() == 0) return false;
	if (points_2d.size() != points_3dws.size()) return false;
	using namespace glm;

	//	pair<Point2f, Point3f>& _pair = pair_pts

	vector<Point3f> points_buf_3d_clf;
	vector<Point2f> points_buf_2d;
	for (int i = 0; i < (int)pair_pts.size(); i++)
	{
		pair<Point2f, Point3f>& _pair = pair_pts[i];
		points_buf_2d.push_back(std::get<0>(_pair));
		points_buf_3d_clf.push_back(std::get<1>(_pair));
	}

	int num_incoming_pts = (int)points_3dws.size();
	for (int i = 0; i < num_incoming_pts; i++)
	{
		Point2f p2d = *(Point2f*)&points_2d[i];
		glm::fvec3 ppp = vzmutils::transformPos(points_3dws[i], mat_ws2clf);
		Point3f p3d = *(Point3f*)&ppp;

		points_buf_2d.push_back(p2d);
		points_buf_3d_clf.push_back(p3d);
	}

	if (num_samples) *num_samples = points_buf_2d.size();
	if (points_buf_2d.size() < 12)
	{
		for (int i = 0; i < (int)points_buf_2d.size(); i++)
		{
			Point2f p2d = *(Point2f*)&points_buf_2d[i];
			Point3f p3d = *(Point3f*)&points_buf_3d_clf[i];

			pair_pts.push_back(PAIR_MAKE(p2d, p3d));
		}
		return false;
	}

	Mat cam_mat = cv::Mat::zeros(3, 3, CV_64FC1); // intrinsic camera parameters
	cam_mat.at<double>(0, 0) = fx;       //      [ fx   0  cx ]
	cam_mat.at<double>(1, 1) = fy;       //      [  0  fy  cy ]
	cam_mat.at<double>(0, 2) = cx;       //      [  0   0   1 ]
	cam_mat.at<double>(1, 2) = cy;
	cam_mat.at<double>(2, 2) = 1;
	cv::Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64FC1);    // vector of distortion coefficients
	cv::Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output rotation vector
	cv::Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);          // output translation vector
	//cv::solvePnP(Mat(*(vector<Point3f>*)&points_buf_3d_clf), Mat(*(vector<Point2f>*)&points_buf_2d), cam_mat, distCoeffs, rvec, tvec, false, SOLVEPNP_DLS);
	//cv::solvePnP(Mat(*(vector<Point3f>*)&points_buf_3d_clf), Mat(*(vector<Point2f>*)&points_buf_2d), cam_mat, distCoeffs, rvec, tvec, true, SOLVEPNP_ITERATIVE);
	cv::solvePnP(points_buf_3d_clf, points_buf_2d, cam_mat, distCoeffs, rvec, tvec, false, SOLVEPNP_DLS);
	cv::solvePnP(points_buf_3d_clf, points_buf_2d, cam_mat, distCoeffs, rvec, tvec, true, SOLVEPNP_ITERATIVE);

	float err_proj = 0;
	{
		vector<cv::Point2f> reprojectPoints;
		cv::projectPoints(Mat(*(vector<Point3f>*) & points_buf_3d_clf), rvec, tvec, cam_mat, cv::noArray(), reprojectPoints);
		float reproj_err_sum = 0.;
		reproj_err_sum = cv::norm(Mat(reprojectPoints), Mat(*(vector<Point2f>*) & points_buf_2d)); //  default L2
		err_proj = sqrt(reproj_err_sum * reproj_err_sum / points_buf_2d.size());
		//cout << "PnP reprojection error : " << err_proj << " pixels, # of point pairs L " << points_buf_2d.size() << endl;
	}

	const float err_criterion = 7.f;
	Mat inliers_ids;
	if ((pair_pts.size() > 20 && err_proj > 5.f)
		|| (pair_pts.size() > 50 && err_proj > 3.f)
		|| (pair_pts.size() > 100 && err_proj > 2.f))
	{
		float confidence = 0.9f;
		if (pair_pts.size() >= 100) confidence = 0.8f;
		cv::solvePnPRansac(Mat(*(vector<Point3f>*) & points_buf_3d_clf), Mat(*(vector<Point2f>*) & points_buf_2d), cam_mat, distCoeffs, rvec, tvec, true, 5, err_criterion, confidence, inliers_ids, SOLVEPNP_ITERATIVE);
		cout << "# of inliers : " << inliers_ids.rows << endl;
		if (inliers_ids.rows > 0)
		{
			vector<Point3f> points_buf_3d_clf_tmp = points_buf_3d_clf;
			vector<Point2f> points_buf_2d_tmp = points_buf_2d;
			points_buf_3d_clf.clear();
			points_buf_2d.clear();
			for (int i = 0; i < inliers_ids.rows; i++)
			{
				int index = inliers_ids.at<int>(i, 0);
				//cout << i << ",";
				points_buf_3d_clf.push_back(points_buf_3d_clf_tmp[index]);
				points_buf_2d.push_back(points_buf_2d_tmp[index]);
			}
			//cout << endl;
		}
	}

	pair_pts.clear();
	for (int i = 0; i < (int)points_buf_2d.size(); i++)
	{
		Point2f p2d = *(Point2f*)&points_buf_2d[i];
		Point3f p3d = *(Point3f*)&points_buf_3d_clf[i];

		pair_pts.push_back(PAIR_MAKE(p2d, p3d));
	}

	//cv::calibrateCamera(Mat(*(vector<Point3f>*)&points_buf_3d_clf), Mat(*(vector<Point2f>*)&points_buf_2d), Size(img_w, img_h), cam_mat, distCoeffs, rvec, tvec, 
	//	CALIB_USE_INTRINSIC_GUESS | CALIB_FIX_PRINCIPAL_POINT | CALIB_FIX_ASPECT_RATIO | CALIB_ZERO_TANGENT_DIST | CALIB_FIX_K1 | CALIB_FIX_K2 | CALIB_FIX_K3 | CALIB_FIX_K4 | CALIB_FIX_K5 | CALIB_FIX_K6, 
	//	TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 30, DBL_EPSILON));

	glm::fvec3 rot_v = glm::fvec3(double_vec3(rvec));
	float rad = glm::length(rot_v);

	fmat4x4 mat_clf2cvf = glm::translate(glm::fvec3(double_vec3(tvec)));
	mat_clf2cvf = glm::rotate(mat_clf2cvf, rad, rot_v / rad);
	fmat4x4 mat_cvf2clf = glm::inverse(mat_clf2cvf);

	fmat4x4 mat_cvf2rscs = glm::rotate((float)CV_PI, glm::fvec3(1, 0, 0));
	fmat4x4 mat_rscs2cvf = glm::inverse(mat_cvf2rscs);

	mat_rscs2clf = mat_cvf2clf * mat_rscs2cvf;

	if (err)
	{
		vector<cv::Point2f> reprojectPoints;
		cv::projectPoints(Mat(*(vector<Point3f>*) & points_buf_3d_clf), rvec, tvec, cam_mat, cv::noArray(), reprojectPoints);

		float reproj_err_sum = 0.;
		reproj_err_sum = cv::norm(Mat(reprojectPoints), Mat(*(vector<Point2f>*) & points_buf_2d)); //  default L2
		*err = sqrt(reproj_err_sum * reproj_err_sum / points_buf_2d.size());
		cout << "PnP reprojection error : " << *err << " pixels, # of point pairs L " << points_buf_2d.size() << endl;
	}

	// TEST //
	//if (points_buf_2d.size() > 100)
	//{
	//	cv::solvePnPRansac(Mat(*(vector<Point3f>*)&points_buf_3d_clf), Mat(*(vector<Point2f>*)&points_buf_2d), cam_mat, distCoeffs, rvec, tvec, false);
	//
	//	vector<cv::Point2f> reprojectPoints;
	//	cv::projectPoints(Mat(*(vector<Point3f>*)&points_buf_3d_clf), rvec, tvec, cam_mat, cv::noArray(), reprojectPoints);
	//	float reproj_err_sum = 0.;
	//	reproj_err_sum = cv::norm(Mat(reprojectPoints), Mat(*(vector<Point2f>*)&points_buf_2d)); //  default L2
	//	*err = sqrt(reproj_err_sum * reproj_err_sum / points_buf_2d.size());
	//	cout << "PnP Ransac reprojection error : " << *err << " pixels" << endl;
	//}

	return true;
};

inline void ComputeCameraStates(const glm::fmat4x4& mat_rscs2clf, // computed through calibration using CalibrteCamLocalFrame
	const glm::fmat4x4& mat_clf2ws, // at every frame, stereo IR cams gives this matrix by tracking rs's rigid IR markers
	vzm::CameraParameters& cam_state // only update CameraParameters::pos, up, view
)
{
	using namespace glm;
	fvec3 pos_c_rscs(0);
	fvec3 vec_up_rscs(0, 1, 0);
	fvec3 vec_view_rscs(0, 0, -1);

	fmat4x4 mat_rscs2ws = mat_clf2ws * mat_rscs2clf;
	pos_c_rscs = vzmutils::transformPos(pos_c_rscs, mat_rscs2ws);
	vec_up_rscs = glm::normalize(vzmutils::transformVec(vec_up_rscs, mat_rscs2ws));
	vec_view_rscs = glm::normalize(vzmutils::transformVec(vec_view_rscs, mat_rscs2ws));

	__cv3__ cam_state.pos = pos_c_rscs;
	__cv3__ cam_state.up = vec_up_rscs;
	__cv3__ cam_state.view = vec_view_rscs;
};