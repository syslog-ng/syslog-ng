#!/usr/bin/env python
import json, sys

j = None

if len(sys.argv) == 1:
    j = json.loads(sys.stdin.read())
else:
    file_name = sys.argv[1]
    with open(file_name) as f:
        j = json.loads(f.read())

nodes = j["nodes"]
arcs = j["arcs"]

print("digraph D {")
for node in nodes:
    print("  Node{} [label=\"{} {}\"]".format(node["node"], node["node"], ", ".join(node["info"])))
for arc in arcs:
    print("  Node{} -> Node{} [label=\"{}\"]".format(arc["from"], arc["to"], arc["type"]))
print("}")
