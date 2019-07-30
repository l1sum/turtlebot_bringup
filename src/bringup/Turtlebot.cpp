#include <iostream>
#include <cmath>

#include <Turtlebot.hpp>

#include <sensor_msgs/msg/imu.hpp>

using namespace std;


// ホイールが地面から離れたことを検知
void Turtlebot::checkWheelDrop(){
    if (kobuki->isRightWheelDrop() || kobuki->isLeftWheelDrop()) {
        delete kobuki;
        cout << "[!] Error : Wheel Drop" << endl;
        abort();
    }
}


// オイラー角からクオータニオンへ変換
geometry_msgs::msg::Quaternion Turtlebot::translateCoordinate(double x, double y, double z){
    auto quaternion = geometry_msgs::msg::Quaternion();
    quaternion.w = cos(x/2)*cos(y/2)*cos(z/2) + sin(x/2)*sin(y/2)*sin(z/2);
    quaternion.x = sin(x/2)*cos(y/2)*cos(z/2) - cos(x/2)*sin(y/2)*sin(z/2);
    quaternion.y = cos(x/2)*sin(y/2)*cos(z/2) + sin(x/2)*cos(y/2)*sin(z/2);
    quaternion.z = cos(x/2)*cos(y/2)*sin(z/2) - sin(x/2)*sin(y/2)*cos(z/2);

    return quaternion;
}


// 速度制御
void Turtlebot::controleByVelocity(geometry_msgs::msg::Twist::SharedPtr msg) {
    checkWheelDrop();

    if (msg->linear.z > 110.0) {
        RCLCPP_INFO(this->get_logger(), "OVER 110.0 [rad/s]");
        target_angular_velocity = 110.0;
    } else {
        target_angular_velocity = msg->linear.z;
    }

    system_angular_velocity = pid->system(target_angular_velocity, N_linear_z_velocity);
    kobuki->setTargetVelocity(msg->linear.x, system_angular_velocity);
}


// オドメトリのブロードキャスト
void Turtlebot::publishOdometry() {
    checkWheelDrop();
    auto odom_msg = nav_msgs::msg::Odometry();

    kobuki->getPose(&N_position_x, &N_position_y, &N_orientation_theta);

    // calculate delata time
    now_time = chrono::system_clock::now();
    auto delta_seconds = chrono::duration_cast<chrono::seconds>(now_time - base_time);
    auto delta_milliseconds = chrono::duration_cast<chrono::milliseconds>(now_time - base_time);

    millisec = delta_milliseconds.count() - delta_seconds.count()*1000;

    odom_msg.child_frame_id = "base_footprint";
    odom_msg.header.frame_id = "odom";
    odom_msg.header.stamp.sec = delta_seconds.count();
    odom_msg.header.stamp.nanosec = millisec;
    odom_msg.pose.pose.position.x = N_position_x;
    odom_msg.pose.pose.position.y = N_position_y;
    odom_msg.pose.pose.orientation = translateCoordinate(0.0, 0.0, N_orientation_theta);

    N_linear_x_velocity = calculateVelocity(N_position_x, O_position_x, 0.01);
    N_linear_y_velocity = calculateVelocity(N_position_y, O_position_y, 0.01);
    N_linear_z_velocity = calculateVelocity(N_orientation_theta, O_orientation_theta, 0.01);

    odom_msg.twist.twist.linear.x = N_linear_x_velocity;
    odom_msg.twist.twist.linear.y = N_linear_y_velocity;

    odom_msg.twist.twist.angular.z = N_linear_z_velocity;

    O_position_x = N_position_x;
    O_position_y = N_position_y;
    O_orientation_theta = N_orientation_theta;

    odom->publish(odom_msg);
}


// 速度計算
double Turtlebot::calculateVelocity(double N_position, double O_position, float time){
    return (O_position - N_position)/time;
}


// 回転慣性値のブロードキャスト
void Turtlebot::publishInertial() {
    auto imu_msg = sensor_msgs::msg::Imu();

    now_time = chrono::system_clock::now();
    auto delta_seconds = chrono::duration_cast<chrono::seconds>(now_time - base_time);
    auto delta_milliseconds = chrono::duration_cast<chrono::milliseconds>(now_time - base_time);

    millisec = delta_milliseconds.count() - delta_seconds.count()*1000;

    imu_msg.header.frame_id      = "imu";
    imu_msg.header.stamp.sec     = delta_seconds.count();
    imu_msg.header.stamp.nanosec = millisec;

    imu_msg.orientation = translateCoordinate(0, 0, kobuki->getInertialAngle());

    imu_msg.angular_velocity.x = 0.0;
    imu_msg.angular_velocity.y = 0.0;

    double InertialAngleRate = kobuki->getInertialAngleRate()/60000;
    bool flag = (int)InertialAngleRate;

    //if ( flag ) {
    //  cout << InertialAngleRate << endl;
    //} else {
    //  cout << -InertialAngleRate << endl;
    //}

    imu_msg.angular_velocity.z = kobuki->getInertialAngleRate();

    inertial->publish(imu_msg);
}