#!/usr/bin/env python3
"""Exporta el grafo ROS 2 tal como lo dibuja rqt_graph, sin abrir ventanas.

Usa el mismo código que el botón "Save as image" de rqt_graph (RosGraphDotcodeGenerator + DotToQtGenerator
sobre una QGraphicsScene), con Qt en modo offscreen. Escribe <salida>.png, <salida>.svg y <salida>.dot.

    source /opt/ros/jazzy/setup.bash
    python3 tools/rqt_graph_export.py docs/img/rqt_graph_sistema --mode active
    python3 tools/rqt_graph_export.py docs/img/rqt_graph_orca_board --mode all --ns-filter /orca_board

Modos (los de la lista desplegable de rqt_graph): nodes = "Nodes only", active = "Nodes/Topics (active)",
all = "Nodes/Topics (all)".
"""
import argparse
import os
import sys
import time

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')

import rclpy  # noqa: E402
from python_qt_binding.QtCore import Qt  # noqa: E402
from python_qt_binding.QtGui import QImage, QPainter  # noqa: E402
from python_qt_binding.QtSvg import QSvgGenerator  # noqa: E402
from python_qt_binding.QtWidgets import QApplication, QGraphicsScene  # noqa: E402
from qt_dotgraph.dot_to_qt import DotToQtGenerator  # noqa: E402
from qt_dotgraph.pydotfactory import PydotFactory  # noqa: E402
from rqt_graph.dotcode import (NODE_NODE_GRAPH, NODE_TOPIC_ALL_GRAPH, NODE_TOPIC_GRAPH,  # noqa: E402
                               RosGraphDotcodeGenerator)
from rqt_graph.rosgraph2_impl import Graph  # noqa: E402

MODES = {'nodes': NODE_NODE_GRAPH, 'active': NODE_TOPIC_GRAPH, 'all': NODE_TOPIC_ALL_GRAPH}
SELF = '_rqt_graph_export'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out', help='ruta de salida sin extensión')
    ap.add_argument('--mode', choices=MODES, default='active')
    ap.add_argument('--ns-filter', default='/', help='filtro de nodos de rqt_graph (p. ej. /orca_board)')
    ap.add_argument('--topic-filter', default='/', help='filtro de tópicos de rqt_graph')
    ap.add_argument('--hide-dead-sinks', action='store_true', help='ocultar tópicos sin suscriptores')
    ap.add_argument('--hide-leaf-topics', action='store_true', help='ocultar tópicos con una sola conexión')
    ap.add_argument('--wait', type=float, default=4.0, help='segundos de descubrimiento DDS antes de exportar')
    ap.add_argument('--scale', type=float, default=2.5, help='escala del PNG respecto al tamaño de la escena')
    a = ap.parse_args()

    rclpy.init()
    node = rclpy.create_node(SELF)  # el guion bajo lo oculta del grafo
    t_end = time.time() + a.wait
    while time.time() < t_end:
        rclpy.spin_once(node, timeout_sec=0.1)

    graph = Graph(node)
    graph.set_node_stale(5.0)
    graph.update()
    dotcode = RosGraphDotcodeGenerator(node).generate_dotcode(
        rosgraphinst=graph, ns_filter=a.ns_filter, topic_filter=a.topic_filter, graph_mode=MODES[a.mode],
        hide_single_connection_topics=a.hide_leaf_topics, hide_dead_end_topics=a.hide_dead_sinks,
        cluster_namespaces_level=0, accumulate_actions=True, dotcode_factory=PydotFactory(), orientation='LR',
        quiet=True, unreachable=False, group_tf_nodes=True, hide_tf_nodes=False, group_image_nodes=True,
        hide_dynamic_reconfigure=True)

    app = QApplication(sys.argv)  # noqa: F841 (necesaria para crear items Qt)
    scene = QGraphicsScene()
    DotToQtGenerator().dotcode_to_qt_items(dotcode, highlight_level=1, same_label_siblings=True, scene=scene)
    rect = scene.itemsBoundingRect().adjusted(-24, -24, 24, 24)
    scene.setSceneRect(rect)

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out + '.dot', 'w') as f:
        f.write(dotcode)
    img = QImage((rect.size() * a.scale).toSize(), QImage.Format_ARGB32_Premultiplied)
    img.fill(Qt.white)
    painter = QPainter(img)
    painter.setRenderHint(QPainter.Antialiasing)
    painter.setRenderHint(QPainter.TextAntialiasing)
    scene.render(painter)
    painter.end()
    img.save(a.out + '.png')
    svg = QSvgGenerator()
    svg.setFileName(a.out + '.svg')
    svg.setSize(rect.size().toSize())
    svg.setViewBox(rect)
    painter = QPainter(svg)
    painter.setRenderHint(QPainter.Antialiasing)
    scene.render(painter)
    painter.end()
    print('%s.{png,svg,dot}: %d x %d px' % (a.out, img.width(), img.height()))
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
