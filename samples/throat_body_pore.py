from threading import Thread
import time
import numpy as np
from espressomd import System, visualization, lb, lbboundaries, constraints, shapes

width = 4
radius = 8
n_pores = 3

length = 3*radius

box_l = np.array([n_pores*length,2*radius+2,5])
system = System(box_l = box_l)
system.cell_system.skin = 0.1

center = [length/2., radius+1, 0]
pores = []
for i in range(n_pores):
    pore = shapes.ThroatBodyPore(radius = radius, center = center, width = width, length = length)
    pores.append(pore)
    center[0]+=length
    
lbf = lb.LBFluid(tau=1,visc=1,agrid=1,dens=1)
system.actors.add(lbf)
for shape in pores:
    lbb = lbboundaries.LBBoundary(shape = shape)
    system.lbboundaries.add(lbb)

def up_vis():
    while True:
        visualizer.update()
        time.sleep(1e-3)
        
visualizer = visualization.openGLLive(system,LB_draw_node_boundaries = True)
t = Thread(target=up_vis)
t.daemon = True
t.start()
visualizer.start()
