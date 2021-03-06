#include <iostream>
#include <state-observation/dynamics-estimators/lipm-dcm-estimator.hpp>
#include <state-observation/dynamics-estimators/unidim-lipm-dcm-estimator.hpp>
#include <state-observation/tools/probability-law-simulation.hpp>
#include <state-observation/tools/rigid-body-kinematics.hpp>

using namespace stateObservation;

/// @brief runs the basic test
///
/// @return int : 0 if success, nonzero if fails
int testUnidimDcmBiasEstimator(int errorCode)
{
  double w0 = sqrt(cst::gravityConstant / 0.9);
  double dt = 0.005;

  double biasDriftPerSecondStd = 0.005;
  double zmpMeasurementErrorStd = 0.001;
  double dcmMeasurementErrorStd = 0.005;

  int signallength = int(120. / dt);

  ///////////////////////////////////////
  /// Build the ground truth signals
  ///////////////////////////////////////
  tools::ProbabilityLawSimulation ran;
  std::vector<double> dcm(signallength), bias(signallength), zmp(signallength);

  /// set the desired exponential convergence of the DCM
  double lambda = 2;

  /// initialize the dcm and bias to a random value
  dcm[0] = ran.getGaussianScalar(2, 0);
  double initBiasstd = 0.01;

  bias[0] = ran.getGaussianScalar(0.01, 0);
  double deviation = ran.getGaussianScalar(0.05, 0);
  zmp[0] = (lambda / w0 + 1) * dcm[0] + deviation;

  for(int i = 0; i < signallength - 1; ++i)
  {
    /// dcm dynamics
    dcm[i + 1] = dcm[i] + dt * w0 * (dcm[i] - zmp[i]);
    /// drift
    bias[i + 1] = bias[i] + ran.getGaussianScalar(biasDriftPerSecondStd * dt);
    /// set a noisy zmp to create  bounded drift of the DCM
    deviation += ran.getGaussianScalar(0.05, 0);
    zmp[i + 1] = (lambda / w0 + 1) * dcm[i] + ran.getGaussianScalar(0.05, 0) + deviation;
  }

  /////////////////////////////////
  /// Build the measurements
  /////////////////////////////////
  std::vector<double> dcm_m_unbiased(signallength), dcm_m(signallength), zmp_m(signallength);

  for(int i = 0; i < signallength; ++i)
  {
    dcm_m_unbiased[i] = dcm[i] + ran.getGaussianScalar(dcmMeasurementErrorStd);
    dcm_m[i] = dcm_m_unbiased[i] + bias[i];
    zmp_m[i] = zmp[i] + ran.getGaussianScalar(zmpMeasurementErrorStd);
  }

  /////////////////////////////////
  /// Run the estimator
  /////////////////////////////////
  std::vector<double> dcm_hat(signallength), bias_hat(signallength);

  double initbias = 0;

  UnidimLipmDcmEstimator est(dt, w0);

  est.resetWithInputs(dcm_m[0], zmp_m[0], biasDriftPerSecondStd, dcmMeasurementErrorStd, zmpMeasurementErrorStd,
                      initbias, initBiasstd);

  IndexedVectorArray log;

  for(int i = 1; i < signallength; i++)
  {
    est.setInputs(dcm_m[i], zmp_m[i]);
    est.update();
    bias_hat[i] = est.getBias();
    dcm_hat[i] = est.getUnbiasedDCM();

    /// NaN detection
    if(dcm_hat[i] != dcm_hat[i])
    {
      return errorCode;
    }

    Vector6 log_k;

    log_k << i, bias_hat[i], bias[i], dcm_hat[i], dcm_m_unbiased[i], dcm[i];
    log.pushBack(log_k);
  }

  /// uncomment the following line to have a bit of a log
  // log.writeInFile("dcm.txt");

  /////////////////////////////////
  /// Check the rror
  /////////////////////////////////
  double error = 0;

  for(int i = signallength - 10; i < signallength; ++i)
  {
    error += fabs(dcm_hat[i] - dcm[i]) + fabs(bias_hat[i] - bias[i]);
  }

  std::cout << "Sum of error on the 10 last samples = " << error << std::endl;

  if(error > 0.07)
  {
    return errorCode;
  }
  else
  {
    return 0;
  }
}

