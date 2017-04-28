Track

Generate raceline: ~/torcs_bin/bin/trackgen -n wheel-2 -c road -r (afterwards manually edited because of manual changes in wheel-2.ac)
Generate shading: ~/torcs_bin/bin/accc +shad wheel-2-src.ac wheel-2-shad.ac (shading file was manually edited, rgb->png, remapped bridge top)
Generate acc: ~/torcs_bin/bin/accc -g wheel-2.acc -l0 wheel-2-src.ac -l1 wheel-2-shad.ac -l2 wheel-2-trk-raceline.ac -d3 1000 -d2 500 -d1 300 -S 300 -es 

--
Copyright (C) 2005 Andrew Sumner
Copyright (C) 2007 Andrew Sumner (fix 3D model errors, new textures)
Copyright (C) 2013 Bernhard Wymann (added raceline, fixes)

Copyleft: this work of art is free, you can redistribute
it and/or modify it according to terms of the Free Art license.
You will find a specimen of this license on the site
Copyleft Attitude http://artlibre.org as well as on other sites.
