#!/usr/bin/env python

import math, random, sys

totalz = 0.0
for i in range(int(sys.argv[1])):
  r = random.random()
  theta = 2*math.pi*random.random()
  x = r*math.cos(theta)
  y = r*math.sin(theta)
  sigma = 0.5
  z = 1.0/(2.0*math.pi*sigma**2.0)*math.e**(-(x**2.0+y**2.0)/(2.0*sigma**2.0))
  print("vec3(%6.3f, %6.3f, %.3f)"%(x, y, z))
  totalz += z

print("scale factor: %.5f" % (1.0/totalz,))

