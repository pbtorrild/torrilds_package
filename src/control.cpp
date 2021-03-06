#include <ros/ros.h>

#include "geometry_msgs/Twist.h"
#include "sensor_msgs/LaserScan.h"


#include <rob4_pkg/ClosestObj.h>
#include <rob4_pkg/EmergStop.h>
#include <rob4_pkg/LineDist.h>
#include <rob4_pkg/SignsFound.h>
#include <rob4_pkg/RoadObj.h>
class control_data{
private:
  //distance in pixel to line
  float desired_pix_dist=320;
  double angular_vel;
  // vel
  float vel_30=0.01;
  float std_vel =0.03;
  float vel_50 = 0.03;
  float vel_70 =0.05;
  float speed_up_vel=0.05;
  //left/right information
  double road_hight=0.2;//road hight is 20 cm
  double look_left=1.; //it will look within one meter to the left
  double look_right=1.; // it will look within  one meter to the right
  double left_right_dist=0.4; // will not go ahead in the case of an object beeing within this th
protected:
  //emergcy stop status
  bool emerg_stop=true;
  bool emerg_speed_up=false;
  //sign input
  bool MainSideRoad=false;
  bool Yield=false;
  bool Kids=false;
  bool DontGoLeft=false;
  bool Seventy=false;
  bool Thirty=false;
  bool Fifty=false;
  bool BothWaysNo=false;
  bool Cross=false;
  //humans and car detection
  bool human=false;
  bool car=false;
  //change of road
  float line_dist_px=0;
  //crossing safety
  bool safe_left_turn=false;
  bool safe_right_turn=false;
public:
  //funcktions below
  double get_angular_vel(double distance_in){
    //computiation
    double dist_2l_error=distance_in-desired_pix_dist;
    ROS_INFO("Error: %f",dist_2l_error);

      //we determine k by taking the maximum wanted angular vel and diviting it by the maximum error
      float k= 0.8*0.003125; //A_vel_max/e_max , A_vel_max=0.5 & e_max=320
      if(dist_2l_error!=-320){
        angular_vel=-dist_2l_error*k;
      }
    return angular_vel;
  }
  void cmd_vel(ros::NodeHandle nh,ros::Publisher pub){
    //check for changes in emerg_stop status
    geometry_msgs::Twist send_data;
    //control funcktions
    //fist check for emergency stop
    if (emerg_stop!=true) {
      send_data.linear.x=std_vel; // go with a std vel of 50 km/h

      //sign to determine speed
      if (Seventy==true) {
        send_data.linear.x=vel_70;
      }
      if (Fifty==true) {
        send_data.linear.x=vel_50;
      }
      if (Thirty==true) {
        send_data.linear.x=vel_30;
      }
      if (emerg_speed_up==true) {
        send_data.linear.x=speed_up_vel;
      }
      //////////////////////////////
      //Do the standard angular change
      send_data.angular.z=get_angular_vel(line_dist_px);

      //////////////////////////////
      // Special cases of change in vel and angle

      //perfon a stop if there is a human at a crossing
      if (human==true && Cross==true) {
        send_data.linear.x=0;
        send_data.angular.z=get_angular_vel(line_dist_px);
      }
      if (safe_left_turn!=true && safe_right_turn!=true && Yield==true) {
        send_data.linear.x=0;
        send_data.angular.z=0;
      }

    }
    else{
      send_data.linear.x=0;
      send_data.angular.z=0;
    }
    pub.publish(send_data);
  }
  void end_of_alg(ros::NodeHandle nh,ros::Publisher pub){

    geometry_msgs::Twist send_data;

    send_data.linear.x=0;
    send_data.angular.z=0;

    pub.publish(send_data);
    ros::spinOnce();
  }
  //callback:
  void callback_signs_found(const rob4_pkg::SignsFound::ConstPtr& received_data){
    MainSideRoad=received_data->MainSideRoad;
    Yield=received_data->Yield;
    Kids=received_data->Kids;
    DontGoLeft=received_data->DontGoLeft;
    Seventy=received_data->Seventy;
    Thirty=received_data->Thirty;
    Fifty=received_data->Fifty;
    BothWaysNo=received_data->BothWaysNo;
    Cross=received_data->Cross;

  }
  void callback_emerg_stop(const rob4_pkg::EmergStop::ConstPtr& received_data){
    emerg_stop=received_data->emerg_stop;
    emerg_speed_up=received_data->emerg_speed_up;
  }
  void callback_road_change(const rob4_pkg::LineDist::ConstPtr& received_data){
    line_dist_px=received_data->line_dist;
  }
  void callback_lidar_data(const sensor_msgs::LaserScan::ConstPtr& msg){
    //extracting information on the sensor message
    double scan_time = msg->scan_time;
    double range_max = msg->range_max;
    double range_min = msg->range_min;
    double angle_increment = msg->angle_increment;
    double angle_min = msg->angle_min;
    double ranges_size = msg->ranges.size(); // function in "std_msgs/Float32MultiArray.h"

    //finding coordinates of the input data
    double Range;
    double Angle;
    double closest_obj_left=range_max;
    double closest_obj_right=range_max;
    for (size_t i = 0; i <= ranges_size; i++){
      //Polar coordinates
      Range = msg->ranges[i];
      Angle = (angle_min + (i* angle_increment));

      double x = Range*cos(Angle);
      double y = Range*sin(Angle);

      //Find closest opbject infront of turtlebot
      if(Range>range_min&& x>road_hight && x<0){
        if (Range<=closest_obj_left &&y>0) {
          closest_obj_left=Range;
        }
        if (Range<=closest_obj_right &&y<0) {
          closest_obj_right=Range;
        }
      }
    }
    if (closest_obj_left<=left_right_dist) {
      safe_left_turn=false;
    }
    else{safe_left_turn=false;}

    if (closest_obj_right<=left_right_dist) {
      safe_right_turn=false;
    }
    else{safe_right_turn=false;}
  }
  void callback_road_obj(const rob4_pkg::RoadObj::ConstPtr& received_data) {
    human=received_data->human_found;
    car=received_data->car_found;
  }
};

int main(int argc, char**argv){
  //standart ros initialize
  ros::init(argc, argv, "vel_controller");
  ros::NodeHandle nh;
  //define publisher
  ros::Publisher pub = nh.advertise<geometry_msgs::Twist>("cmd_vel", 1);
  //set class member
  control_data monitor;
  //subscibers
  ros::Subscriber sub = nh.subscribe("scan", 1,&control_data::callback_lidar_data,&monitor);
  ros::Subscriber sub_signs = nh.subscribe("signs_found",1,&control_data::callback_signs_found,&monitor);
  ros::Subscriber sub_emerg_stop = nh.subscribe("emerg_stop_status",1,&control_data::callback_emerg_stop,&monitor);
  ros::Subscriber sub_road_change = nh.subscribe("line_dist",1,&control_data::callback_road_change,&monitor);
  ros::Subscriber sub_road_obj = nh.subscribe("road_obj",1,&control_data::callback_road_obj,&monitor);
  //initialise the node with an emerg_stop
  ros::Rate rate(15.);
  while (ros::ok()) {
    monitor.cmd_vel(nh,pub);
    rate.sleep();
    ros::spinOnce();
  }
  monitor.end_of_alg(nh,pub);
}