/// @brief runs the basic test
///
/// @return int : 0 if success, nonzero if fails
int testDcmBiasEstimator(int errorCode)
{
  double w0 = sqrt(cst::gravityConstant / 0.9);
  double dt = 0.005;

  double biasDriftPerSecondStd = 0.005;
  double zmpMeasurementErrorStd = 0.001;
  double dcmMeasurementErrorStd = 0.005;

  int signallength = int(120. / dt);

  ///////////////////////////////////////
  /// Build the ground truth signals
  ///////////////////////////////////////
  tools::ProbabilityLawSimulation ran;
  IndexedVectorArray dcm(signallength), localBias(signallength), bias(signallength), zmp(signallength);
  std::vector<double> yaw(signallength);

  /// set the desired exponential convergence of the DCM
  double lambda = 2;

  /// initialize the dcm and localBias to a random value
  dcm[0] = ran.getGaussianVector(Matrix2::Identity() * 2, Vector2::Zero(), 2);
  double initBiasstd = 0.01;

  localBias[0] = ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * 0.01;
  Vector2 deviation = ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * 0.05;
  zmp[0] = (lambda / w0 + 1) * dcm[0] + deviation;

  for(int i = 0; i < signallength - 1; ++i)
  {
    /// dcm dynamics
    dcm[i + 1] = dcm[i] + dt * w0 * (dcm[i] - zmp[i]);
    /// local bias drift
    localBias[i + 1] =
        localBias[i] + ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * biasDriftPerSecondStd * dt;
    /// set a noisy zmp to create  bounded drift of the DCM
    deviation += ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * 0.05;
    zmp[i + 1] =
        (lambda / w0 + 1) * dcm[i] + ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * 0.05 + deviation;
  }

  for(int i = 0; i < signallength; ++i)
  {
    /// global-frame bias computation
    yaw[i] = ran.getGaussianScalar(2 * M_PI);
    bias[i] = Rotation2D(yaw[i]) * localBias[i];
  }

  /////////////////////////////////
  /// Build the measurements
  /////////////////////////////////
  IndexedVectorArray dcm_m_unbiased(signallength), dcm_m(signallength), zmp_m(signallength);

  for(int i = 0; i < signallength; ++i)
  {

    dcm_m_unbiased[i] =
        dcm[i] + ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * dcmMeasurementErrorStd;
    dcm_m[i] = dcm_m_unbiased[i] + bias[i];
    zmp_m[i] = zmp[i] + ran.getGaussianVector(Matrix2::Identity(), Vector2::Zero(), 2) * zmpMeasurementErrorStd;
  }

  /////////////////////////////////
  /// Run the estimator
  /////////////////////////////////
  IndexedVectorArray dcm_hat(signallength), bias_hat(signallength);

  Vector2 initbias{0.0, 0.0};

  LipmDcmEstimator est(dt, w0, biasDriftPerSecondStd, dcmMeasurementErrorStd, zmpMeasurementErrorStd);

  est.resetWithMeasurements(dcm_m[0], zmp_m[0], yaw[0], true, initbias, Vector2::Constant(initBiasstd));

  IndexedVectorArray log;

  for(int i = 1; i < signallength; i++)
  {
    est.setInputs(dcm_m[i], zmp_m[i], yaw[i]);
    est.update();
    bias_hat[i] = est.getBias();
    dcm_hat[i] = est.getUnbiasedDCM();

    /// NaN detection
    if(dcm_hat[i] != dcm_hat[i])
    {
      std::cout << "NaN detected = " << i << " " << dcm_hat[i].transpose() << std::endl;
      return errorCode;
    }

    Vector log_k(11);

    log_k << i, bias_hat[i], localBias[i], dcm_hat[i], dcm_m_unbiased[i], dcm[i];
    log.pushBack(log_k);
  }

  /// uncomment the following line to have a bit of a log
  // log.writeInFile("dcm.txt");

  /////////////////////////////////
  /// Check the error
  /////////////////////////////////
  double error = 0;

  for(int i = signallength - 10; i < signallength; ++i)
  {
    error += (dcm_hat[i] - dcm[i]).norm() + (bias_hat[i] - bias[i]).norm();
  }

  std::cout << "Sum of error on the 10 last samples = " << error << std::endl;

  if(error > 0.07)
  {
    return errorCode;
  }
  else
  {
    return 0;
  }
}

