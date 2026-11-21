"""Disco — boîtier paramétrique, mm. Python 3 + cadquery 2.7.
Exécuter : python3 generer.py. Les STL sont orientés pour l'impression.
Origine carte : angle avant gauche, antenne à l'avant (Y négatif).
Cotes issues de photos : prototype, valider le gabarit avant impression.
"""
from pathlib import Path
import cadquery as cq
import json
OUT=Path(__file__).resolve().parent
# PARAMÈTRES : vérifier ces cotes sur la carte réelle.
PCB_X,PCB_Y=83.0,68.0
PCB_THICKNESS=1.6
CLEARANCE=0.7                 # jeu par côté autour du circuit
WALL=2.0
FLOOR=2.0
PCB_Z=6.0                     # face inférieure carte, 4 mm au-dessus du fond
BASE_H=20.0
LID_THICKNESS=2.0
HOLE_INSET_X,HOLE_INSET_Y=3.5,3.5 # estimés depuis les photos, non mesurés
PIN_D=1.8                     # volontairement fin, trou carte supposé >= 3 mm
PIN_H=2.0
POST_R=2.7
ANT_X0,ANT_X1=30.0,56.0
ANT_NOTCH_DEPTH=8.0           # découpe ouverte du fond ET du couvercle
JACK_Y0,JACK_Y1=5.0,22.0
PORT_Z=5.2                   # fenêtres ouvertes jusqu'au couvercle
RIGHT_Y0,RIGHT_Y1=6.0,54.0     # SD + deux USB, grande fenêtre commune
FIT=0.25                      # jeu du clip avec paroi
CLIP_T=1.2
CLIP_W=12.0
CLIP_BOTTOM=10.0
BEAD_Z=11.5
BEAD_PROJECTION=0.5           # engagement nominal 0.25 mm
CLIP_CENTERS=(17.0,67.0)

def box(x0,y0,z0,x1,y1,z1):
 return cq.Workplane('XY').box(x1-x0,y1-y0,z1-z0,centered=(False,False,False)).translate((x0,y0,z0))
def cyl(x,y,z,r,h):
 return cq.Workplane('XY').circle(r).extrude(h).translate((x,y,z))
x0=y0=-CLEARANCE-WALL
x1=PCB_X+CLEARANCE+WALL;y1=PCB_Y+CLEARANCE+WALL
holes=[(x,y) for x in (HOLE_INSET_X,PCB_X-HOLE_INSET_X) for y in (HOLE_INSET_Y,PCB_Y-HOLE_INSET_Y)]
ant=box(ANT_X0,y0-1,-1,ANT_X1,ANT_NOTCH_DEPTH,BASE_H+LID_THICKNESS+1)
base=box(x0,y0,0,x1,y1,BASE_H).edges('|Z').fillet(2.5)
base=base.cut(box(-CLEARANCE,-CLEARANCE,FLOOR,PCB_X+CLEARANCE,PCB_Y+CLEARANCE,BASE_H+1))
base=base.cut(ant)
base=base.cut(box(x0-1,JACK_Y0,PORT_Z,0,JACK_Y1,BASE_H+1))
base=base.cut(box(PCB_X,RIGHT_Y0,PORT_Z,x1+1,RIGHT_Y1,BASE_H+1))
for x,y in holes:
 base=base.union(cyl(x,y,FLOOR,POST_R,PCB_Z-FLOOR))
 # tapered pin tip for easy insertion, no force fit into the PCB
 base=base.union(cyl(x,y,PCB_Z,PIN_D/2,PIN_H-.5))
 tip=cq.Solid.makeCone(PIN_D/2,.5,.5,cq.Vector(x,y,PCB_Z+PIN_H-.5))
 base=base.union(cq.Workplane(obj=tip))
