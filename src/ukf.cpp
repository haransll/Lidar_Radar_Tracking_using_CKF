#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
    
  is_initialized_ = false;
    
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = false;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.5;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
    
    time_us_ = 0;
    
    n_aug_ = 7;

    weights_ = VectorXd(2*n_aug_+1);
    
    weights_cub_ = VectorXd(2*n_aug_);
    
    n_x_ = 5;
    
    Xcub_pred_ = MatrixXd(n_x_,2*n_aug_);
    
    // Define spreading parameter
    lambda_ = 0;
    
    // Matrix to hold sigma points
    Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_+1 );


    // Set NIS
    NIS_radar_ = 0;
    NIS_laser_ = 0;
    
    
}

UKF::~UKF() {}


/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
    
    /**
     TODO:
     
     Complete the initialization. See ukf.h for other member properties.
     
     Hint: one or more values initialized above might be wildly off...
     */
    /*****************************************************************************
     *  Initialization
     ****************************************************************************/
    if (!is_initialized_) {
        
        if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
            double rho = meas_package.raw_measurements_(0);
            double phi = meas_package.raw_measurements_(1);
            double rhodot = meas_package.raw_measurements_(2);

            // polar to cartesian - r * cos(angle) for x and r * sin(angle) for y
            // ***** Middle value for 'v' can be tuned *****
            x_ << rho * cos(phi), rho * sin(phi), 4, rhodot * cos(phi), rhodot * sin(phi);

            //state covariance matrix
            //***** values can be tuned *****
            P_ << std_radr_*std_radr_, 0, 0, 0, 0,
            0, std_radr_*std_radr_, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 0, std_radphi_, 0,
            0, 0, 0, 0, std_radphi_;


        }
        else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
            // Initialize state.
            // ***** Last three values below can be tuned *****
            x_ << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1), 4, 0.5, 0.0;

            //state covariance matrix
            //***** values can be tuned *****
            P_ << std_laspx_*std_laspx_, 0, 0, 0, 0,
            0, std_laspy_*std_laspy_, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 0, 1, 0,
            0, 0, 0, 0, 1;


        }
        


        // done initializing, no need to predict or update
        is_initialized_ = true;
        time_us_= meas_package.timestamp_;
        return;
    }
    
    

    
 
    // Calculate delta_t, store current time for future
    double dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
    time_us_ = meas_package.timestamp_;
    //predict
 
    cout<<"dt "<<dt<<endl;
    // dt = 0.05;
    Prediction(dt);
    
    
    // Measurement updates
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
        UpdateRadar(meas_package);
    } else {
        UpdateLidar(meas_package);
    }
 
}

void UKF::Prediction(double delta_t) {
    
    
    //calculate square root of P
    MatrixXd A_ = P_.llt().matrixL();
    
    
    // Define spreading parameter for augmentation
    //lambda_ = 3 - n_aug_;
    lambda_ = 0;
    
    
    //create augmented mean vector
    VectorXd x_aug_ = VectorXd(7);
    
    //create augmented state covariance
    MatrixXd P_aug_ = MatrixXd(7, 7);
    
    //create sigma point matrix
    MatrixXd Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);
    
    //create augmented mean state
    x_aug_.head(5) = x_;
    x_aug_(5) = 0;
    x_aug_(6) = 0;
    
    //create augmented covariance matrix
    P_aug_.fill(0.0);
    P_aug_.topLeftCorner(5,5) = P_;
    P_aug_(5,5) =  std_a_*std_a_;
    P_aug_(6,6) = std_yawdd_*std_yawdd_;
    
    //create square root matrix
    MatrixXd A_aug = P_aug_.llt().matrixL();
    
    //create augmented sigma points
    Xsig_aug_.col(0) = x_aug_;
    for(int i = 0; i < n_aug_; i++) {
        Xsig_aug_.col(i+1) = x_aug_ + std::sqrt(lambda_+n_aug_)*A_aug.col(i);
        Xsig_aug_.col(i+1+n_aug_) = x_aug_ - std::sqrt(lambda_+n_aug_)*A_aug.col(i);
    }
    
    //predict sigma points
    //set vectors for each part added to x
    
    
    for(int i = 0; i < 2 * n_aug_ + 1; i++) {
        VectorXd calc_col = Xsig_aug_.col(i);
        double px = calc_col(0);
        double py = calc_col(1);
        double v = calc_col(2);
        double yaw = calc_col(3);
        double yawd = calc_col(4);
        double v_aug = calc_col(5);
        double v_yawdd = calc_col(6);
        
        //original
        VectorXd orig = calc_col.head(5);
        
        
        double px_p,py_p;
        //avoid division by zero
        if (fabs(yawd) > 0.001) {
            px_p = px + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
            py_p = py + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
        }
        else {
            px_p = px + v*delta_t*cos(yaw);
            py_p = py + v*delta_t*sin(yaw);
        }
        
        double v_p = v;
        double yaw_p =  yaw + yawd*delta_t;
        double yawd_p = yawd;
        
        //add noise
        px_p = px_p + 0.5*v_aug*delta_t*delta_t * cos(yaw);
        py_p = py_p + 0.5*v_aug*delta_t*delta_t * sin(yaw);
        v_p = v_p + v_aug*delta_t;
        yaw_p = yaw_p + 0.5*v_yawdd*delta_t*delta_t;
        yawd_p = yawd_p + v_yawdd*delta_t;
        
        cout<< "v_p  "<<v_p<<endl;
        
        //write predicted sigma point into right column
        Xsig_pred_(0,i) = px_p;
        Xsig_pred_(1,i) = py_p;
        Xsig_pred_(2,i) = v_p;
        Xsig_pred_(3,i) = yaw_p;
        Xsig_pred_(4,i) = yawd_p;
        
        
    }
    
    
    //create vector for predicted state
    VectorXd x_pred = VectorXd(n_x_);
    
    //create covariance matrix for prediction
    MatrixXd P_pred = MatrixXd(n_x_, n_x_);
    
    x_pred.fill(0.0);
    P_pred.fill(0.0);
    
    for(int i = 0; i < 2 * n_aug_ + 1; i++) {
        
        //set weights
        if (i == 0) {
            weights_(i) = lambda_ / (lambda_ + n_aug_);
        } else {
            weights_(i) = .5 / (lambda_ + n_aug_);
        }
        
        //predict state mean
        x_pred += weights_(i) * Xsig_pred_.col(i);
    }
    
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {
        
        //predict state covariance matrix
        VectorXd x_diff = Xsig_pred_.col(i) - x_pred;
        
        //normalize angles
        if (x_diff(3) > M_PI) {
            x_diff(3) -= 2. * M_PI;
        } else if (x_diff(3) < -M_PI) {
            x_diff(3) += 2. * M_PI;
        }
        P_pred += weights_(i) * x_diff * x_diff.transpose();
    }
    
    x_ = x_pred;
    P_ = P_pred;
    
    
}




