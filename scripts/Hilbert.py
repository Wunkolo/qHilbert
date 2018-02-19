from PIL import Image
from PIL import ImageDraw

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
	PointCount = Width ** 2
	for i,Line in enumerate(zip(*[Points[Off:] for Off in range(Quantum + 1)])):
		# list( zip(*[a[Off:] for Off in range(0,Quantum + 1)]) )
		Scaled = ( (Line[0][0] * Spacing, 
					Line[0][1] * Spacing),
				   (Line[1][0] * Spacing,
					Line[1][1] * Spacing))
		draw.line(
			Scaled,
			fill=i
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
		).save(Path + "Hilbert" + str(i).zfill(6) + ".png")
	del draw

GenHilbertFrames({})

#ffmpeg -f image2 -framerate 50 -i Hilbert%6d.png Hilbert.gif -t 4