# Small internal catches, only 0.5 mm deep. Clips release by flexing inward.
for x in CLIP_CENTERS:
 base=base.cut(box(x-CLIP_W/2-.6,-CLEARANCE-.6,BEAD_Z-.3,x+CLIP_W/2+.6,-CLEARANCE+.1,BEAD_Z+1.9))
 base=base.cut(box(x-CLIP_W/2-.6,PCB_Y+CLEARANCE-.1,BEAD_Z-.3,x+CLIP_W/2+.6,PCB_Y+CLEARANCE+.6,BEAD_Z+1.9))
# Fingernail recess at the back, no tool required.
base=base.cut(box(PCB_X/2-6,y1-1,BASE_H-1.2,PCB_X/2+6,y1+1,BASE_H+1))
lid=box(x0,y0,BASE_H,x1,y1,BASE_H+LID_THICKNESS).edges('|Z').fillet(2.5).cut(ant)
for x in CLIP_CENTERS:
 for front in (True,False):
  face=-CLEARANCE+FIT if front else PCB_Y+CLEARANCE-FIT
  a,b=(face,face+CLIP_T) if front else (face-CLIP_T,face)
  lid=lid.union(box(x-CLIP_W/2,a,CLIP_BOTTOM,x+CLIP_W/2,b,BASE_H+.1))
  # Symmetric 45-ish degree ramps: no horizontal unsupported hook.
  outside=face-BEAD_PROJECTION if front else face+BEAD_PROJECTION
  pts=[(face,BEAD_Z),(outside,BEAD_Z+.8),(face,BEAD_Z+1.6)]
  bead=cq.Workplane('YZ').polyline(pts).close().extrude(CLIP_W).translate((x-CLIP_W/2,0,0))
  lid=lid.union(bead)
# Four lid stops keep the board captive without pressing on components.
for x,y in holes:
 stop=cyl(x,y,PCB_Z+PCB_THICKNESS+.3,2.7,BASE_H-(PCB_Z+PCB_THICKNESS+.3)+.1)
 stop=stop.cut(cyl(x,y,PCB_Z+PCB_THICKNESS+.2,1.3,1.5))
 lid=lid.union(stop)
# Narrow roof slots ventilate while remaining printable flat without supports.
for y in range(22,54,6):
 lid=lid.cut(box(25,y,BASE_H-.1,58,y+2,BASE_H+LID_THICKNESS+.1))
# Print lid exterior face down, clips and stops upwards.
lid_print=lid.rotate((0,0,0),(1,0,0),180).translate((0,PCB_Y,BASE_H+LID_THICKNESS))
# Low-material fitting jig: same hole locations; board bottom at 6 mm.
jig=box(x0,y0,0,x1,y1,1.2).edges('|Z').fillet(2.5)
jig=jig.cut(box(7,7,-1,PCB_X-7,PCB_Y-7,2)).cut(ant)
for x,y in holes:
 jig=jig.union(cyl(x,y,1.2,POST_R,PCB_Z-1.2)).union(cyl(x,y,PCB_Z,PIN_D/2,PIN_H))
parts={'01_Fond':base,'02_Couvercle_face_exterieure_sur_plateau':lid_print,'00_Gabarit_carte':jig}
report={}
for name,part in parts.items():
 shape=part.val()
 assert shape.isValid() and len(shape.Solids())==1, name
 cq.exporters.export(part,str(OUT/(name+'.stl')),tolerance=.05,angularTolerance=.15)
 bb=shape.BoundingBox()
 report[name]={'valid':shape.isValid(),'solids':len(shape.Solids()),'volume_mm3':round(shape.Volume(),1),'dimensions_mm':[round(bb.xlen,2),round(bb.ylen,2),round(bb.zlen,2)]}
assert base.intersect(lid).val().Volume()<.01,'collision fond/couvercle'
# STEP uses assembled coordinates for CAD editing.
assembly=cq.Assembly(name='Disco_boîtier_V1')
assembly.add(base,name='Fond',color=cq.Color(.28,.13,.42))
assembly.add(lid,name='Couvercle',color=cq.Color(.85,.68,.31))
assembly.save(str(OUT/'Disco_Boitier_assemblage.step'))
(OUT/'verification_geometrique.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