/// @brief test rotationMatrix2Angle
///
/// @param errorCode
/// @return int
int testrotationMatrix2Angle(int errorCode)
{

  {
    Vector3 axis = Vector3::Random().normalized();
    Vector3 v = Vector3::Random().cross(axis).normalized();
    double angle = -1.9745; /// random value
    Matrix3 m = (AngleAxis(angle, axis)).matrix() * AngleAxis(-0.546, v).matrix();

    double error = fabs(angle - kine::rotationMatrixToAngle(m, axis, v));
    std::cout << "Angle error " << error << std::endl;

    if(error > cst::epsilon1)
    {
      return errorCode;
    }
  }
  {
    double angle = 2.6845; /// random value

    Vector2 v = Vector2::Random().normalized();
    Vector3 v3;
    v3 << v, 0;
    Matrix3 m = AngleAxis(angle, Vector3::UnitZ()).matrix() * AngleAxis(-1.245, v3).matrix();

    double error = fabs(angle - kine::rotationMatrixToYaw(m, v));

    std::cout << "Angle error " << error << std::endl;

    if(error > cst::epsilon1)
    {
      return errorCode;
    }
  }
  {
    double angle = 2.6845; /// random value

    Vector2 v = Vector2::UnitX();
    Vector3 v3;
    v3 << v, 0;
    Matrix3 m = AngleAxis(angle, Vector3::UnitZ()).matrix() * AngleAxis(-0.689, Vector3::UnitY()).matrix()
                * AngleAxis(-1.245, v3).matrix();

    double error = fabs(angle - kine::rotationMatrixToYaw(m));

    std::cout << "Angle error " << error << std::endl;

    if(error > cst::epsilon1)
    {
      return errorCode;
    }
  }
  {
    double angle = 1.4587; /// random value

    Vector2 v = Vector2::Random().normalized();
    Vector3 v3;
    v3 << v, 0;

    Matrix3 m = AngleAxis(angle, Vector3::UnitZ()).matrix() * AngleAxis(3.54, v3).matrix();

    double error = fabs(angle - kine::rotationMatrixToYawAxisAgnostic(m));

    std::cout << "Angle error " << error << std::endl;

    if(error > cst::epsilon1)
    {
      return errorCode;
    }
  }
  return 0;
}

int main()
{
  int exitCode;

  exitCode = testrotationMatrix2Angle(1);

  if(exitCode != 0)
  {
    std::cout << "Failed, testrotationMatrix2Angle error code: " << exitCode << std::endl;
    return exitCode;
  }

  exitCode = testUnidimDcmBiasEstimator(2);

  if(exitCode != 0)
  {
    std::cout << "Failed, testUnidimDcmBiasEstimator error code: " << exitCode << std::endl;
    return exitCode;
  }

  exitCode = testDcmBiasEstimator(3);
  if(exitCode != 0)
  {
    std::cout << "Failed, testDcmBiasEstimator error code: " << exitCode << std::endl;
    return exitCode;
  }

  std::cout << "Succeeded" << std::endl;
  return exitCode;
}