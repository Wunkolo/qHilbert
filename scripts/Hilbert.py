from PIL import Image
from PIL import ImageDraw
import random
from itertools import tee

def qHilbertSOA(Width, Distances):
	Level = 1
	PositionsX = [0] * len(Distances)
	PositionsY = [0] * len(Distances)
	CurDistances = Distances
	for i in range(Width.bit_length() - 1):
		# Determine Regions
		RegionsX = [0b1 & (Dist // 2) for Dist in CurDistances]
		RegionsY = [0b1 & (Dist ^ RegionsX[i]) for i,Dist in enumerate(CurDistances)]
		# Flip if RegX == 0 RegY==1
		PositionsX = [
			(Level-1 - PosX) if RegionsX[i] == 1 and RegionsY[i] == 0 else PosX \
			for i,PosX in enumerate(PositionsX)
		]
		PositionsY = [
			(Level-1 - PosY) if RegionsX[i] == 1 and RegionsY[i] == 0 else PosY \
			for i,PosY in enumerate(PositionsY)
		]
		# Swap X and Y if RegX == 0
		PositionsX,PositionsY = zip(
			*[ reversed(ele) if RegionsY[i] == 0 else ele for i,ele in enumerate(zip(PositionsX,PositionsY))]\
		)
		PositionsX = [ PosX + Level * RegionsX[i] for i,PosX in enumerate(PositionsX)]
		PositionsY = [ PosY + Level * RegionsY[i] for i,PosY in enumerate(PositionsY)]
		CurDistances = [ Dist // 4 for Dist in CurDistances ]
		Level *= 2
	return list(zip(PositionsX,PositionsY))

def groups(iterable,n):
	return zip(*[iter(iterable)]*n)

# Config
# Name: Name of generated frames. Default "Hilbert"
# Path: Path for output images Default: "."
# Width: Must be power of 2. Default: 32
# Spacing: Determines the "Width" of the cell (use odd numbers). Default: 3
# Scale: Scaling for the resulting image. Default: 2
# Quantum: Number of lines drawn per-frame. Default: 1
def GenHilbertFrames(Params):
	Name = Params.get("Name", "Hilbert")
	Width = Params.get("Width",32)
	Path = Params.get("Path",".") + "/"
	Spacing = Params.get("Spacing",3)
	Scale = Params.get("Scale",2)
	Quantum = Params.get("Quantum",1)

	img = Image.new('RGB',(Width*Spacing,Width*Spacing))
	Points = qHilbertSOA(Width,list(range(0,Width**2)))
	draw = ImageDraw.Draw(img)
	PointCount = (Width ** 2) // (Quantum + 1)
	# Iterate n-wise groups of points to draw lines between them all
	# this is a moving window! fix!
	# should have 1 element of overlap?
	for i,Edges in enumerate(groups(Points,Quantum+1)):#enumerate(zip(*[ Points[Off:] for Off in range(Quantum + 1) ])):
		ScaledPoints = [ (Point[0] * Spacing, Point[1] * Spacing) for Point in Edges ]
		draw.line(
			ScaledPoints,
			fill = ( i, abs((i & 0xFF) - 0x7F), 0xFF - (i & 0xFF) )
		)
		draw.point(
			ScaledPoints,
			fill = ( 0xFF, 0x00, 0x00 )
		)

		print(
			"%6d/%6d %02.02f%%\r" % (i,PointCount,100*(i/PointCount)),
			end=''
		)
		# save every frame
		# scale it x2
		img.resize(
			(img.width * Scale,img.height * Scale),
			Image.NEAREST
		).save(Path + Name + str(i).zfill(6) + ".png")
	del draw

GenHilbertFrames(
	{
		"Quantum": 2,
		"Scale": 6
	}
)

#ffmpeg -f image2 -framerate 50 -i Hilbert%6d.png Hilbert.gif -t 4
