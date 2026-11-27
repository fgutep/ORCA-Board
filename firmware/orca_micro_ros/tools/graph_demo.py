#!/usr/bin/env python3
"""Nodos de ejemplo del PC alrededor de /orca_board, para ver el sistema completo en rqt_graph.

Crean los publicadores y suscriptores del grafo objetivo (presentación, 06_grafo_ros2), pero NUNCA
publican: no mueven motores ni servos. Sirven para rqt_graph y como plantilla de nombres y QoS.

    source /opt/ros/jazzy/setup.bash        # y can_msgs (ver README) para el puente CAN
    python3 tools/graph_demo.py             # Ctrl-C para salir
"""
import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.qos import qos_profile_sensor_data
from diagnostic_msgs.msg import DiagnosticArray
from geometry_msgs.msg import Twist
from sensor_msgs.msg import BatteryState, Imu
from std_msgs.msg import Bool, Float32MultiArray

try:
    from can_msgs.msg import Frame
except ImportError:
    Frame = None


def make(name, pubs=(), subs=()):
    node = rclpy.create_node(name)
    for topic, msg_type in pubs:
        node.create_publisher(msg_type, topic, 10)
    for topic, msg_type, qos in subs:
        node.create_subscription(msg_type, topic, lambda _msg: None, qos)
    return node


def main():
    rclpy.init()
    nodes = [
        make('teleop', pubs=[('cmd_vel', Twist)]),
        make('arm_controller', pubs=[('servo_cmd', Float32MultiArray)]),
        make('ekf_filter_node', subs=[('imu', Imu, qos_profile_sensor_data)]),  # /imu es best effort
        make('battery_monitor', subs=[('battery_state', BatteryState, 10)]),
        make('safety_supervisor', pubs=[('orca/estop', Bool)], subs=[('diagnostics', DiagnosticArray, 10)]),
    ]
    if Frame is not None:
        nodes.append(make('can_bridge', pubs=[('can_tx', Frame)], subs=[('can_rx', Frame, 10)]))
    else:
        print('can_msgs no está instalado: se omite /can_bridge (sudo apt install ros-jazzy-can-msgs)')
    executor = SingleThreadedExecutor()
    for node in nodes:
        executor.add_node(node)
    print('Nodos de ejemplo activos: ' + ', '.join(n.get_name() for n in nodes) + ' (Ctrl-C para salir)')
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