/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
    
    VectorXd z = meas_package.raw_measurements_;
    cout<<"z "<<z<<endl;
    
    MatrixXd H_ = MatrixXd(2,5);
    H_.fill(0.0);
    H_(0,0) = 1;
    H_(1,1) = 1;
    
    MatrixXd R_ = MatrixXd(2,2);
    R_.fill(0.0);
    R_(0,0) = std_laspx_*std_laspx_;
    R_(1,1) = std_laspy_*std_laspy_;
    
    
    VectorXd z_pred = H_ * x_;
    VectorXd y = z - z_pred;
    MatrixXd Ht = H_.transpose();
    MatrixXd S = H_ * P_ * Ht + R_;
    MatrixXd Si = S.inverse();
    MatrixXd PHt = P_ * Ht;
    MatrixXd K = PHt * Si;
    
    //new estimate
    x_ = x_ + (K * y);
    long x_size = x_.size();
    MatrixXd I = MatrixXd::Identity(x_size, x_size);
    P_ = (I - K * H_) * P_;
    
    
    // residual
    VectorXd z_diff = z - z_pred;
    
    //calculate NIS
    NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;
    
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
    //set measurement dimension, radar can measure r, phi, and r_dot
    int n_z = 3;
    
    //create matrix for sigma points in measurement space
    MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
    
    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z);
    
    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z,n_z);
    
    Zsig.fill(0.0);
    z_pred.fill(0.0);
    S.fill(0.0);
    double rho = 0;
    double phi = 0;
    double rho_d = 0;
    
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {
        //transform sigma points into measurement space
        VectorXd state_vec = Xsig_pred_.col(i);
        double px = state_vec(0);
        double py = state_vec(1);
        double v = state_vec(2);
        double yaw = state_vec(3);
        double yaw_d = state_vec(4);
        
        rho = sqrt(px*px+py*py);
        phi = atan2(py,px);
        rho_d = (px*cos(yaw)*v+py*sin(yaw)*v) / rho;
        
        Zsig.col(i) << rho,
        phi,
        rho_d;
        
        //calculate mean predicted measurement
        z_pred += weights_(i) * Zsig.col(i);
    }
    
    //calculate measurement covariance matrix S
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {
        VectorXd z_diff = Zsig.col(i) - z_pred;
        if (z_diff(1) > M_PI) {
            z_diff(1) -= 2. * M_PI;
        } else if (z_diff(1) < - M_PI) {
            z_diff(1) += 2. * M_PI;
        }
        S += weights_(i) * z_diff * z_diff.transpose();
    }
    
    // Add R (noise) to S

    // Create R for update noise later
    MatrixXd R_radar = MatrixXd(3,3);
    
    R_radar << std_radr_*std_radr_, 0, 0,
    0, std_radphi_*std_radphi_, 0,
    0, 0, std_radrd_*std_radrd_;
    
    
    S += R_radar;
    
    //create example vector for incoming radar measurement
    VectorXd z = VectorXd(n_z);
    
    double meas_rho = meas_package.raw_measurements_(0);
    double meas_phi = meas_package.raw_measurements_(1);
    double meas_rhod = meas_package.raw_measurements_(2);
    
    z << meas_rho,
    meas_phi,
    meas_rhod;
    
    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z);
    Tc.fill(0.0);
    
    //calculate cross correlation matrix
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //normalize angles
        if (x_diff(3) > M_PI) {
            x_diff(3) -= 2. * M_PI;
        } else if (x_diff(3) < -M_PI) {
            x_diff(3) += 2. * M_PI;
        }
        VectorXd z_diff = Zsig.col(i) - z_pred;
        //normalize angles
        if (z_diff(1) > M_PI) {
            z_diff(1) -= 2. * M_PI;
        } else if (z_diff(1) < -M_PI) {
            z_diff(1) += 2. * M_PI;
        }
        Tc += weights_(i) * x_diff * z_diff.transpose();
        
    }
    
    // residual
    VectorXd z_diff = z - z_pred;
    
    //normalize angles
    if (z_diff(1) > M_PI) {
        z_diff(1) -= 2. * M_PI;
    } else if (z_diff(1) < -M_PI) {
        z_diff(1) += 2. * M_PI;
    }
    
    //calculate NIS
    NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;
    
    //calculate Kalman gain K;
    MatrixXd K = Tc * S.inverse();
    
    //update state mean and covariance matrix
    x_ += K*z_diff;
    P_ -= K*S*K.transpose();
    
}
